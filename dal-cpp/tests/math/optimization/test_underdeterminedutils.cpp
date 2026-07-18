//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>

using Dal::DateTime_;
using Dal::Exception_;
using Dal::Vector_;
using Dal::Sparse::TriDiagonal_;

TEST(UnderdeterminedUtilsTest, TestSelfCouplePWCBuildsExpectedWeights) {
    TriDiagonal_ weights(3);
    const Vector_<DateTime_> knots(3);
    Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.5);

    ASSERT_EQ(weights.Size(), 3);
    ASSERT_NEAR(weights(0, 0), 1.0, 1e-10);
    ASSERT_NEAR(weights(1, 1), 1.5, 1e-10);
    ASSERT_NEAR(weights(2, 2), 1.0, 1e-10);
    ASSERT_NEAR(weights(0, 1), -0.5, 1e-10);
    ASSERT_NEAR(weights(1, 0), -0.5, 1e-10);
    ASSERT_NEAR(weights(1, 2), -0.5, 1e-10);
    ASSERT_NEAR(weights(2, 1), -0.5, 1e-10);
}

TEST(UnderdeterminedUtilsTest, TestSelfCouplePWCRespectsOffset) {
    TriDiagonal_ weights(4);
    const Vector_<DateTime_> knots(2);
    Dal::Underdetermined::SelfCouplePWC(&weights, knots, 1.0, 2);

    ASSERT_NEAR(weights(0, 0), 0.0, 1e-10);
    ASSERT_NEAR(weights(1, 1), 0.0, 1e-10);
    // each of the two knots couples to the other, so both diagonal entries are 2 * tau
    ASSERT_NEAR(weights(2, 2), 2.0, 1e-10);
    ASSERT_NEAR(weights(3, 3), 2.0, 1e-10);
    ASSERT_NEAR(weights(2, 3), -1.0, 1e-10);
    ASSERT_NEAR(weights(3, 2), -1.0, 1e-10);
}

TEST(UnderdeterminedUtilsTest, TestSelfCouplePWCAccumulatesIntoExistingWeights) {
    TriDiagonal_ weights(2);
    const Vector_<DateTime_> knots(2);
    Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.5);
    Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.5);

    ASSERT_NEAR(weights(0, 0), 2.0, 1e-10);
    ASSERT_NEAR(weights(1, 1), 2.0, 1e-10);
    ASSERT_NEAR(weights(0, 1), -1.0, 1e-10);
    ASSERT_NEAR(weights(1, 0), -1.0, 1e-10);
}

TEST(UnderdeterminedUtilsTest, TestSelfCouplePWCRejectsInvalidInputs) {
    const Vector_<DateTime_> knots(2);
    {
        ASSERT_THROW(Dal::Underdetermined::SelfCouplePWC(nullptr, knots, 0.5), Exception_);
    }
    {
        TriDiagonal_ weights(3);
        ASSERT_THROW(Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.5, -1), Exception_);
    }
    {
        TriDiagonal_ weights(3);
        ASSERT_THROW(Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.0), Exception_);
    }
    {
        // 2 knots at offset 1 need a 3x3 matrix
        TriDiagonal_ weights(2);
        ASSERT_THROW(Dal::Underdetermined::SelfCouplePWC(&weights, knots, 0.5, 1), Exception_);
    }
}

TEST(UnderdeterminedUtilsTest, TestWeightsPWCReturnsSizedTridiagonal) {
    const Vector_<DateTime_> knots(4);
    const std::unique_ptr<TriDiagonal_> weights = Dal::Underdetermined::WeightsPWC(knots, 0.25);

    ASSERT_EQ(weights->Size(), 4);
    ASSERT_NEAR((*weights)(0, 0), 0.5, 1e-10);
    ASSERT_NEAR((*weights)(1, 1), 0.75, 1e-10);
    ASSERT_NEAR((*weights)(2, 2), 0.75, 1e-10);
    ASSERT_NEAR((*weights)(3, 3), 0.5, 1e-10);
    ASSERT_NEAR((*weights)(0, 1), -0.25, 1e-10);
    ASSERT_NEAR((*weights)(3, 2), -0.25, 1e-10);
}
