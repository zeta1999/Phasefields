//
// Created by janos on 7/16/20.
//

#include "tree_functional.h"

#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/Math/Vector3.h>

using namespace Magnum;

RecursiveFunctional::RecursiveFunctional(Containers::Array<const Mg::Vector3d> const& vs, Containers::Array<const Mg::UnsignedInt> const& is, PhasefieldTree& t) :
        vertices(vs),
        indices(is),
        tree(t)
{
    updateInternalDataStructures();
}

Mg::UnsignedInt RecursiveFunctional::numParameters() const {
    return tree.phasefieldData.size();
}

Mg::UnsignedInt RecursiveFunctional::numConstraints() const {
    return tree.numLeafs * 2;
}

void RecursiveFunctional::updateInternalDataStructures(){
    weights = computeIntegralOperator(triangles(), vertices);
}

void RecursiveFunctional::getSparsityStructure(SparseMatrix& jacobian) const {
    jacobian.clear();
    jacobian.numCols = numParameters();
    jacobian.numRows = numConstraints() * tree.numLeafs;

    auto m = numConstraints();
    auto n = numParameters();
    UnsignedInt row = 0;
    tree.traverse([&](PhasefieldNode const& leaf){
        if(leaf.isLeaf()){
            PhasefieldNode const* node = &leaf;
            while(1){
                for(UnsignedInt j = 0; j < m; ++j, ++row){
                    for(UnsignedInt i = 0; i < n; ++i){
                        Containers::arrayAppend(jacobian.rows, row);
                        auto col = node->idx * n + i;
                        Containers::arrayAppend(jacobian.cols, col);
                    }
                }
                if(node->parent != PhasefieldNode::None){
                    break;
                } else {
                    node = &tree.nodes[node->parent];
                }
            }
        }
    });
    jacobian.nnz = jacobian.rows.size();
    Containers::arrayResize(jacobian.values, jacobian.nnz);
}

void RecursiveFunctional::operator()(
        Containers::ArrayView<const double> data,
        double& cost,
        Containers::ArrayView<double> gradData,
        Containers::ArrayView<double> constr,
        SparseMatrix* jacobian) const {

    SmootherStep smoothStep;


    auto& nodes = tree.nodes;
    auto n = tree.phasefieldSize;
    auto m = numConstraints();
    auto size = nodes.size();

    //auto gradients = tree.gradients();
    auto& jacValues = jacobian->values;
    auto prefixes = tree.temps();
    auto phasefields = tree.phasefields();
    Containers::StridedArrayView2D<double> gradients{{gradData, size*n}, {size, n}};

    cost = 0.;
    for(auto& x : prefixes[0]) x = 1.;

    int jacIdx = 0;
    tree.traverse([&](PhasefieldNode& node) -> void {
        if(!node.isLeaf()) {
            for (UnsignedInt i = 0; i < n; ++i) {
                if(node.leftChild != PhasefieldNode::None){
                    double pos = smoothStep.eval(
                            phasefields[node.leftChild][i]);
                    phasefields[node.leftChild][i] = pos * phasefields[node.idx][i];
                }
                if(node.rightChild != PhasefieldNode::None){
                    double neg = smoothStep.eval(
                            -phasefields[node.leftChild][i]);
                    phasefields[node.rightChild][i] = neg * phasefields[node.idx][i];
                }
            }
        } else {
            PhasefieldNode const& leaf = node;

            Containers::Array<double> productsGrad(Containers::ValueInit, n);
            Containers::Array<double> productsJacData(Containers::ValueInit, n);
            Containers::StridedArrayView2D<double> productsJac(productsJacData, {m, n});

            // @TODO we now assume everything zeroed and we only have to add everything ontop

            for(Functional const& functional : objectives) {
                functional(phasefields[node.idx].asContiguous(), prefixes[node.idx].asContiguous(), cost, gradients[leaf.idx].asContiguous(), productsGrad);
            }

            for(std::size_t i = 0; i < constraints.size(); ++i) {
                auto jacSlice = jacValues.slice(jacIdx, jacIdx + n);
                constraints[i](phasefields[node.idx].asContiguous(), prefixes[node.idx].asContiguous(), cost, jacSlice, productsJac[i].asContiguous());
                jacIdx += n;
            }

            /* The last prefix array is unused, so we use it to accumulate partial products*/
            auto postfix = prefixes[leaf.idx];
            for(auto& x : postfix) x = 1.;

            PhasefieldNode const* node1 = &leaf;
            while(1){
                auto idx = node1->idx;
                double sign = tree.isLeftChild(*node1);
                for(UnsignedInt i = 0; i < n; ++i){
                    gradients[idx][i] += prefixes[idx][i] * smoothStep.grad(sign * phasefields[idx][i]) * postfix[i] * productsGrad[i];
                    postfix[i] *= phasefields[idx][i];
                }

                for(std::size_t j = 0; j < m; ++j) {
                    for(UnsignedInt i = 0; i < n; ++i){
                        jacValues[jacIdx++] += prefixes[idx][i] * smoothStep.grad(sign * phasefields[idx][i]) * postfix[i] * productsJac[j][i];
                    }
                }

                if(node1->parent != PhasefieldNode::None) {
                    break;
                } else {
                    node1 = &tree.nodes[node1->parent];
                }
            }
        }
    });
}


