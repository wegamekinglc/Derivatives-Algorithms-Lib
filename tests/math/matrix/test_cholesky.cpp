//
// Created by wegamekinglc on 22-12-17.
//

#include <gtest/gtest.h>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/utilities/numerics.hpp>

using namespace Dal;

TEST(MatrixTest, TestCholeskyDecomposition) {
    const int n = 11;
    double tmp[n][n] = {
      {6.4e-05, 5.28e-05, 2.28e-05,  0.00032, 0.00036, 6.4e-05, 6.3968010664e-06,  7.2e-05, 7.19460269899e-06, 1.2e-05, 1.19970004999e-06},
      {5.28e-05, 0.000121, 1.045e-05, 0.00044, 0.000165, 2.2e-05, 2.19890036657e-06, 1.65e-05, 1.64876311852e-06, 1.1e-05, 1.09972504583e-06},
      {2.28e-05,  1.045e-05, 9.025e-05, 0,  0.0001425, 9.5e-06, 9.49525158294e-07, 2.85e-05,  2.84786356835e-06, 4.75e-06, 4.74881269789e-07},
      {0.00032, 0.00044, 0, 0.04, 0.009, 0.0008, 7.996001333e-05, 0.0006, 5.99550224916e-05, 0.0001,  9.99750041661e-06},
      {0.00036, 0.000165,  0.0001425, 0.009,   0.0225,  0.0003, 2.99850049987e-05, 0.001125,  0.000112415667172, 0.000225,  2.24943759374e-05},
      {6.4e-05, 2.2e-05, 9.5e-06,   0.0008,  0.0003,  0.0001, 9.99500166625e-06, 7.5e-05, 7.49437781145e-06, 2e-05,   1.99950008332e-06},
      {6.3968010664e-06,  2.19890036657e-06, 9.49525158294e-07, 7.996001333e-05,   2.99850049987e-05, 9.99500166625e-06, 9.99000583083e-07, 7.49625124969e-06, 7.49063187129e-07, 1.99900033325e-06, 1.99850066645e-07},
      {7.2e-05, 1.65e-05,  2.85e-05,  0.0006,  0.001125,  7.5e-05, 7.49625124969e-06, 0.000225,  2.24831334343e-05, 1.5e-05, 1.49962506249e-06},
      {7.19460269899e-06, 1.64876311852e-06, 2.84786356835e-06, 5.99550224916e-05, 0.000112415667172, 7.49437781145e-06, 7.49063187129e-07, 2.24831334343e-05, 2.24662795123e-06, 1.49887556229e-06, 1.49850090584e-07},
      {1.2e-05, 1.1e-05, 4.75e-06,  0.0001,  0.000225,  2e-05,  1.99900033325e-06, 1.5e-05, 1.49887556229e-06, 2.5e-05, 2.49937510415e-06},
      {1.19970004999e-06, 1.09972504583e-06, 4.74881269789e-07, 9.99750041661e-06, 2.24943759374e-05, 1.99950008332e-06, 1.99850066645e-07, 1.49962506249e-06, 1.49850090584e-07, 2.49937510415e-06, 2.49875036451e-07}};

    SquareMatrix_<> m(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            m(i, j) = tmp[i][j];

    Handle_<SquareMatrixDecomposition_> de_comp(CholeskyDecomposition(m));
}

TEST(MatrixTest, TestCholeskyDecompositionV2) {
    const int n = 3;
    double tmp[n][n] = {
        {6.0, 2.0, 3.0},
        {2.0, 9.0, 1.0},
        {3.0, 1.0, 13.0}
    };

    SquareMatrix_<> m(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            m(i, j) = tmp[i][j];

    Vector_<> b{1.0, 2.0, 3.0};
    Vector_<> x(n);
    Handle_<SymmetricMatrixDecomposition_> de_comp(CholeskyDecomposition(m));

    de_comp->Solve(b, &x);
    Vector_<> expected = {-0.00869565, 0.2,  0.2173913};
    for(int i = 0; i < n; ++i)
        ASSERT_NEAR(expected[i], x[i], 1e-7);
}

TEST(MatrixTest, TestCholeskySolve) {
    const int n = 3;
    double tmp[n][n] = {
      {6.0, 2.0, 3.0},
      {2.0, 9.0, 1.0},
      {3.0, 1.0, 13.0}
    };

    SquareMatrix_<> m(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            m(i, j) = tmp[i][j];

    Vector_<Vector_<>> b(1);
    b[0] = {1.0, 2.0, 3.0};

    CholeskySolve(&m, &b);
    Vector_<> expected = {-0.00869565, 0.2,  0.2173913};
    for(int i = 0; i < n; ++i)
        ASSERT_NEAR(expected[i], b[0][i], 1e-7);
}

TEST(MatrixTest, TestCholeskyMultiply) {
    const int n = 3;
    double tmp[n][n] = {
            {6.0, 2.0, 3.0},
            {2.0, 9.0, 1.0},
            {3.0, 1.0, 13.0}
    };

    SquareMatrix_<> m(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            m(i, j) = tmp[i][j];

    Handle_<SymmetricMatrixDecomposition_> de_comp(CholeskyDecomposition(m));

    Vector_<> x{1.0, 2.0, 3.0};
    Vector_<> b(n, 0.0);
    de_comp->Multiply(x, &b);

    Vector_<> expected = {19.0, 23.0,  44.0};
    for(int i = 0; i < n; ++i)
        ASSERT_NEAR(expected[i], b[i], 1e-7);
}

TEST(MatrixTest, TestCholeskyMakeCorrelated) {
    const int n = 3;
    double tmp[n][n] = {
            {1.2, 0.2, -0.3},
            {0.2, 1.0, 0.1},
            {-0.3, 0.1, 0.8}
    };

    SquareMatrix_<> m(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            m(i, j) = tmp[i][j];

    Handle_<SymmetricMatrixDecomposition_> de_comp(CholeskyDecomposition(m));

    int i_path = 0;
    std::unique_ptr<SequenceSet_> set(NewSobol(n, i_path));
    Vector_<> dst(n);
    Vector_<> correlated(n);
    int num_path = 500000;

    Matrix_<> deviates(num_path, n);

    for (int i = 0; i < num_path; ++i) {
        set->FillNormal(&dst);
        de_comp->MakeCorrelated(dst.begin(), &correlated);
        for (int j = 0; j < n; ++j)
            deviates(i, j) = correlated[j];
    }

    SquareMatrix_<> calcVar(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            auto c1 = deviates.Col(i);
            auto c2 = deviates.Col(j);
            calcVar(i, j) = InnerProduct(c1, c2) / num_path;
            ASSERT_NEAR(calcVar(i, j), m(i, j), 1e-4);
        }
}
