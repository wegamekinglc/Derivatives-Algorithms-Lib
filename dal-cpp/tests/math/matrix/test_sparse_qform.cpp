//
// Created by wegam on 2026/07/04.
//

#include <gtest/gtest.h>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/utilities/numerics.hpp>

using namespace Dal;
using namespace Dal::Sparse;

namespace {
    Vector_<Vector_<>> SolveEachRow(SymmetricDecomposition_& deComp, const Matrix_<>& j_mat) {
        const int jRows = j_mat.Rows();
        const int n = j_mat.Cols();
        Vector_<Vector_<>> solvedRows(jRows);
        for (int i = 0; i < jRows; ++i) {
            Vector_<> rowJ(n);
            for (int k = 0; k < n; ++k)
                rowJ[k] = j_mat(i, k);
            solvedRows[i].Resize(n);
            deComp.Solve(rowJ, &solvedRows[i]);
        }
        return solvedRows;
    }
} // namespace

TEST(SparseQFormTest, TestDenseSymmetricDecompositionQForm) {
    const int n = 4;
    double tmpW[n][n] = {
        {4.0, 1.0, 0.5, 0.2},
        {1.0, 5.0, 0.3, 0.7},
        {0.5, 0.3, 6.0, 1.1},
        {0.2, 0.7, 1.1, 7.0}};
    SquareMatrix_<> w(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            w(i, j) = tmpW[i][j];

    std::unique_ptr<SymmetricDecomposition_> deComp(CholeskyDecomposition(w));

    const int jRows = 3;
    Matrix_<> j_mat(jRows, n, 0.0);
    double tmpJ[jRows][n] = {
        {1.0, 2.0, -1.0, 0.5},
        {0.5, -1.0, 2.0, 1.0},
        {1.5, 0.25, 0.75, -0.5}};
    for (int i = 0; i < jRows; ++i)
        for (int k = 0; k < n; ++k)
            j_mat(i, k) = tmpJ[i][k];

    SquareMatrix_<> dst;
    deComp->QForm(j_mat, &dst);

    // Reference: dst_ref[i][k] = sum_r (W^{-1} J^T)[r][i] * J[k][r] via per-row solves.
    // Solve W x_i = J_i for each J row; then dst_ref[i][k] = InnerProduct(x_i, J_k).
    const Vector_<Vector_<>> solvedRows = SolveEachRow(*deComp, j_mat);

    ASSERT_EQ(dst.Rows(), jRows);
    ASSERT_EQ(dst.Cols(), jRows);
    for (int i = 0; i < jRows; ++i)
        for (int k = 0; k < jRows; ++k) {
            const double expected = InnerProduct(solvedRows[i], j_mat.Row(k));
            ASSERT_NEAR(dst(i, k), expected, 1e-9);
            ASSERT_NEAR(dst(k, i), expected, 1e-9);
        }
}
