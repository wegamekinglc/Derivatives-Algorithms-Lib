//
// Created by Cheng Li on 2017/12/21.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vector.hpp>
#include <gtest/gtest.h>

TEST(VectorTest, DefaultConstructionTest) {
    Vector_<> s;
    ASSERT_TRUE(s.empty());
}

TEST(VectorTest, SizedConstructionTest) {
    const int n = 10;
    Vector_<> s(n);
    ASSERT_EQ(s.size(), n);
}

TEST(VectorTest, SizedFilledConstructionTest) {
    const int n = 10;
    const double val = 1.;
    Vector_<> s(n, val);

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, ContainerConstructionTest) {
    const int n = 10;
    const double val = 1.;

    std::vector<double> src(n, val);
    Vector_<> s(src.begin(), src.end());

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, InitializedListConstructionTest) {
    Vector_<> s = {1., 2., 3., 4., 5.};

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, VectorSwapTest) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<> s2;
    s1.Swap(&s2);

    ASSERT_TRUE(s1.empty());
    ASSERT_EQ(s2.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s2[i], i + 1.);
    }
}

TEST(VectorTest, VectorFillTest) {
    const int n = 10;
    const double val = 1.;
    Vector_<> s(n);
    s.Fill(val);

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, VectorResizeTest) {
    const int n1 = 5;
    const int n2 = 10;

    Vector_<> s(n1);
    ASSERT_EQ(s.size(), n1);

    s.Resize(n2);
    ASSERT_EQ(s.size(), n2);
}

TEST(VectorTest, VectorSelfMultiplyTest) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    s *= 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], 3 * (i + 1.));
    }
}

TEST(VectorTest, VectorSelfPlusTest) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    s += 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], 3 + i + 1.);
    }
}

TEST(VectorTest, VectorSelfMinusTest) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    s -= 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1. - 3.);
    }
}

TEST(VectorTest, VectorSelfPlusVectorTest) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<> s2 = {5., 4., 3., 2., 1.};
    s1 += s2;

    ASSERT_EQ(s1.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s1[i], 6.);
    }
}

TEST(VectorTest, VectorSelfMinusVectorTest) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<> s2 = {5., 4., 3., 2., 1.};
    s1 -= s2;

    ASSERT_EQ(s1.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s1[i], 2 * i - 4.);
    }
}

TEST(VectorTest, VectorAssignTest) {
    Vector_<> s;
    double data[] = {1., 2., 3., 4., 5.};

    s.Assign(&data[0], &data[4] + 1);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, VectorAppendByIteratorTest) {
    Vector_<> s;
    double data[] = {1., 2., 3., 4., 5.};

    s.Append(&data[0], &data[4] + 1);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, VectorAppendByContainerTest) {
    Vector_<> s;
    Vector_<> data = {1., 2., 3., 4., 5.};

    s.Append(data);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, VectorEqualTest) {
    Vector_<> s1;
    Vector_<> s2 = {1., 2., 3., 4., 5.};
    Vector_<> s3 = {1., 2., 3., 4., 5.};

    ASSERT_FALSE(s1 == s2);
    ASSERT_TRUE(s2 == s3);
}

TEST(VectorTest, VectorNotEqualTest) {
    Vector_<> s1;
    Vector_<> s2 = {1., 2., 3., 4., 5.};
    Vector_<> s3 = {1., 2., 3., 4., 5.};

    ASSERT_TRUE(s1 != s2);
    ASSERT_FALSE(s2 != s3);
}

TEST(VectorTest, VectorJoinContiner) {
    Vector_<> s1 = {1., 2., 3.};
    Vector_<> s2 = {4., 5., 6.};

    auto s3 = vector::Join(s1, s2);
    for (auto i = 0; i != 6; ++i) {
        ASSERT_DOUBLE_EQ(s3[i], i + 1.);
    }
}

TEST(VectorTest, VectorUpTo) {
    int n = 6;
    auto s = vector::UpTo(n);

    for (auto i = 0; i != n; ++i) {
        ASSERT_EQ(s[i], i);
    }
}