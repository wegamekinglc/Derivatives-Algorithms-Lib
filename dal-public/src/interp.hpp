//
// Created by wegam on 2022/5/9.
//

#pragma once

#include <dal-public/src/types.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interp2d.hpp>

namespace Dal {
    Handle_<Interp1_> Interp1NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& y);
    Handle_<Interp1_>
    Interp1NewLinearSmoothed(const String_& name, const Vector_<>& x, const Vector_<>& y, double smoothing, const Vector_<>& fitWeights = {});
    Handle_<Interp1_> Interp1NewCubic(
        const String_& name, const Vector_<>& x, const Vector_<>& y, const Vector_<int>& boundaryOrder = {}, const Vector_<>& boundaryValue = {});
    Vector_<> Interp1Get(const Handle_<Interp1_>& interpolator, const Vector_<>& x);
    void Interp1Get(const Handle_<Interp1_>& interpolator, const Vector_<>& x, Vector_<>* values);
    Handle_<Interp2_> Interp2NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& values);
    Matrix_<> Interp2Get(const Handle_<Interp2_>& interpolator, const Vector_<>& x, const Vector_<>& y);
    void Interp2Get(const Handle_<Interp2_>& interpolator, const Vector_<>& x, const Vector_<>& y, Matrix_<>* values);
} // namespace Dal
