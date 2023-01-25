//
// Created by wegam on 2023/1/25.
//

#include <gtest/gtest.h>
#include <dal/math/ndarray.hpp>

using namespace Dal;

TEST(NDArrayTest, TestEmptyArrayN) {
    ArrayN_<> array_n(Vector_<int>({0, 0, 0}));
    ASSERT_TRUE(array_n.IsEmpty());
}

TEST(NDArrayTest, TestArrayNSwapWithVector) {
    ArrayN_<> array_1(Vector_<int>({0}));
    Vector_<> vec1{1.0, 2.0, 3.0};
    array_1.Swap(&vec1);

    // with thrown when ArrayN is not 1-D
    ArrayN_<> array_2(Vector_<int>({2, 2}));
    Vector_<> vec2{1.0, 2.0, 3.0};
    ASSERT_THROW(array_2.Swap(&vec2), std::runtime_error);
}


TEST(NDArrayTest, TestArrayNLoc) {
    ArrayN_<> array_n(Vector_<int>({2, 3}));
    ASSERT_EQ(array_n.Sizes(), Vector_<int>({2, 3}));

    array_n[{1, 2}] = 2.0;
    ASSERT_DOUBLE_EQ((array_n[{1, 2}]), 2.0);
}

TEST(NDArrayTest, TestCubeLoc) {
    Cube_<> cube(2, 3, 4);
    cube(1, 2, 3) = 2.0;
    ASSERT_DOUBLE_EQ(cube(1, 2, 3), 2.0);
}