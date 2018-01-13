//
// Created by Cheng Li on 2018/1/13.
//

#include <dal/platform/platform.hpp>
#include <dal/math/matrix.hpp>
#include <gtest/gtest.h>

using matrix_t = dal::Matrix_<>;

TEST(MatrixTest, TestMatrixNullConstructor) {
    matrix_t m1;

    ASSERT_EQ(m1.Rows(), 0);
    ASSERT_EQ(m1.Cols(), 0);
}

TEST(MatrixTest, TestMatrixRowsColsConstructor) {
    matrix_t m1(3, 4);

    ASSERT_EQ(m1.Rows(), 3);
    ASSERT_EQ(m1.Cols(), 4);
}

TEST(MatrixTest, TestMatrixCopyConstructor) {
    matrix_t m1(3, 4);
    matrix_t m2(m1);

    ASSERT_EQ(m2.Rows(), 3);
    ASSERT_EQ(m2.Cols(), 4);
}

TEST(MatrixTest, TestMatrixEmpty) {
    matrix_t m1;
    ASSERT_TRUE(m1.Empty());

    matrix_t m2(3, 4);
    ASSERT_FALSE(m2.Empty());
}

TEST(MatrixTest, TestMatrixClear) {
    matrix_t m1(3, 4);

    m1.Clear();
    ASSERT_TRUE(m1.Empty());
    ASSERT_EQ(m1.Rows(), 0);
    ASSERT_EQ(m1.Cols(), 0);
}