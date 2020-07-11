//
// Created by janos on 6/25/20.
//


#include "functional.h"

#include <Magnum/Math/Functions.h>

#include <adolc/adouble.h>
#include <adolc/taping.h>
#include <adolc/adolc_sparse.h>

#include <imgui.h>
#include <set>

using namespace Magnum;

static std::set<int> usedTags;

void Functional::initTag(int& tag) {
    tag = -1;
    auto it = usedTags.begin();
    while(it != usedTags.end()){
        if(*it > tag + 1){
            usedTags.insert(++tag);
            return;
        }
        tag = *it;
    }
    usedTags.insert(++tag);
}


Functional::~Functional(){
    if(erased){
        destruct(erased);
        usedTags.erase(tag);
    }
}

void Functional::operator()(
        double* parameter,
        double* residuals,
        SparseMatrix* g,
        SparseMatrix* h) const {

    if(evalWithHessian == nullptr ||  && alwaysRetape || isFirstEvaluation){
        isFirstEvaluation = false;
        tapeEvaluation(parameter);
    }

    std::size_t m = numParameters();
    std::size_t n = numResiduals();

    gradient(tag, n, x, grad_f);
    sparse_jac(tag, m, n, 1, x, &nnz_jac, &rind_g, &cind_g, &jacval, options_g);


    /* apply scaling and loss function */
    for (std::size_t i = 0; i < m; ++i) {
        Mg::Double rho[3], phi[3] = {residuals[i], 1., 0.};
        if(scaling){
            auto s = *scaling;
            for (auto& r : phi) r *= s;
        }

        loss->Evaluate(phi[0], rho);

        if(residuals){
            residuals[i] = rho[1];
        }

        if (jac) {
            double lossGrad = rho[1] * phi[1];
            for(auto& x : jac->row(i))
                x *= lossGrad;
        }

        if(hess){
            double lossHess = rho[2] * phi[1] * phi[1] + rho[1] * phi[2];
            for (std::size_t j = 0; j < hess->size(); ++j) {
                hess->values[j] = lossHess *
            }
        }
    }
}

void Functional::tapeEvaluation(double const* x) const {
    double dummy;
    trace_on(tag);
    Containers::Array<adouble> xs(numParameters());
    Containers::Array<adouble> ys(numResiduals());
    for (uint32_t i = 0; i < xs.size(); ++i)
        xs[i] <<= x[i];
    ad(erased, xs.data(), ys.data());
    for (uint32_t i = 0; i < ys.size(); ++i){
        if(scaling)
            ys[i] *= *scaling;

        ys[i] = loss->evaluate(ys[i])
        ys[i] >>= dummy;
    }
    trace_off();
}

bool Functional::drawImGuiOptions(){
    if(options){
        ImGui::Checkbox("Check Derivatives", &checkDerivatives);
        return options(erased);
    }
    return false;
}
