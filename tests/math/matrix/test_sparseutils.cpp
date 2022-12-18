//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparseutils.hpp>

using namespace Dal;

TEST(SparseUtilsTest, TestAddCoupling) {
    const int n = 10;
    std::unique_ptr <Sparse::TriDiagonal_> ret_val(new Sparse::TriDiagonal_(n));
    Sparse::AddCoupling(ret_val.get(), 2, 3, 1.0);
    ASSERT_DOUBLE_EQ(*ret_val->At(2, 2), 1.0);
    ASSERT_DOUBLE_EQ(*ret_val->At(2, 3), -1.0);
    ASSERT_DOUBLE_EQ(*ret_val->At(3, 2), -1.0);
    ASSERT_DOUBLE_EQ(*ret_val->At(3, 3), 1.0);
}
