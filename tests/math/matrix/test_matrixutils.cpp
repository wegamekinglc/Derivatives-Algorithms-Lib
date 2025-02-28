//
// Created by wegam on 2022/4/10.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/matrix/matrixutils.hpp>

using matrix_t = Dal::Matrix_<>;

TEST(MatrixTest, TestMakeTranspose) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    auto m2 = Dal::Matrix::MakeTranspose(m1);

    ASSERT_EQ(m2(0, 0), m1(0, 0)); ASSERT_EQ(m2(0, 1), m1(1, 0));
    ASSERT_EQ(m2(1, 0), m1(0, 1)); ASSERT_EQ(m2(1, 1), m1(1, 1));
    ASSERT_EQ(m2(2, 0), m1(0, 2)); ASSERT_EQ(m2(2, 1), m1(1, 2));
}

TEST(MatrixTest, TestAppend) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    matrix_t m2(1, 3);
    m2(0, 0) = 1.0; m2(0, 1) = 2.0; m2(0, 2) = 3.0;

    Dal::Matrix::Append(&m1, m2);
    ASSERT_EQ(m1.Rows(), 3);
    ASSERT_EQ(m1.Cols(), 3);

    ASSERT_EQ(m1(0, 0), 1); ASSERT_EQ(m1(0, 1), 2); ASSERT_EQ(m1(0, 2), 3);
    ASSERT_EQ(m1(1, 0), 4); ASSERT_EQ(m1(1, 1), 5); ASSERT_EQ(m1(1, 2), 6);
    ASSERT_EQ(m1(2, 0), 1); ASSERT_EQ(m1(2, 1), 2); ASSERT_EQ(m1(2, 2), 3);
}

TEST(MatrixTest, TestAppendWithBigMatrix) {
    matrix_t m1(2, 1);
    m1(0, 0) = 1.0;
    m1(1, 0) = 2.0;

    matrix_t m2(1, 3);
    m2(0, 0) = 1.0; m2(0, 1) = 2.0; m2(0, 2) = 3.0;

    Dal::Matrix::Append(&m1, m2);
    ASSERT_EQ(m1.Rows(), 3);
    ASSERT_EQ(m1.Cols(), 3);

    ASSERT_EQ(m1(0, 0), 1); ASSERT_EQ(m1(0, 1), 0); ASSERT_EQ(m1(0, 2), 0);
    ASSERT_EQ(m1(1, 0), 2); ASSERT_EQ(m1(1, 1), 0); ASSERT_EQ(m1(1, 2), 0);
    ASSERT_EQ(m1(2, 0), 1); ASSERT_EQ(m1(2, 1), 2); ASSERT_EQ(m1(2, 2), 3);
}

TEST(MatrixTest, TestSingleTransformInplace) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    Dal::Matrix::Transform(&m1, Dal::Square);
    ASSERT_EQ(m1(0, 0), 1); ASSERT_EQ(m1(0, 1), 4); ASSERT_EQ(m1(0, 2), 9);
    ASSERT_EQ(m1(1, 0), 16); ASSERT_EQ(m1(1, 1), 25); ASSERT_EQ(m1(1, 2), 36);
}

TEST(MatrixTest, TestBinaryTransformInplace) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    matrix_t m2(2, 3);
    m2(0, 0) = 1.0; m2(0, 1) = 2.0; m2(0, 2) = 3.0;
    m2(1, 0) = 4.0; m2(1, 1) = 5.0; m2(1, 2) = 6.0;

    Dal::Matrix::Transform(&m1, m2,[](double x, double y){ return x * y;});
    ASSERT_EQ(m1(0, 0), 1); ASSERT_EQ(m1(0, 1), 4); ASSERT_EQ(m1(0, 2), 9);
    ASSERT_EQ(m1(1, 0), 16); ASSERT_EQ(m1(1, 1), 25); ASSERT_EQ(m1(1, 2), 36);
}

TEST(MatrixTest, TestSingleTransformWithOutput) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    matrix_t m2(2, 3);
    Dal::Matrix::Transform(m1, Dal::Square, &m2);
    ASSERT_EQ(m2(0, 0), 1); ASSERT_EQ(m2(0, 1), 4); ASSERT_EQ(m2(0, 2), 9);
    ASSERT_EQ(m2(1, 0), 16); ASSERT_EQ(m2(1, 1), 25); ASSERT_EQ(m2(1, 2), 36);
}

TEST(MatrixTest, TestBinaryTransformWithOutput) {
    matrix_t m1(2, 3);
    m1(0, 0) = 1.0; m1(0, 1) = 2.0; m1(0, 2) = 3.0;
    m1(1, 0) = 4.0; m1(1, 1) = 5.0; m1(1, 2) = 6.0;

    matrix_t m2(2, 3);
    m2(0, 0) = 1.0; m2(0, 1) = 2.0; m2(0, 2) = 3.0;
    m2(1, 0) = 4.0; m2(1, 1) = 5.0; m2(1, 2) = 6.0;

    matrix_t m3(2, 3);
    Dal::Matrix::Transform(m1, m2, [](double x, double y){ return x * y;}, &m3);
    ASSERT_EQ(m3(0, 0), 1); ASSERT_EQ(m3(0, 1), 4); ASSERT_EQ(m3(0, 2), 9);
    ASSERT_EQ(m3(1, 0), 16); ASSERT_EQ(m3(1, 1), 25); ASSERT_EQ(m3(1, 2), 36);
}





