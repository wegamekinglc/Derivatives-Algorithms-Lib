//
// Created by wegamekinglc on 22-12-17.
//

#pragma once

#include <dal/math/matrix/sparse.hpp>

namespace Dal {
    class SquareMatrixDecomposition_;

    Sparse::SymmetricDecomposition_* CholeskyDecomposition(const SquareMatrix_<>& src);
    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization = Dal::EPSILON);
}
