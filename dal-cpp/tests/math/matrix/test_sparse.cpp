//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>

using namespace Dal;
using namespace Dal::Sparse;

TEST(MatrixTest, TestSymmetricDecomposition) {
    const int n = 10;
    TriDiagonal_ diag(n);
    for (int i = 0; i < n; ++i) {
        diag.Set(i, i, 10.0);
        if (i < n - 1)
            diag.Set(i, i+1, 3.0);
        if (i >= 1)
            diag.Set(i, i-1, 3.0);
    }

    std::unique_ptr<SymmetricDecomposition_> de_comp(dynamic_cast<SymmetricDecomposition_*>(diag.Decompose()));
    Vector_<> b(n, 1.0);
    Vector_<> x(n);
    de_comp->Solve(b, &x);

    Vector_<> expected = {0.08333239, 0.05555869, 0.0648053 , 0.06175697, 0.06267147, 0.06267147, 0.06175697, 0.0648053 , 0.05555869, 0.08333239};
    for (int i = 0; i < n; ++i)
        ASSERT_NEAR(expected[i], x[i], 1e-8);
}