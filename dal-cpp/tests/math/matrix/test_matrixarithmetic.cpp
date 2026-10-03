//
// Created by Cheng Li on 2018/8/14.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/utilities/numerics.hpp>

using namespace Dal;

namespace {
    // A = [1 2 0; 0 1 3] (2x3), B = [2 0; 1 1; 0 2] (3x2), so A*B = [4 2; 1 7]
    Matrix_<> DemoA() {
        Matrix_<> a(2, 3);
        a(0, 0) = 1.0;
        a(0, 1) = 2.0;
        a(0, 2) = 0.0;
        a(1, 0) = 0.0;
        a(1, 1) = 1.0;
        a(1, 2) = 3.0;
        return a;
    }
    Matrix_<> DemoB() {
        Matrix_<> b(3, 2);
        b(0, 0) = 2.0;
        b(0, 1) = 0.0;
        b(1, 0) = 1.0;
        b(1, 1) = 1.0;
        b(2, 0) = 0.0;
        b(2, 1) = 2.0;
        return b;
    }
    void Assert2x2(const Matrix_<>& m, double m00, double m01, double m10, double m11) {
        ASSERT_EQ(m.Rows(), 2);
        ASSERT_EQ(m.Cols(), 2);
        ASSERT_DOUBLE_EQ(m(0, 0), m00);
        ASSERT_DOUBLE_EQ(m(0, 1), m01);
        ASSERT_DOUBLE_EQ(m(1, 0), m10);
        ASSERT_DOUBLE_EQ(m(1, 1), m11);
    }
} // namespace

TEST(MatrixTest, TestMultiplyMatrixMatrix) {
    Matrix_<> c;
    Matrix::Multiply(DemoA(), DemoB(), &c);
    Assert2x2(c, 4.0, 2.0, 1.0, 7.0);
}

TEST(MatrixTest, TestMultiplyMatrixMatrixAliasedLeft) {
    Matrix_<> a = DemoA();
    Matrix::Multiply(a, DemoB(), &a);
    Assert2x2(a, 4.0, 2.0, 1.0, 7.0);
}

TEST(MatrixTest, TestMultiplyMatrixMatrixAliasedRight) {
    Matrix_<> b = DemoB();
    Matrix::Multiply(DemoA(), b, &b);
    Assert2x2(b, 4.0, 2.0, 1.0, 7.0);
}

TEST(MatrixTest, TestMultiplyMatrixVector) {
    Vector_<> x(3);
    x[0] = 1.0;
    x[1] = 2.0;
    x[2] = -1.0;
    Vector_<> y;
    Matrix::Multiply(DemoA(), x, &y);
    ASSERT_EQ(y.size(), 2U);
    ASSERT_DOUBLE_EQ(y[0], 5.0);
    ASSERT_DOUBLE_EQ(y[1], -1.0);
}

TEST(MatrixTest, TestMultiplyVectorMatrix) {
    Vector_<> v(3);
    v[0] = 1.0;
    v[1] = 2.0;
    v[2] = 0.0;
    Vector_<> y;
    Matrix::Multiply(v, DemoB(), &y);
    ASSERT_EQ(y.size(), 2U);
    ASSERT_DOUBLE_EQ(y[0], 4.0);
    ASSERT_DOUBLE_EQ(y[1], 2.0);
}

TEST(MatrixTest, TestMultiplyVectorMatrixAliased) {
    Vector_<> v(3);
    v[0] = 1.0;
    v[1] = 2.0;
    v[2] = 0.0;
    Matrix::Multiply(v, DemoB(), &v);
    ASSERT_EQ(v.size(), 2U);
    ASSERT_DOUBLE_EQ(v[0], 4.0);
    ASSERT_DOUBLE_EQ(v[1], 2.0);
}

TEST(MatrixTest, TestMultiplySizeMismatch) {
    Matrix_<> c;
    Vector_<> v;
    ASSERT_THROW(Matrix::Multiply(DemoA(), DemoA(), &c), Exception_);
    ASSERT_THROW(Matrix::Multiply(DemoA(), Vector_<>(2), &v), Exception_);
    ASSERT_THROW(Matrix::Multiply(Vector_<>(3), DemoA(), &v), Exception_);
}

TEST(MatrixTest, TestWeightedInnerProduct) {
    Matrix_<> w(2, 2);
    w(0, 0) = 2.0;
    w(0, 1) = 0.0;
    w(1, 0) = 1.0;
    w(1, 1) = 1.0;
    Vector_<> left(2, 1.0);
    left[1] = 2.0;
    Vector_<> right(2);
    right[0] = 3.0;
    right[1] = 4.0;
    ASSERT_DOUBLE_EQ(Matrix::WeightedInnerProduct(left, w, right), 20.0);
}

TEST(MatrixTest, TestAddJSquaredToUpper) {
    Matrix_<> a(3, 2);
    a(0, 0) = 1.0;
    a(0, 1) = 0.0;
    a(1, 0) = 0.0;
    a(1, 1) = 1.0;
    a(2, 0) = 1.0;
    a(2, 1) = 1.0;
    Matrix_<> h(3, 3, -1.0);
    Matrix::AddJSquaredToUpper(a, &h);
    // upper triangle accumulates on top of the -1 fill; the strict lower triangle stays untouched
    double expected[3][3] = {{0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {-1.0, -1.0, 1.0}};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            ASSERT_DOUBLE_EQ(h(i, j), expected[i][j]) << "i=" << i << " j=" << j;
}

TEST(MatrixTest, TestAddJSquaredToUpperPartialBlocks) {
    for (const int rows : {1, 3, 17}) {
        for (const int cols : {0, 1, 15, 16, 17, 31, 32, 33}) {
            SCOPED_TRACE(::testing::Message() << "rows=" << rows << " cols=" << cols);
            Matrix_<> a(rows, cols);
            for (int row = 0; row < rows; ++row)
                for (int col = 0; col < cols; ++col)
                    a(row, col) = (row + 1) * (col % 5 - 2) * 0.25;
            Matrix_<> h(rows, rows, -7.0);
            Matrix::AddJSquaredToUpper(a, &h);
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < rows; ++col) {
                    double expected = -7.0;
                    if (col >= row)
                        for (int k = 0; k < cols; ++k)
                            expected += a(row, k) * a(col, k);
                    ASSERT_DOUBLE_EQ(h(row, col), expected);
                }
            }
        }
    }
}

#ifdef NDEBUG
TEST(MatrixTest, TestVols) {
    const int n = 3;
    Matrix_<> cov(n, n);

    cov(0, 0) = 4.0;  cov(0, 1) = 1.5;  cov(0, 2) = -1.0;
    cov(1, 0) = 1.5;  cov(1, 1) = 3.0;  cov(1, 2) = -1.2;
    cov(2, 0) = -1.0; cov(2, 1) = -1.2; cov(2, 2) = 2.0;

    Matrix_<> corr(n, n);
    Matrix::Vols(cov, &corr);

    Matrix_<> expectedCorr(n, n);
    expectedCorr(0, 0) = 1.0;
    expectedCorr(0, 1) = 0.43301270;
    expectedCorr(0, 2) = -0.35355339;
    expectedCorr(1, 0) = 0.43301270;
    expectedCorr(1, 1) = 1.0;
    expectedCorr(1, 2) = -0.48989795;
    expectedCorr(2, 0) = -0.35355339;
    expectedCorr(2, 1) = -0.48989795;
    expectedCorr(2, 2) = 1.0;

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            ASSERT_NEAR(corr(i, j), expectedCorr(i, j), 1e-5);
}
#endif
