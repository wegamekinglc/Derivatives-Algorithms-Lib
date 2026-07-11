//
// Created by wegam on 2021/2/24.
//

#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(MatrixTest, TestNewBandedDiagonal) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    ASSERT_EQ(mat->Size(), n);

    mat->Set(9, 8, 1.0);
    ASSERT_EQ((*mat)(9, 8), 1.0);

    mat->Add(9, 8, 2.0);
    ASSERT_EQ((*mat)(9, 8), 3.0);

    ASSERT_THROW(mat->Set(0, 2, 2.0), Exception_);
}

TEST(MatrixTest, TestNewBandedBanded) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 2, 1);
    ASSERT_EQ(mat->Size(), n);

    mat->Set(7, 9, 1.0);
    ASSERT_EQ((*mat)(7, 9), 1.0);

    mat->Add(9, 8, 3.0);
    ASSERT_EQ((*mat)(9, 8), 3.0);
}

TEST(MatrixTest, TestLowerBandAccumulator) {
    LowerBandAccumulator_ acc(3, 1);
    acc.Add(Vector_<>{2.0}, -1);
    acc.Add(Vector_<>{3.0, 4.0}, -1);
    acc.Add(Vector_<>{5.0, 6.0}, 0);

    const Vector_<> b{2.0, 11.0, 28.0};
    Vector_<> calculated;
    acc.SolveLeft(b, &calculated);

    const Vector_<> expected{1.0, 2.0, 3.0};
    ASSERT_EQ(calculated.size(), expected.size());
    for (int i = 0; i < calculated.size(); ++i)
        ASSERT_NEAR(calculated[i], expected[i], 1e-10);
}

TEST(MatrixTest, TestTriDiagonalMultiply) {
    const auto n = 10;
    Sparse::TriDiagonal_ trig(n);
    for (int i = 0; i < n; ++i) {
        if (i == 0) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i + 1, 2.0);
        } else if (i == 9) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
        } else {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
            trig.Set(i, i + 1, 2.0);
        }
    }

    Vector_<> v(10, 1.0);
    Vector_<> expected = {7.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 6.0};
    Vector_<> calculated(n);
    trig.MultiplyLeft(v, &calculated);
    for (int i = 0; i < n; ++i)
        ASSERT_DOUBLE_EQ(calculated[i], expected[i]);

    trig.MultiplyRight(v, &calculated);
    expected = {6.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 7.0};
    for (int i = 0; i < n; ++i)
        ASSERT_DOUBLE_EQ(calculated[i], expected[i]);
}

TEST(MatrixTest, TestTriDiagonalSolve) {
    const auto n = 10;
    Sparse::TriDiagonal_ trig(n);
    for (int i = 0; i < n; ++i) {
        if (i == 0) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i + 1, 2.0);
        } else if (i == 9) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
        } else {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
            trig.Set(i, i + 1, 2.0);
        }
    }

    Vector_<> v(10, 1.0);
    Vector_<> expected = {0.15237329, 0.11906678, 0.1261464, 0.1251006, 0.1241753, 0.12701146, 0.12038371, 0.135535, 0.10097065, 0.17980587};
    Vector_<> calculated(n);
    trig.Decompose()->SolveLeft(v, &calculated);
    for (int i = 0; i < n; ++i)
        ASSERT_NEAR(calculated[i], expected[i], 1e-6);

    trig.Decompose()->SolveRight(v, &calculated);
    expected = {0.17980587, 0.10097065, 0.135535, 0.12038371, 0.12701146, 0.1241753, 0.1251006, 0.1261464, 0.11906678, 0.15237329};
    for (int i = 0; i < n; ++i)
        ASSERT_NEAR(calculated[i], expected[i], 1e-6);
}

// Varying x with distinct, asymmetric above/below bands exercises every branch of
// the fused TriMultiply (interior points combine all three bands; both boundaries
// drop one term). Hand-computed expected values lock the fused contract.
TEST(MatrixTest, TestTriDiagonalMultiplyAsymmetricFused) {
    const auto n = 5;
    Sparse::TriDiagonal_ trig(n);
    trig.Set(0, 0, 2.0);
    trig.Set(0, 1, 3.0); // above[0]
    trig.Set(1, 0, 4.0); // below[0]
    trig.Set(1, 1, 5.0);
    trig.Set(1, 2, 6.0); // above[1]
    trig.Set(2, 1, 7.0); // below[1]
    trig.Set(2, 2, 8.0);
    trig.Set(2, 3, 9.0);  // above[2]
    trig.Set(3, 2, 10.0); // below[2]
    trig.Set(3, 3, 11.0);
    trig.Set(3, 4, 12.0); // above[3]
    trig.Set(4, 3, 13.0); // below[3]
    trig.Set(4, 4, 14.0);

    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> calculated(n);
    trig.MultiplyLeft(x, &calculated);
    // r[0] = diag[0]*x[0] + above[0]*x[1]
    // r[1] = diag[1]*x[1] + above[1]*x[2] + below[0]*x[0]
    // r[2] = diag[2]*x[2] + above[2]*x[3] + below[1]*x[1]
    // r[3] = diag[3]*x[3] + above[3]*x[4] + below[2]*x[2]
    // r[4] = diag[4]*x[4] + below[3]*x[3]
    Vector_<> expected = {2 * 1 + 3 * 2, 5 * 2 + 6 * 3 + 4 * 1, 8 * 3 + 9 * 4 + 7 * 2, 11 * 4 + 12 * 5 + 10 * 3, 14 * 5 + 13 * 4};
    for (int i = 0; i < n; ++i)
        ASSERT_DOUBLE_EQ(calculated[i], expected[i]);
}
