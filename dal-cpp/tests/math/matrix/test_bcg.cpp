//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
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

// Tri-diagonal systems with distinct band values exercise every branch of the
// fused Krylov sweeps. CG requires a symmetric positive-definite matrix, so it
// gets a symmetric system; BCG handles the asymmetric case (and its shadow path).
// The contract is residual ||Ax - b||_inf near machine precision after a tight solve.
namespace {
    void BuildSymmetricTridiag(Sparse::Square_* mat, int n) {
        for (int i = 0; i < n; ++i) {
            mat->Set(i, i, 4.0 + 0.1 * static_cast<double>(i));
            if (i > 0) {
                const double off = -0.5 - 0.01 * static_cast<double>(i);
                mat->Set(i, i - 1, off);
                mat->Set(i - 1, i, off);
            }
        }
    }

    void BuildAsymmetricTridiag(Sparse::Square_* mat, int n) {
        for (int i = 0; i < n; ++i) {
            mat->Set(i, i, 4.0 + 0.1 * static_cast<double>(i));
            if (i > 0)
                mat->Set(i, i - 1, -0.5 - 0.01 * static_cast<double>(i));
            if (i + 1 < n)
                mat->Set(i, i + 1, -0.3 + 0.02 * static_cast<double>(i));
        }
    }

    double ResidualInfNorm(const Sparse::Square_& A, const Vector_<>& x, const Vector_<>& b) {
        Vector_<> ax(x.size());
        A.MultiplyLeft(x, &ax);
        double worst = 0.0;
        for (int i = 0; i < static_cast<int>(ax.size()); ++i)
            worst = std::max(worst, std::fabs(ax[i] - b[i]));
        return worst;
    }
} // namespace

TEST(MatrixTest, TestCGSolveSymmetricLowResidual) {
    const int n = 40;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    BuildSymmetricTridiag(mat, n);

    Vector_<> b(n);
    for (int i = 0; i < n; ++i)
        b[i] = 1.0 + 0.1 * static_cast<double>(i % 7);
    Vector_<> x(n, 0.0);

    Sparse::CGSolve(*mat, b, 1e-12, 1e-14, 500, &x);
    ASSERT_LT(ResidualInfNorm(*mat, x, b), 1e-8);
}

TEST(MatrixTest, TestBCGSolveAsymmetricLowResidual) {
    const int n = 40;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    BuildAsymmetricTridiag(mat, n);

    Vector_<> b(n);
    for (int i = 0; i < n; ++i)
        b[i] = 1.0 + 0.1 * static_cast<double>(i % 7);
    Vector_<> x(n, 0.0);

    Sparse::BCGSolve(*mat, b, 1e-12, 1e-14, 500, &x);
    ASSERT_LT(ResidualInfNorm(*mat, x, b), 1e-8);
}

