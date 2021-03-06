//
// Created by janos on 13.03.20.
//

#include "Upload.h"

#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Containers/Optional.h>

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Sampler.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Duplicate.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/MeshTools/GenerateFlatNormals.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Color.h>
#include <Magnum/MeshTools/GenerateNormals.h>
#include <Magnum/Shaders/Generic.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Trade/MeshData.h>

using namespace Magnum;
using namespace Corrade;


void upload(GL::Mesh& mesh, GL::Buffer& vertices, GL::Buffer& indices, Trade::MeshData& meshData) {

    vertices.setData(meshData.vertexData());
    indices.setData(meshData.indexData());

    CORRADE_ASSERT((!meshData.isIndexed() || indices.id()) && vertices.id(),
                   "upload: invalid external buffer(s)",);

    /* Basics */
    mesh.setPrimitive(meshData.primitive());

    /* Vertex data */
    GL::Buffer verticesRef = GL::Buffer::wrap(vertices.id(), GL::Buffer::TargetHint::Array);
    GL::Buffer indicesRef = GL::Buffer::wrap(indices.id(), GL::Buffer::TargetHint::Array);
    for(UnsignedInt i = 0; i != meshData.attributeCount(); ++i) {
        Containers::Optional<GL::DynamicAttribute> attribute;

        /* Ignore implementation-specific formats because GL needs three
           separate values to describe them so there's no way to put them in a
           single 32-bit value :( */
        const VertexFormat format = meshData.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            Warning{} << "upload: ignoring attribute" << meshData.attributeName(i) << "with an implementation-specific format" << reinterpret_cast<void*>(vertexFormatUnwrap(format));
            continue;
        }

        switch(meshData.attributeName(i)) {
            case Trade::MeshAttribute::Position:
                /* Pick 3D position always, the format will properly reduce it
                   to a 2-component version if needed */
                attribute.emplace(Shaders::Generic3D::Position{}, format);
                break;
            case Trade::MeshAttribute::TextureCoordinates:
                /** @todo have Generic2D derived from Generic that has all
                    attribute definitions common for 2D and 3D */
                attribute.emplace(Shaders::Generic2D::TextureCoordinates{}, format);
                break;
            case Trade::MeshAttribute::Color:
                /** @todo have Generic2D derived from Generic that has all
                    attribute definitions common for 2D and 3D */
                /* Pick Color4 always, the format will properly reduce it to a
                   3-component version if needed */
                attribute.emplace(Shaders::Generic2D::Color4{}, format);
                break;
            case Trade::MeshAttribute::Tangent:
                /* Pick Tangent4 always, the format will properly reduce it to
                   a 3-component version if needed */
                attribute.emplace(Shaders::Generic3D::Tangent4{}, format);
                break;
            case Trade::MeshAttribute::Bitangent:
                attribute.emplace(Shaders::Generic3D::Bitangent{}, format);
                break;
            case Trade::MeshAttribute::Normal:
                attribute.emplace(Shaders::Generic3D::Normal{}, format);
                break;

                /* So it doesn't yell that we didn't handle a known attribute */
            case Trade::MeshAttribute::Custom: break; /* LCOV_EXCL_LINE */
        }

        if(!attribute) {
            Warning{} << "upload: ignoring unknown attribute" << meshData.attributeName(i);
            continue;
        }

        mesh.addVertexBuffer(verticesRef,
                             meshData.attributeOffset(i), meshData.attributeStride(i),
                             *attribute);

    }

    if(meshData.isIndexed()) {
        mesh.setIndexBuffer(indicesRef, 0, meshData.indexType())
                .setCount(meshData.indexCount());
    } else mesh.setCount(meshData.vertexCount());

}


Trade::MeshData preprocess(
        Containers::ArrayView<Mg::Vector3d const> const& vertices,
        Containers::ArrayView<Mg::UnsignedInt const> const& indices,
        CompileFlags flags)
{
    Containers::Array<Trade::MeshAttributeData> extra;
    Containers::arrayAppend(extra, Containers::InPlaceInit, Trade::MeshAttribute::Position, VertexFormat::Vector3, nullptr);

    using L = std::initializer_list<std::tuple<CompileFlags, VertexFormat, Trade::MeshAttribute>>;
    for(auto&& [flag, format, attribute] : L{
        {CompileFlag::AddNormalAttribute|CompileFlag::GenerateFlatNormals|CompileFlag::GenerateSmoothNormals, VertexFormat::Vector3, Trade::MeshAttribute::Normal},
        {CompileFlag::AddColorAttribute, VertexFormat::Vector4, Trade::MeshAttribute::Color},
        {CompileFlag::AddTextureCoordinates, VertexFormat::Vector2, Trade::MeshAttribute::TextureCoordinates}})
    {
        if (flags & flag)
            Containers::arrayAppend(extra, Trade::MeshAttributeData{attribute, format, nullptr});
    }

    /* If we want flat normals, we need to first duplicate everything using
       the index buffer. Otherwise just interleave the potential extra
       attributes in. */
    Trade::MeshIndexData indexData{indices};
    Trade::MeshData meshData(MeshPrimitive::Triangles,
            {}, indices, indexData, vertices.size());

    Trade::MeshData generated{MeshPrimitive::Points, 0};
    if (flags & CompileFlag::GenerateFlatNormals){
        generated = MeshTools::duplicate(meshData, extra);
        auto positionsView = generated.mutableAttribute<Vector3>(Trade::MeshAttribute::Position);
        for (int i = 0; i < indices.size(); ++i)
            positionsView[i] = Vector3{vertices[indices[i]]};
    } else {
        generated = MeshTools::interleave(meshData, extra);
        auto positionsView = generated.mutableAttribute<Vector3>(Trade::MeshAttribute::Position);
        for (int i = 0; i < positionsView.size(); ++i)
            positionsView[i] = Vector3{vertices[i]};
    }

    /* Generate the normals. @todo what if mesh is not indexed?*/
    if (flags & CompileFlag::GenerateFlatNormals)
        MeshTools::generateFlatNormalsInto(
                generated.attribute<Vector3>(Trade::MeshAttribute::Position),
                generated.mutableAttribute<Vector3>(Trade::MeshAttribute::Normal));
    else if(flags & CompileFlag::GenerateSmoothNormals)
        MeshTools::generateSmoothNormalsInto(generated.indices(),
                                             generated.attribute<Vector3>(Trade::MeshAttribute::Position),
                                             generated.mutableAttribute<Vector3>(Trade::MeshAttribute::Normal));

    return generated;
}

//void makeScene(Viewer& scene){
//    upload(phasefieldData, phasefieldData.meshData);
//    phasefieldData.drawable = new ColorMapPhongDrawable(viewer.scene, pd.shaders[DrawableType::ColorMapPhong], viewer.drawableGroup)
//    phasefieldData.type = DrawableType::ColorMapPhong;
//}

void reuploadVertices(GL::Buffer& vertices, Trade::MeshData const& meshData)
{
    auto data = vertices.map(0,
                             vertices.size(),
                             GL::Buffer::MapFlag::Write);

    CORRADE_CONSTEXPR_ASSERT(data, "could not map vertex data");
    Utility::copy(meshData.vertexData(), data);
    vertices.unmap();
}


