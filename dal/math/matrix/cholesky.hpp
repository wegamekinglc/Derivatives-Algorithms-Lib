//
// Created by wegamekinglc on 22-12-17.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {
    class SquareMatrixDecomposition_;

    SquareMatrixDecomposition_* CholeskyDecomposition(const SquareMatrix_<>& src);
    void CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization = Dal::EPSILON);
}
