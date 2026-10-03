//
// Created by wegamekinglc on 2026/10/3.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/simdkernels.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>

using namespace Dal;

#if defined(DAL_TEST_MIXED_SIMD)
namespace Dal::TestSupport {
    double SimdSumSse2(const double* src, size_t n);
    double SimdSumAvx2(const double* src, size_t n);
    double SimdDotSse2(const double* lhs, const double* rhs, size_t n);
    double SimdDotAvx2(const double* lhs, const double* rhs, size_t n);
} // namespace Dal::TestSupport
#endif

namespace {
    // deterministic quasi-random values in [-1, 1]
    double Pattern(size_t ii, size_t salt) {
        const double angle = 0.7 * static_cast<double>(ii + 13 * salt) + 0.3 * static_cast<double>(salt);
        return std::sin(angle) * std::cos(0.31 * static_cast<double>(ii));
    }

    Vector_<> Patterned(size_t n, size_t salt) {
        Vector_<> retVal(n);
        for (size_t ii = 0; ii < n; ++ii)
            retVal[ii] = Pattern(ii, salt);
        return retVal;
    }

    double ScalarDot(const Vector_<>& lhs, const Vector_<>& rhs) {
        double retVal = 0.0;
        for (size_t ii = 0; ii < lhs.size(); ++ii)
            retVal += lhs[ii] * rhs[ii];
        return retVal;
    }

    void AssertNearRelative(double actual, double expected) {
        const double scale = std::max(1.0, std::abs(expected));
        ASSERT_NEAR(actual, expected, 1e-10 * scale);
    }
} // namespace

TEST(SimdKernelsTest, TestDotSizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> lhs = Patterned(n, 1);
        const Vector_<> rhs = Patterned(n, 2);
        AssertNearRelative(Math::Dot(Math::DoubleData(lhs), Math::DoubleData(rhs), n), ScalarDot(lhs, rhs));
    }
}

TEST(SimdKernelsTest, TestSumSizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> src = Patterned(n, 3);
        double expected = 0.0;
        for (size_t ii = 0; ii < n; ++ii)
            expected += src[ii];
        AssertNearRelative(Math::Sum(Math::DoubleData(src), n), expected);
    }
}

TEST(SimdKernelsTest, TestInnerProductFastPaths) {
    const Vector_<> lhs = Patterned(23, 4);
    const Vector_<> rhs = Patterned(23, 5);
    AssertNearRelative(InnerProduct(lhs, rhs), ScalarDot(lhs, rhs));

    Matrix_<> writable(3, 23);
    for (int ir = 0; ir < writable.Rows(); ++ir)
        for (int ic = 0; ic < writable.Cols(); ++ic)
            writable(ir, ic) = Pattern(static_cast<size_t>(ic), static_cast<size_t>(ir + 10));
    const Matrix_<>& m = writable;
    AssertNearRelative(InnerProduct(m.Row(1), rhs), ScalarDot(Vector_<>(m.Row(1)), rhs));
    AssertNearRelative(InnerProduct(lhs, m.Row(2)), ScalarDot(lhs, Vector_<>(m.Row(2))));
    AssertNearRelative(InnerProduct(m.Row(0), m.Row(1)), ScalarDot(Vector_<>(m.Row(0)), Vector_<>(m.Row(1))));
}

TEST(SimdKernelsTest, TestNonContiguousAndIntegerInnerProducts) {
    Matrix_<> m(3, 2);
    for (int row = 0; row < m.Rows(); ++row) {
        m(row, 0) = row + 1;
        m(row, 1) = row + 4;
    }
    const Matrix_<>& cm = m;
    ASSERT_DOUBLE_EQ(InnerProduct(m.Col(0), cm.Col(1)), 32.0);
    const Vector_<int> lhs{1, 2, 3};
    const Vector_<int> rhs{4, 5, 6};
    ASSERT_EQ(InnerProduct(lhs, rhs), 32);
}

TEST(SimdKernelsTest, TestCancellationErrorBound) {
    for (const size_t n : {4U, 8U, 17U, 33U}) {
        SCOPED_TRACE(n);
        Vector_<> values(n, 0.0);
        values[0] = 1e16;
        values[1] = 1.0;
        values[2] = -1e16;
        values[3] = 1.0;
        const Vector_<> ones(n, 1.0);
        const double eps = std::numeric_limits<double>::epsilon();
        const double errorBound = 2.0 * n * eps / (1.0 - 2.0 * n * eps) * (2e16 + 2.0);
        // Cancellation permits large relative error; bound the absolute error by the input norm.
        ASSERT_NEAR(Math::Sum(values.data(), n), 2.0, errorBound);
        ASSERT_NEAR(Math::Dot(values.data(), ones.data(), n), 2.0, errorBound);
    }
}

TEST(SimdKernelsTest, TestDotAcrossMagnitudes) {
    for (const double scale : {1e-150, 1.0, 1e150}) {
        SCOPED_TRACE(scale);
        Vector_<> lhs(17), rhs(17);
        double expected = 0.0;
        for (size_t ii = 0; ii < lhs.size(); ++ii) {
            const double value = static_cast<double>(ii) - 8.0;
            lhs[ii] = scale * value;
            rhs[ii] = value / scale;
            expected += value * value;
        }
        ASSERT_NEAR(Math::Dot(lhs.data(), rhs.data(), lhs.size()), expected, 1e-12 * expected);
    }
}

TEST(SimdKernelsTest, TestAccumulateFastPath) {
    const Vector_<> src = Patterned(46, 6);
    double expected = 0.0;
    for (size_t ii = 0; ii < src.size(); ++ii)
        expected += src[ii];
    AssertNearRelative(Accumulate(src), expected);
}

TEST(SimdKernelsTest, TestAxpySizes) {
    for (const size_t n : {0U, 1U, 2U, 3U, 4U, 5U, 7U, 8U, 9U, 15U, 16U, 17U, 33U, 1000U}) {
        const Vector_<> src = Patterned(n, 7);
        const double scale = 0.37;
        Vector_<> dst = Patterned(n, 8);
        const Vector_<> expected = dst;
        Math::Axpy(scale, Math::DoubleData(src), dst.size() == 0 ? nullptr : &*dst.begin(), n);
        for (size_t ii = 0; ii < n; ++ii)
            ASSERT_DOUBLE_EQ(dst[ii], expected[ii] + scale * src[ii]);
    }
}

TEST(SimdKernelsTest, TestEmptyRanges) {
    const Vector_<> empty;
    ASSERT_DOUBLE_EQ(Math::Dot(nullptr, nullptr, 0), 0.0);
    ASSERT_DOUBLE_EQ(Math::Sum(nullptr, 0), 0.0);
    ASSERT_DOUBLE_EQ(InnerProduct(empty, empty), 0.0);
    ASSERT_DOUBLE_EQ(Accumulate(empty), 0.0);
}

TEST(SimdKernelsTest, TestMutableAndConstRowsUseSameReduction) {
    Matrix_<> m(2, 8);
    const Vector_<> values{1e16, 1.0, -1e16, 1.0, 0.0, 0.0, 0.0, 0.0};
    const Vector_<> ones(8, 1.0);
    for (int col = 0; col < m.Cols(); ++col) {
        m(0, col) = values[col];
        m(1, col) = 1.0;
    }
    const Matrix_<>& cm = m;
    const double expected = Math::Dot(values.data(), ones.data(), values.size());
    ASSERT_DOUBLE_EQ(InnerProduct(m.Row(0), ones), expected);
    ASSERT_DOUBLE_EQ(InnerProduct(ones, m.Row(0)), expected);
    ASSERT_DOUBLE_EQ(InnerProduct(m.Row(0), m.Row(1)), expected);
    ASSERT_DOUBLE_EQ(InnerProduct(m.Row(0), cm.Row(1)), expected);
    ASSERT_DOUBLE_EQ(InnerProduct(cm.Row(0), m.Row(1)), expected);
}

#if defined(DAL_TEST_MIXED_SIMD)
TEST(SimdKernelsTest, TestMixedIsaConsumersUseSameReduction) {
    if (!__builtin_cpu_supports("avx2"))
        GTEST_SKIP() << "AVX2 consumer requires an AVX2 CPU";
    const Vector_<> values{1e16, 1.0, -1e16, 1.0, 0.0, 0.0, 0.0, 0.0};
    const Vector_<> ones(values.size(), 1.0);
    ASSERT_DOUBLE_EQ(TestSupport::SimdSumSse2(values.data(), values.size()), TestSupport::SimdSumAvx2(values.data(), values.size()));
    ASSERT_DOUBLE_EQ(TestSupport::SimdDotSse2(values.data(), ones.data(), values.size()),
                     TestSupport::SimdDotAvx2(values.data(), ones.data(), values.size()));
}
#endif
