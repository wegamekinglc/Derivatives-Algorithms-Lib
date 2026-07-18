//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/model/utilities.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

TEST(ModelTest, TestFillDataLeavesWideSpacedGridUntouched) {
    const Vector_<> original{0.0, 1.0, 2.0};
    const Vector_<> filled = AAD::FillData(original, 10.0);
    ASSERT_EQ(filled, original);
}

TEST(ModelTest, TestFillDataRefinesToMaxSpacing) {
    const Vector_<> original{0.0, 1.0};
    const Vector_<> filled = AAD::FillData(original, 0.3);
    ASSERT_EQ(filled.size(), 5);
    ASSERT_NEAR(filled[0], 0.0, 1e-10);
    ASSERT_NEAR(filled[1], 0.25, 1e-10);
    ASSERT_NEAR(filled[2], 0.5, 1e-10);
    ASSERT_NEAR(filled[3], 0.75, 1e-10);
    ASSERT_NEAR(filled[4], 1.0, 1e-10);
}

TEST(ModelTest, TestFillDataDropsPointsCloserThanMinSpacing) {
    const Vector_<> original{0.0, 1.0};
    // refinement spacing is 0.25; the candidate at 0.75 is within 0.26 of the knot at 1.0
    const Vector_<> filled = AAD::FillData(original, 0.3, 0.26);
    ASSERT_EQ(filled.size(), 4);
    ASSERT_NEAR(filled[0], 0.0, 1e-10);
    ASSERT_NEAR(filled[1], 0.25, 1e-10);
    ASSERT_NEAR(filled[2], 0.5, 1e-10);
    ASSERT_NEAR(filled[3], 1.0, 1e-10);
}

TEST(ModelTest, TestFillDataUnionsAddedPoints) {
    const Vector_<> original{0.0, 1.0, 2.0};
    const Vector_<> added{0.5, 1.5};
    const Vector_<> filled = AAD::FillData(original, 10.0, 0.0, added.begin(), added.end());
    ASSERT_EQ(filled.size(), 5);
    ASSERT_NEAR(filled[0], 0.0, 1e-10);
    ASSERT_NEAR(filled[1], 0.5, 1e-10);
    ASSERT_NEAR(filled[2], 1.0, 1e-10);
    ASSERT_NEAR(filled[3], 1.5, 1e-10);
    ASSERT_NEAR(filled[4], 2.0, 1e-10);
}

TEST(ModelTest, TestFillDataIgnoresAddedPointNearExistingKnot) {
    const Vector_<> original{0.0, 1.0};
    const Vector_<> added{1.005};
    // 1.005 is within minDx of the existing knot at 1.0, so the union keeps only the original knot
    const Vector_<> filled = AAD::FillData(original, 10.0, 0.01, added.begin(), added.end());
    ASSERT_EQ(filled, original);
}
