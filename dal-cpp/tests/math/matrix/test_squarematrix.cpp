//
// Created by Claude on 2026/9/28.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/matrix/squarematrix.hpp>

using Dal::SquareMatrix_;

TEST(SquareMatrixTest, TestElementAccessNonDouble) {
    SquareMatrix_<int> m(2, 0);
    m(0, 1) = 3;
    const SquareMatrix_<int>& cm = m;
    ASSERT_EQ(cm(0, 1), 3);
    ASSERT_EQ(cm(1, 0), 0);
}

TEST(SquareMatrixTest, TestElementAccessDouble) {
    SquareMatrix_<double> m(2, 1.5);
    m(1, 1) = 2.5;
    ASSERT_DOUBLE_EQ(m(1, 1), 2.5);
    ASSERT_DOUBLE_EQ(m(0, 0), 1.5);
}
