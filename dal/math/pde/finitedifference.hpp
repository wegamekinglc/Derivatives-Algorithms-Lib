//
// Created by wegam on 2023/2/25.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal::PDE {
    class FiniteDifference_ {
    public:
        static void Dx(int wind, const Vector_<>& x, Matrix_<>& dx);
        static void Dxx(const Vector_<>& x, Matrix_<>& dxx);
    };
}
