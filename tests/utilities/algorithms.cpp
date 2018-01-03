//
// Created by Cheng Li on 2017/12/29.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vector.hpp>
#include <dal/utilities/algorithms.hpp>
#include <functional>
#include <gtest/gtest.h>

using vector_t = dal::Vector_<>;

TEST(AlgorothmsTest, TestTransformWithUniaryOPerator) {
    vector_t s = {1, 2, 3};
    vector_t d(3);

    Transform(s, std::negate<>(), &d);

    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i], -d[i]);
    }
}

TEST(AlgorothmsTest, TestTransformWithBinaryOperator) {
    vector_t s = {1, 2, 3};
    vector_t d(s.size());

    Transform(s, s, std::plus<>(), &d);
    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i] + s[i], d[i]);
    }
}

TEST(AlgorothmsTest, TestApply) {
    vector_t s1 = {1, 2, 3};
    auto s2 = Apply(std::negate<>(), s1);
    for (int i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], -s1[i]);
    }
}

TEST(AlgorothmsTest, TestCopyWithPreAllocatedVector) {
    vector_t s1 = {1, 2, 3};
    vector_t s2(s1.size());

    Copy(s1, &s2);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], s2[i]);
    }
}

TEST(AlgorothmsTest, TestCopyWithReturnedValue) {
    vector_t s1 = {1, 2, 3};

    auto s2 = Copy(s1);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], s2[i]);
    }
}