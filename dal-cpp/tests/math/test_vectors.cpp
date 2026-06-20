//
// Created by Cheng Li on 2017/12/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>

using vector_t = Dal::Vector_<>;

TEST(VectorTest, TestDefaultConstruction) {
    vector_t s;
    ASSERT_TRUE(s.empty());
}

TEST(VectorTest, TestSizedConstruction) {
    const int n = 10;
    vector_t s(n);
    ASSERT_EQ(s.size(), n);
}

TEST(VectorTest, TestSizedFilledConstruction) {
    const int n = 10;
    const double val = 1.;
    vector_t s(n, val);

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, TestContainerConstruction) {
    const int n = 10;
    const double val = 1.;

    std::vector<double> src(n, val);
    vector_t s(src.begin(), src.end());

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, TestInitializedListConstruction) {
    vector_t s = {1., 2., 3., 4., 5.};

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorSwap) {
    vector_t s1 = {1., 2., 3., 4., 5.};
    vector_t s2;
    s1.Swap(&s2);

    ASSERT_TRUE(s1.empty());
    ASSERT_EQ(s2.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s2[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorFill) {
    const int n = 10;
    const double val = 1.;
    vector_t s(n);
    s.Fill(val);

    ASSERT_EQ(s.size(), n);
    for (auto i = 0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}

TEST(VectorTest, TestVectorResize) {
    const int n1 = 5;
    const int n2 = 10;

    vector_t s(n1);
    ASSERT_EQ(s.size(), n1);

    s.Resize(n2);
    ASSERT_EQ(s.size(), n2);
}

TEST(VectorTest, TestVectorSelfMultiply) {
    vector_t s = {1., 2., 3., 4., 5.};
    s *= 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], 3 * (i + 1.));
    }
}

TEST(VectorTest, TestVectorSelfPlus) {
    vector_t s = {1., 2., 3., 4., 5.};
    s += 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], 3 + i + 1.);
    }
}

TEST(VectorTest, TestVectorSelfMinus) {
    vector_t s = {1., 2., 3., 4., 5.};
    s -= 3;

    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1. - 3.);
    }
}

TEST(VectorTest, TestVectorSelfPlusVector) {
    vector_t s1 = {1., 2., 3., 4., 5.};
    vector_t s2 = {5., 4., 3., 2., 1.};
    s1 += s2;

    ASSERT_EQ(s1.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s1[i], 6.);
    }
}

TEST(VectorTest, TestVectorSelfMinusVector) {
    vector_t s1 = {1., 2., 3., 4., 5.};
    vector_t s2 = {5., 4., 3., 2., 1.};
    s1 -= s2;

    ASSERT_EQ(s1.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s1[i], 2 * i - 4.);
    }
}

TEST(VectorTest, TestVectorAssign) {
    vector_t s;
    double data[] = {1., 2., 3., 4., 5.};

    s.Assign(&data[0], &data[4] + 1);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorAppendByIterator) {
    vector_t s;
    double data[] = {1., 2., 3., 4., 5.};

    s.Append(&data[0], &data[4] + 1);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorAppendByContainer) {
    vector_t s;
    vector_t data = {1., 2., 3., 4., 5.};

    s.Append(data);
    ASSERT_EQ(s.size(), 5);
    for (auto i = 0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorEqual) {
    vector_t s1;
    vector_t s2 = {1., 2., 3., 4., 5.};
    vector_t s3 = {1., 2., 3., 4., 5.};

    ASSERT_FALSE(s1 == s2);
    ASSERT_TRUE(s2 == s3);
}

TEST(VectorTest, TestVectorNotEqual) {
    vector_t s1;
    vector_t s2 = {1., 2., 3., 4., 5.};
    vector_t s3 = {1., 2., 3., 4., 5.};

    ASSERT_TRUE(s1 != s2);
    ASSERT_FALSE(s2 != s3);
}

TEST(VectorTest, TestVectorJoinContainer) {
    vector_t s1 = {1., 2., 3.};
    vector_t s2 = {4., 5., 6.};

    auto s3 = Dal::Vector::Join(s1, s2);
    for (auto i = 0; i != 6; ++i) {
        ASSERT_DOUBLE_EQ(s3[i], i + 1.);
    }
}

TEST(VectorTest, TestVectorUpTo) {
    int n = 6;
    auto s = Dal::Vector::UpTo(n);

    for (auto i = 0; i != n; ++i) {
        ASSERT_EQ(s[i], i);
    }
}

TEST(VectorTest, TestVectorMultiply) {
    vector_t s1 = {1., 2., 3.};
    auto s2 = 2.0 * s1;
    auto s3 = s1 * 2.0;

    for(auto i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], 2.0 * s1[i]);
        ASSERT_DOUBLE_EQ(s3[i], 2.0 * s1[i]);
    }
}
