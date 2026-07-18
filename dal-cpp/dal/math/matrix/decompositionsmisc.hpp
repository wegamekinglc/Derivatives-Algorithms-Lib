//
// Created by wegam on 2022/12/17.
//

#pragma once

#include <memory>

namespace Dal {
    class SquareMatrixDecomposition_;
    class SymmetricMatrixDecomposition_;

    std::unique_ptr<SymmetricMatrixDecomposition_> DiagonalAsDecomposition(const Vector_<>& diag);
    std::unique_ptr<SquareMatrixDecomposition_> LowerTriangularAsDecomposition(const SquareMatrix_<>& src);
} // namespace Dal
