//
// Created by wegamekinglc on 2026/10/3.
//

#include <cmath>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/simdkernels.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>
#include <gtest/gtest.h>

using namespace Dal;

namespace {
    // deterministic quasi-random values in [-1, 1]
    double Pattern(size_t ii, size_t salt) {
        const double angle = 0.7 * static_cast<double>(ii + 13 * salt) + 0.3 * static_cast<double>(salt);
        return std::sin(angle) * std::cos(0.31 * static_cast<double>(ii));
    }

    Vector_<> Patterned(size_t n, size_t salt) {
        Vector_<> ret_val(n);
        for (size_t ii = 0; ii < n; ++ii)
            ret_val[ii] = Pattern(ii, salt);
        return ret_val;
    }

    double ScalarDot(const Vector_<>& lhs, const Vector_<>& rhs) {
        double ret_val = 0.0;
        for (size_t ii = 0; ii < lhs.size(); ++ii)
            ret_val += lhs[ii] * rhs[ii];
        return ret_val;
    }

    void ExpectNearRelative(double actual, double expected) {
        const double scale = std::max(1.0, std::abs(expected));
        EXPECT_NEAR(actual, expected, 1e-10 * scale);
    }
} // namespace

TEST(SimdKernelsTest, TestDotSizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> lhs = Patterned(n, 1);
        const Vector_<> rhs = Patterned(n, 2);
        ExpectNearRelative(Math::Dot(Math::DoubleData(lhs), Math::DoubleData(rhs), n), ScalarDot(lhs, rhs));
    }
}

TEST(SimdKernelsTest, TestSumSizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> src = Patterned(n, 3);
        double expected = 0.0;
        for (size_t ii = 0; ii < n; ++ii)
            expected += src[ii];
        ExpectNearRelative(Math::Sum(Math::DoubleData(src), n), expected);
    }
}

TEST(SimdKernelsTest, TestInnerProductFastPaths) {
    const Vector_<> lhs = Patterned(23, 4);
    const Vector_<> rhs = Patterned(23, 5);
    ExpectNearRelative(InnerProduct(lhs, rhs), ScalarDot(lhs, rhs));

    Matrix_<> m(3, 23);
    for (int ir = 0; ir < m.Rows(); ++ir)
        for (int ic = 0; ic < m.Cols(); ++ic)
            m(ir, ic) = Pattern(static_cast<size_t>(ic), static_cast<size_t>(ir + 10));
    ExpectNearRelative(InnerProduct(m.Row(1), rhs), ScalarDot(Vector_<>(m.Row(1)), rhs));
    ExpectNearRelative(InnerProduct(lhs, m.Row(2)), ScalarDot(lhs, Vector_<>(m.Row(2))));
    ExpectNearRelative(InnerProduct(m.Row(0), m.Row(1)), ScalarDot(Vector_<>(m.Row(0)), Vector_<>(m.Row(1))));
}

TEST(SimdKernelsTest, TestAccumulateFastPath) {
    const Vector_<> src = Patterned(46, 6);
    double expected = 0.0;
    for (size_t ii = 0; ii < src.size(); ++ii)
        expected += src[ii];
    ExpectNearRelative(Accumulate(src), expected);
}

TEST(SimdKernelsTest, TestAxpySizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> src = Patterned(n, 7);
        const double scale = 0.37;
        Vector_<> dst = Patterned(n, 8);
        const Vector_<> expected = dst;
        Math::Axpy(scale, Math::DoubleData(src), dst.size() == 0 ? nullptr : &*dst.begin(), n);
        for (size_t ii = 0; ii < n; ++ii)
            EXPECT_DOUBLE_EQ(dst[ii], expected[ii] + scale * src[ii]);
    }
}

TEST(SimdKernelsTest, TestEmptyRanges) {
    const Vector_<> empty;
    EXPECT_DOUBLE_EQ(Math::Dot(nullptr, nullptr, 0), 0.0);
    EXPECT_DOUBLE_EQ(Math::Sum(nullptr, 0), 0.0);
    EXPECT_DOUBLE_EQ(InnerProduct(empty, empty), 0.0);
    EXPECT_DOUBLE_EQ(Accumulate(empty), 0.0);
}
