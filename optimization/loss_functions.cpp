//
// Created by janos on 20.04.20.

#include "loss_functions.hpp"

#include <Corrade/Utility/StlMath.h>
#include <utility>
#include <new>
#include <adolc/adouble.h>

template<class T>
LossFunction::LossFunction(T f){
    erased = ::operator new(sizeof(T), std::align_val_t(alignof(T)));
    ::new(erased) T(std::move(f));
    ad = +[](void* e, adouble const& x, adouble& y){
        (*static_cast<T*>(e))(x, y);
    };
    loss = +[](void* e, double x, double ys[3]){
        (*static_cast<T*>(e))(x, ys);
    };
    destroy = +[](void* e){
        static_cast<T*>(e)->~T();
        ::operator delete(e, sizeof(T), std::align_val_t(alignof(T)));
    };
}

LossFunction::~LossFunction(){
    if(erased) {
        destroy(erased);
    }
}

LossFunction::LossFunction(LossFunction&& other) noexcept{
    using std::swap;
    swap(*this, other);
}


LossFunction& LossFunction::operator=(LossFunction&& other) noexcept{
    using std::swap;
    swap(*this, other);
    return *this;
}

void swap(LossFunction& f1, LossFunction& f2){
    using std::swap;
    swap(f1.ad, f2.ad);
    swap(f1.loss, f2.loss);
    swap(f1.destroy, f2.destroy);
    swap(f1.erased, f2.erased);
}

void LossFunction::operator()(double const& in, double out[3]){
    loss(erased, in, out);
}
void LossFunction::operator()(adouble const& x, adouble& y){
    ad(erased, x, y);
}

void TrivialLoss::operator()(double const& in, double out[3]){
    out[0] = in;
    out[1] = 1.;
    out[2] = 0.;
}

void QuadraticLoss::operator()(double const& in, double out[3]){
    out[0] = in * in;
    out[1] = 2 * in;
    out[2] = 2.;
}

void CauchyLoss::operator()(double const& in, double out[3]){
    double sum = in + 1.;
    double inv = 1./sum;
    out[0] = log(sum);
    out[1] = inv;
    out[2] = -1. * inv * inv;
}

ScaledLoss::ScaledLoss(LossFunction f_, double s_):
    f(std::move(f_)),
    s(s_)
{
}

void ScaledLoss::operator()(double const& in, double out[3]){
    double fout[3];
    f(in, fout);
    out[0] = s * fout[0];
    out[1] = s * fout[1];
    out[2] = s * fout[2];
}

ComposedLoss::ComposedLoss(LossFunction f_, LossFunction g_):
    g(std::move(g_)),
    f(std::move(f_))
{
}

void ComposedLoss::operator()(double const& in, double out[3]){
    double fout[3];
    double gout[3];
    f(in, fout);
    g(fout[0], gout);
    out[0] = gout[0];
    out[1] = gout[1] * fout[1];
    out[2] = gout[2] * fout[1] * fout[1] + gout[1] * fout[2];
}

/* explicit instantiations */
template LossFunction::LossFunction(TrivialLoss);
template LossFunction::LossFunction(ScaledLoss);
template LossFunction::LossFunction(CauchyLoss);
template LossFunction::LossFunction(QuadraticLoss);
template LossFunction::LossFunction(ComposedLoss);
