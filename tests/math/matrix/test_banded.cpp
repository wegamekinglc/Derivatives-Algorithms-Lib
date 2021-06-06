//
// Created by wegam on 2021/2/24.
//

#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(BandedTest, TestNewBandedDiagonal) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 1, 1);
    ASSERT_EQ(mat->Size(), n);

    mat->Set(9, 8, 1.0);
    ASSERT_EQ((*mat)(9, 8), 1.0);

    mat->Add(9, 8, 2.0);
    ASSERT_EQ((*mat)(9, 8), 3.0);

    ASSERT_THROW(mat->Set(0, 2, 2.0), Exception_);
}


TEST(BandedTest, TestNewBandedBanded) {
    const int n = 10;
    Sparse::Square_* mat = Sparse::NewBandDiagonal(n, 2, 1);
    ASSERT_EQ(mat->Size(), n);

    mat->Set(7, 9, 1.0);
    ASSERT_EQ((*mat)(7, 9), 1.0);

    mat->Add(9, 8, 3.0);
    ASSERT_EQ((*mat)(9, 8), 3.0);
}
