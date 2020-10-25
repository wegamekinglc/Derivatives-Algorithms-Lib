//
// Created by Cheng Li on 2018/1/13.
//

#include <dal/platform/platform.hpp>
#include <dal/math/matrixs.hpp>
#include <gtest/gtest.h>

using matrix_t = Dal::Matrix_<>;

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

TEST(MatrixTest, TestMatrixMoveConstructor) {
    auto m2(matrix_t(3, 4));
    ASSERT_EQ(m2.Rows(), 3);
    ASSERT_EQ(m2.Cols(), 4);
}

TEST(MatrixTest, TestMatrixCopyMoveConstructor) {
    auto m2 = matrix_t(3, 4);
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

TEST(MatrixTest, TestMatrixFirstLast) {
    matrix_t m1(3, 4);
    ASSERT_EQ(3 * 4, m1.Last() - m1.First());
}

TEST(MatrixTest, TestMatrixConstAccess) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;
    const matrix_t m2(m1);
    ASSERT_DOUBLE_EQ(2., m2(1, 0));
}

TEST(MatrixTest, TestMatrixAccess) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;
    ASSERT_DOUBLE_EQ(2., m1(1, 0));
}

TEST(MatrixTest, TestMatrixConstRow) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;
    const matrix_t m2(m1);

    matrix_t::ConstRow_ row = m2.Row(1);
    ASSERT_DOUBLE_EQ(row[0], 2.);
    ASSERT_DOUBLE_EQ(row[1], 0.);
}

TEST(MatrixTest, TestMatrixRow) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;

    matrix_t::Row_ row = m1.Row(1);
    ASSERT_DOUBLE_EQ(row[0], 2.);
    ASSERT_DOUBLE_EQ(row[1], 0.);
}

TEST(MatrixTest, TestMatrixRowSize) {
    matrix_t  m1(2, 3);
    auto row = m1.Row(1);
    ASSERT_EQ(m1.Cols(), row.size());
}

TEST(MatrixTest, TestMatrixRowFrontBack) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;

    matrix_t::ConstRow_ row = m1.Row(1);
    ASSERT_DOUBLE_EQ(row.front(), 2.);
    ASSERT_DOUBLE_EQ(row.back(), 0.);
}

TEST(MatrixTest, TestMatrixConstIteratorConstRow) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;
    const matrix_t m2(m1);

    matrix_t::ConstRow_ row = m2.Row(1);
    ASSERT_EQ(row.end() - row.begin(), 2);
}

TEST(MatrixTest, TestMatrixConstIteratorRow) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;

    const matrix_t::Row_ row = m1.Row(1);
    ASSERT_EQ(row.end() - row.begin(), 2);
}

TEST(MatrixTest, TestMatrixIteratorRow) {
    matrix_t m1(2, 2);
    m1(1, 0) = 2.;

    matrix_t::Row_ row = m1.Row(1);
    *row.begin() = 0.5;
    *(row.end() - 1) = 1.0;
    ASSERT_DOUBLE_EQ(row[0], 0.5);
    ASSERT_DOUBLE_EQ(row[1], 1.);
}

TEST(MatrixTest, TestMatrixConstCol) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;
    const matrix_t m2(m1);

    matrix_t::ConstCol_ col = m2.Col(1);
    ASSERT_DOUBLE_EQ(col[0], 0.);
    ASSERT_DOUBLE_EQ(col[1], 2.);
    ASSERT_DOUBLE_EQ(col[2], 0.);
}

TEST(MatrixTest, TestMatrixCol) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;

    matrix_t::Col_ col = m1.Col(1);
    ASSERT_DOUBLE_EQ(col[0], 0.);
    ASSERT_DOUBLE_EQ(col[1], 2.);
    ASSERT_DOUBLE_EQ(col[2], 0.);
}

TEST(MatrixTest, TestMatrixColSize) {
    matrix_t m1(3, 2);
    auto col = m1.Col(1);
    ASSERT_EQ(m1.Rows(), col.size());
}

TEST(MatrixTest, TestMatrixConstIteratorConstCol) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;
    const matrix_t m2(m1);

    matrix_t::ConstCol_ col = m2.Col(1);
    auto iter = col.begin();
    ASSERT_DOUBLE_EQ(*iter, 0.);
    ASSERT_DOUBLE_EQ(*++iter, 2.);
    ASSERT_DOUBLE_EQ(*++iter, 0.);
}

TEST(MatrixTest, TestMatrixIteratorCol) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;

    matrix_t::Col_ col = m1.Col(1);
    auto iter = col.begin();
    ASSERT_DOUBLE_EQ(*iter, 0.);
    ASSERT_DOUBLE_EQ(*++iter, 2.);
    ASSERT_DOUBLE_EQ(*++iter, 0.);
}

#ifdef NDEBUG
TEST(MatrixTest, TestMatrixConstColBeginEnd) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;
    const matrix_t m2(m1);

    matrix_t::ConstCol_ col = m2.Col(1);
    auto iter = col.end();
    ASSERT_EQ(col.end() - col.begin(), m2.Rows());

    iter = col.begin();

}

TEST(MatrixTest, TestMatrixColBeginEnd) {
    matrix_t m1(3, 2);
    m1(1, 1) = 2.;

    matrix_t::Col_ col = m1.Col(1);
    auto iter = col.end();
    ASSERT_EQ(col.end() - col.begin(), m1.Rows());
}
#endif