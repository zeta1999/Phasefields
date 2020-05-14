//
// Created by janos on 02.05.20.
//

#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/SceneGraph/SceneGraph.h>
#include <Corrade/Containers/EnumSet.h>


using Object3D = Magnum::SceneGraph::Object<Magnum::SceneGraph::MatrixTransformation3D>;
using Scene3D = Magnum::SceneGraph::Scene<Magnum::SceneGraph::MatrixTransformation3D>;
using Drawable = Magnum::SceneGraph::Drawable3D;
using DrawableGroup = Magnum::SceneGraph::DrawableGroup3D;

enum class ColorMapType : Magnum::UnsignedInt {
    Turbo = 0,
    Magma = 1,
    Plasma = 2,
    Inferno = 3,
    Viridis = 4
};

enum class ShaderType : Magnum::UnsignedInt {
    FlatTextured = 0,
    PhongDiffuse = 2,
    MeshVisualizer = 3,
    MeshVisualizerPrimitiveId = 4,
    VertexColor = 8
};


enum class VisualizationFlag : Magnum::UnsignedInt {
    Phasefield = 1u << 0u,
    GeodesicWeights = 1u << 1u,
    ConnectedComponents = 1u << 2u,
    Gradient = 1u << 3u,
};

using VisualizationFlags = Corrade::Containers::EnumSet<VisualizationFlag>;
CORRADE_ENUMSET_OPERATORS(VisualizationFlags)

