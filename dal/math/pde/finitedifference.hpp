//
// Created by wegam on 2023/2/25.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal::PDE {
    Sparse::TriDiagonal_* Dx(const Vector_<>& x);
    Sparse::TriDiagonal_* Dxx(const Vector_<>& x);
    Vector_<> BCx(const Vector_<>& x);
    Vector_<> BCxx(const Vector_<>& x);
}
