#pragma once

#include <gtest/gtest.h>

#include <dal/math/matrix/squarematrix.hpp>

namespace Dal::Test {
    struct TransposeRefinementCase_ {
        SquareMatrix_<> matrix_{3, 0.};
        Matrix_<> rhs_, seeds_;
        Vector_<> solution_, lambda_;

        TransposeRefinementCase_(double coupling, int columns) : rhs_(3, columns), seeds_(3, columns, 0.) {
            matrix_(0, 0) = .49977023936292558;
            matrix_(1, 0) = .67076481424944678;
            matrix_(1, 1) = .24986618787781789;
            matrix_(2, 0) = .25;
            matrix_(2, 1) = .45;
            matrix_(2, 2) = .3;
            matrix_(0, 2) = coupling;
            const double denominator = matrix_(0, 0) + coupling / .3 * (matrix_(1, 0) * .45 / matrix_(1, 1) - .25);
            const double x0 = (1. - coupling / .3 * (3. - .45 / matrix_(1, 1) * 2.)) / denominator;
            const double x1 = (2. - matrix_(1, 0) * x0) / matrix_(1, 1);
            solution_ = {x0, x1, (3. - .25 * x0 - .45 * x1) / .3};
            const double lambda0 = .7 / denominator;
            lambda_ = {lambda0, coupling / .3 * .45 / matrix_(1, 1) * lambda0, -coupling / .3 * lambda0};
            for (int column = 0; column < columns; ++column) {
                for (int row = 0; row < 3; ++row)
                    rhs_(row, column) = (column == 0 ? 1. : -2.) * (row + 1.);
                seeds_(0, column) = column == 0 ? .7 : -.35;
            }
        }

        [[nodiscard]] double RhsAdjoint(int row, int column) const { return (column == 0 ? 1. : -.5) * lambda_[row]; }
        [[nodiscard]] double MatrixAdjoint(int row, int column) const { return -(rhs_.Cols() == 1 ? 1. : 2.) * lambda_[row] * solution_[column]; }
    };

    inline void AssertTransposeValue(double actual, double expected) {
        if (expected == 0.)
            ASSERT_DOUBLE_EQ(actual, 0.);
        else
            ASSERT_NEAR(actual / expected, 1., 1e-12);
    }
} // namespace Dal::Test
