//
// Created by wegam on 2022/12/17.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {
    class SquareMatrixDecomposition_;
    class SymmetricMatrixDecomposition_;

    SymmetricMatrixDecomposition_* DiagonalAsDecomposition(const Vector_<>& diag);
    SquareMatrixDecomposition_* LowerTriangularAsDecomposition(const SquareMatrix_<>& src);
}
