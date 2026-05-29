//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/bcg.hpp>

using namespace Dal;

TEST(MatrixTest, TestCGSolve) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    mat->Set(9, 8, 3.0);
    mat->Set(8, 9, 3.0);

    for(int i = 0; i < n; ++i)
        mat->Set(i, i, 10.0);

    Vector_<> result(n);
    Vector_<> b(n, 1.0);
    Sparse::CGSolve(*mat, b, 1e-4, 1e-4, 100, &result);

    ASSERT_NEAR(result[8], 0.07692308, 1e-8);
    ASSERT_NEAR(result[9], 0.07692308, 1e-8);
}

TEST(MatrixTest, TestBCGSolve) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    mat->Set(9, 8, 3.0);
    mat->Set(8, 9, 2.0);

    for(int i = 0; i < n; ++i)
        mat->Set(i, i, 10.0);

    Vector_<> result(n);
    Vector_<> b(n, 1.0);
    Sparse::BCGSolve(*mat, b, 1e-4, 1e-4, 100, &result);

    ASSERT_NEAR(result[8], 0.08510638, 1e-8);
    ASSERT_NEAR(result[9], 0.07446809, 1e-8);
}

