//
// Created by Codex on 2026/10/16.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <dal/math/simdkernels.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace {
    void AssertSame(double actual, double expected) {
        if (std::isnan(expected))
            ASSERT_TRUE(std::isnan(actual));
        else
            ASSERT_EQ(std::memcmp(&actual, &expected, sizeof(double)), 0);
    }
} // namespace

TEST(InverseNormalBlockTest, TestScalarBuildRoundingPolicy) {
    // Rounded Acklam Horner stages; the native FMA build fuses each stage.
#if defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__)) || defined(__ARM_FEATURE_FMA)
    AssertSame(Dal::InverseNCDF(0.1, false, false), -0x1.4813c3681e9fap+0);
    AssertSame(Dal::InverseNCDF(0.02425, false, false), -0x1.f913f9ae1a1bcp+0);
#else
    AssertSame(Dal::InverseNCDF(0.1, false, false), -0x1.4813c3681e9f4p+0);
    AssertSame(Dal::InverseNCDF(0.02425, false, false), -0x1.f913f9ae19f39p+0);
#endif
}

TEST(InverseNormalBlockTest, TestBoundariesAndSizes) {
    const std::vector<double> inputs{0.0,
                                     1.0,
                                     std::numeric_limits<double>::denorm_min(),
                                     std::nextafter(1.0, 0.0),
                                     1e-100,
                                     1e-12,
                                     0.02425,
                                     std::nextafter(0.02425, 0.0),
                                     std::nextafter(0.02425, 1.0),
                                     0.97575,
                                     std::nextafter(0.97575, 0.0),
                                     std::nextafter(0.97575, 1.0),
                                     0.5,
                                     std::nextafter(0.5, 0.0),
                                     std::nextafter(0.5, 1.0),
                                     0.1,
                                     0.9};
    Dal::Math::InverseNCDFBlock(nullptr, 0);
    std::vector<size_t> sizes{31, 32, 33, 34, 35, 52, 53, 366, 367};
    for (size_t size = 0; size <= inputs.size(); ++size)
        sizes.push_back(size);
    for (const bool precise : {false, true}) {
        for (const bool polish : {false, true}) {
            for (const size_t size : sizes) {
                const size_t capacity = std::max(size, inputs.size());
                std::vector<double> values(capacity + 2, -7.0);
                for (size_t i = 0; i < capacity; ++i)
                    values[i + 1] = inputs[i % inputs.size()];
                Dal::Math::InverseNCDFBlock(values.data() + 1, size, precise, polish);
                AssertSame(values.front(), -7.0);
                AssertSame(values.back(), -7.0);
                for (size_t i = 0; i < size; ++i)
                    AssertSame(values[i + 1], Dal::InverseNCDF(inputs[i % inputs.size()], precise, polish));
                for (size_t i = size; i < capacity; ++i)
                    AssertSame(values[i + 1], inputs[i % inputs.size()]);
            }
        }
    }
}

TEST(InverseNormalBlockTest, TestDenseCentralAndTailBitPatterns) {
    std::vector<double> values(1000003);
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = (static_cast<double>(i) + 0.5) / values.size();
    auto expected = values;
    for (auto& value : expected)
        value = Dal::InverseNCDF(value, false, false);
    Dal::Math::InverseNCDFBlock(values.data(), values.size());
    ASSERT_EQ(std::memcmp(values.data(), expected.data(), values.size() * sizeof(double)), 0);
}

TEST(InverseNormalBlockTest, TestInvalidLanesThrow) {
    for (const double invalid : {-1.0, std::nextafter(0.0, -1.0), std::nextafter(1.0, 2.0), std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        for (size_t lane = 0; lane < 37; ++lane) {
            std::vector<double> values(37, 0.4);
            values[lane] = invalid;
            ASSERT_THROW(Dal::Math::InverseNCDFBlock(values.data(), values.size()), Dal::Exception_);
        }
    }
}

TEST(InverseNormalBlockTest, TestVectorAndRemainderBoundaries) {
    for (const size_t size : {31, 32, 33, 34, 35, 52, 53, 366, 367}) {
        std::vector<double> values(size + 2, -7.0);
        for (size_t i = 0; i < size; ++i)
            values[i + 1] = (static_cast<double>(i) + 0.5) / size;
        values[1] = 0.02425;
        values[2] = std::nextafter(0.02425, 1.0);
        values[3] = 1.0 - 0.02425;
        values[4] = std::nextafter(1.0 - 0.02425, 0.0);
        values[5] = std::nextafter(0.5, 0.0);
        values[6] = std::nextafter(0.5, 1.0);
        const auto expected = values;
        Dal::Math::InverseNCDFBlock(values.data() + 1, size);
        AssertSame(values.front(), expected.front());
        AssertSame(values.back(), expected.back());
        for (size_t i = 0; i < size; ++i)
            AssertSame(values[i + 1], Dal::InverseNCDF(expected[i + 1], false, false));
    }
}
