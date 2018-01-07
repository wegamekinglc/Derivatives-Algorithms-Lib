//
// Created by Cheng Li on 2017/12/29.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vector.hpp>
#include <dal/utilities/algorithms.hpp>
#include <functional>
#include <gtest/gtest.h>

using vector_t = dal::Vector_<>;

TEST(AlgorithmsTest, TestTransformWithUniaryOPerator) {
    vector_t s = {1, 2, 3};
    vector_t d(3);

    dal::Transform(s, std::negate<>(), &d);

    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i], -d[i]);
    }
}

TEST(AlgorithmsTest, TestTransformWithBinaryOperator) {
    vector_t s = {1, 2, 3};
    vector_t d(s.size());

    dal::Transform(s, s, std::plus<>(), &d);
    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i] + s[i], d[i]);
    }
}

TEST(AlgorithmsTest, TestTransformInplace) {
    vector_t s = {1, 2, 3};
    Transform(&s, std::negate<>());

    for (int i = 0; i != s.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i], -(i + 1));
    }
}

TEST(AlgorithmsTest, TestTransformInplaceWithOther) {
    vector_t s = {1, 2, 3};
    vector_t o = {1, 2, 3};

    dal::Transform(&s, o, std::plus<>());
    for (int i = 0; i != s.size(); ++i) {
        ASSERT_DOUBLE_EQ(2. * (i + 1), s[i]);
    }
}

TEST(AlgorithmsTest, TestFill) {
    vector_t s(3);
    double v = 1.;

    dal::Fill(&s, v);
    for (int i = 0; i != s.size(); ++i) {
        ASSERT_DOUBLE_EQ(1., s[i]);
    }
}

TEST(AlgorithmsTest, TestApplyWithUniaryOPerator) {
    vector_t s1 = {1, 2, 3};
    auto s2 = dal::Apply(std::negate<>(), s1);
    for (int i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], -s1[i]);
    }
}

TEST(AlgorithmsTest, TestApplyWithBinaryOPerator) {
    vector_t s1 = {1, 2, 3};
    vector_t s2 = {3, 2, 1};
    auto s3= dal::Apply(std::plus<>(), s1, s2);
    for (int i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s3[i], s1[i] + s2[i]);
    }
}

TEST(AlgorithmsTest, TestAppendGeneral) {
    std::vector<int> s1 = {1, 2, 3};
    std::vector<int> s2 = {4, 5, 6};

    dal::Append(&s1, s2);
    for (int i = 0; i != s1.size(); ++i) {
        ASSERT_EQ(s1[i], i + 1);
    }
}

TEST(AlgorithmsTest, TestAppendVector) {
    vector_t s1 = {1., 2., 3.};
    vector_t s2 = {4., 5., 6.};
    dal::Append(&s1, s2);
    for (int i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], i + 1.);
    }
}

TEST(AlgorithmsTest, TestCopyWithPreAllocatedVector) {
    vector_t s1 = {1, 2, 3};
    vector_t s2(s1.size());

    dal::Copy(s1, &s2);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], s2[i]);
    }
}

TEST(AlgorithmsTest, TestCopyWithReturnedValue) {
    vector_t s1 = {1, 2, 3};

    auto s2 = dal::Copy(s1);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], s2[i]);
    }
}

TEST(AlgorithmsTest, TestConcatenate) {
    vector_t s1 = {1., 2., 3.};
    vector_t s2 = {4., 5., 6.};
    auto s3 = dal::Concatenate(s1, s2);
    for (int i = 0; i != s1.size(); ++i) {
        ASSERT_DOUBLE_EQ(s1[i], i + 1.);
    }
}

TEST(AlgorithmsTest, TestFilter) {
    vector_t s1 = {-1., 1., -2., 2., -3., 3.};
    auto s2 = dal::Filter(s1, std::bind(std::greater<>(), std::placeholders::_1, 0.));
    ASSERT_EQ(s2.size(), 3);
    for (int i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], i + 1.);
    }
}