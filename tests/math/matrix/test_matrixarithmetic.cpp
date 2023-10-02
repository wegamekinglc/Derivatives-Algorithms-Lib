//
// Created by Cheng Li on 2018/8/14.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>

using namespace Dal;

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