//
// Created by wegam on 2023/1/25.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/ndarray.hpp>

using namespace Dal;

TEST(NDArrayTest, TestEmptyArrayN) {
    ArrayN_<> array_n(Vector_<int>({0, 0, 0}));
    ASSERT_TRUE(array_n.IsEmpty());
}

TEST(NDArrayTest, TestArrayNRequiresDimension) {
    ASSERT_THROW(ArrayN_<>(Vector_<int>{}), Dal::Exception_);
}

TEST(NDArrayTest, TestResizePreservesOverlap) {
    ArrayN_<int> array({2, 2});
    array[{0, 0}] = 7;
    array[{1, 1}] = 9;
    array.Resize({2, 2});
    ASSERT_EQ((array[{0, 0}]), 7);
    array.Resize({3, 3});
    ASSERT_EQ((array[{0, 0}]), 7);
    ASSERT_EQ((array[{1, 1}]), 9);
    ASSERT_EQ((array[{2, 2}]), 0);
    array.Resize({1, 1});
    ASSERT_EQ((array[{0, 0}]), 7);
    array.Resize({0, 2});
    ASSERT_TRUE(array.IsEmpty());
    array.Resize({2, 2});
    ASSERT_EQ((array[{0, 0}]), 0);
}

TEST(NDArrayTest, TestResizeRejectsInvalidShapeWithoutMutation) {
    ArrayN_<int> array({2, 2}, 7);
    ASSERT_THROW(array.Resize({2}), Exception_);
    ASSERT_THROW(array.Resize({-1, 2}), Exception_);
    ASSERT_THROW(array.Resize({50000, 50000}), Exception_);
    ASSERT_EQ(array.Sizes(), (Vector_<int>{2, 2}));
    ASSERT_EQ((array[{0, 0}]), 7);
    ASSERT_THROW(ArrayN_<int>({-1, 2}), Exception_);
    ASSERT_THROW(ArrayN_<int>({50000, 50000}), Exception_);
    ASSERT_TRUE(ArrayN::Moves({2, 2}, {0, 2}).empty());
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

TEST(NDArrayTest, TestCubeElementAccessNonDouble) {
    Cube_<int> cube(2, 3, 4);
    cube(1, 2, 3) = 7;
    const Cube_<int>& ccube = cube;
    ASSERT_EQ(ccube(1, 2, 3), 7);
    ASSERT_EQ(ccube(0, 0, 0), 0);

    int* slice = cube.SliceBegin(1, 2);
    slice[0] = 9;
    ASSERT_EQ(cube(1, 2, 0), 9);
    ASSERT_EQ(cube.SliceEnd(1, 2) - cube.SliceBegin(1, 2), 4);
}
