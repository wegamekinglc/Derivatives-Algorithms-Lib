//
// Created by dal-implementer on 2026/7/8.
//

#pragma once

#include <dal/math/matrix/banded.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::PDE {
    Sparse::TriDiagonal_* NewDx(const Vector_<>& x);
    Sparse::TriDiagonal_* NewDxx(const Vector_<>& x);
} // namespace Dal::PDE
