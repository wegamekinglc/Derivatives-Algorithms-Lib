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

/*
 * TODO: make the tests meaningful
 */
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
    auto offset = 0;
    Vector_<> v_in{1.0, 2.0};
    acc.Add(v_in, offset);
    acc.Add(2.0 * v_in, offset);
}

TEST(MatrixTest, TestTriDiagonalMultiply) {
    const auto n = 10;
    Sparse::TriDiagonal_ trig(n);
    for(int i = 0; i < n; ++i) {
        if (i == 0) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i + 1, 2.0);
        }
        else if (i == 9) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
        }
        else {
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
    for(int i = 0; i < n; ++i) {
        if (i == 0) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i + 1, 2.0);
        }
        else if (i == 9) {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
        }
        else {
            trig.Set(i, i, 5.0);
            trig.Set(i, i - 1, 1.0);
            trig.Set(i, i + 1, 2.0);
        }
    }

    Vector_<> v(10, 1.0);
    Vector_<> expected = {0.15237329, 0.11906678, 0.1261464 , 0.1251006 , 0.1241753 , 0.12701146, 0.12038371, 0.135535 , 0.10097065, 0.17980587};
    Vector_<> calculated(n);
    trig.Decompose()->SolveLeft(v, &calculated);
    for (int i = 0; i < n; ++i)
        ASSERT_NEAR(calculated[i], expected[i], 1e-6);

    trig.Decompose()->SolveRight(v, &calculated);
    expected = {0.17980587, 0.10097065, 0.135535  , 0.12038371, 0.12701146, 0.1241753 , 0.1251006 , 0.1261464 , 0.11906678, 0.15237329};
    for (int i = 0; i < n; ++i)
        ASSERT_NEAR(calculated[i], expected[i], 1e-6);
}