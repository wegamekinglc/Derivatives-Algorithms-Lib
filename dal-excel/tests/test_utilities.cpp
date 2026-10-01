//
// Created by Codex on 2026/10/1.
//

#include <gtest/gtest.h>

#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__utilities_test_api.hpp>

TEST(ExcelUtilitiesTest, TestCalendarFunctions) {
    Dal::Excel::ScriptTestInitialize(1);
    const Dal::Date_ saturday(2026, 1, 31);
    bool businessDay = true;
    Dal::Is_BizDay("", saturday, &businessDay);
    ASSERT_FALSE(businessDay);
    Dal::Date_ result;
    Dal::Next_BizDay("", saturday, &result);
    ASSERT_EQ(Dal::Date_(2026, 2, 2), result);
    Dal::Prev_BizDay("", saturday, &result);
    ASSERT_EQ(Dal::Date_(2026, 1, 30), result);
    Dal::Adjust_Date("", saturday, "ModifiedFollowing", &result);
    ASSERT_EQ(Dal::Date_(2026, 1, 30), result);
    int count = -1;
    Dal::Count_BusDays("", Dal::Date_(2026, 1, 30), Dal::Date_(2026, 2, 2), &count);
    ASSERT_EQ(1, count);
}

TEST(ExcelUtilitiesTest, TestInterpolationFactoriesAndReusableOutput) {
    const Dal::Vector_<> x{0.0, 1.0, 2.0};
    const Dal::Vector_<> y{1.0, 3.0, 5.0};
    Dal::Handle_<Dal::Interp1_> interpolator;
    Dal::Vector_<> values;
    Dal::Interp1_New_Linear("linear", x, y, &interpolator);
    Dal::Interp1_Get(interpolator, {0.5, 1.5}, &values);
    ASSERT_DOUBLE_EQ(2.0, values[0]);
    ASSERT_DOUBLE_EQ(4.0, values[1]);
    const double* storage = &values[0];
    Dal::Interp1_Get(interpolator, {1.0, 2.0}, &values);
    ASSERT_EQ(storage, &values[0]);
    ASSERT_DOUBLE_EQ(3.0, values[0]);
    Dal::Interp1_New_Linear_Smoothed("smoothed", x, y, 1.0, {}, &interpolator);
    Dal::Interp1_Get(interpolator, {0.5}, &values);
    ASSERT_NEAR(7.0 / 3.0, values[0], 1e-10);
    Dal::Interp1_New_Cubic("cubic", x, y, {1}, {2.0}, &interpolator);
    Dal::Interp1_Get(interpolator, {1.5}, &values);
    ASSERT_NEAR(4.0, values[0], 1e-10);
}

TEST(ExcelUtilitiesTest, TestBilinearEvaluationAndNullHandle) {
    Dal::Matrix_<> values(2, 2);
    values(0, 0) = 1.0;
    values(0, 1) = 4.0;
    values(1, 0) = 3.0;
    values(1, 1) = 6.0;
    Dal::Handle_<Dal::Interp2_> interpolator;
    Dal::Interp2_New_Linear("plane", {0.0, 1.0}, {0.0, 1.0}, values, &interpolator);
    Dal::Interp2_Get(interpolator, {0.25, 0.75}, {0.5}, &values);
    ASSERT_EQ(2, values.Rows());
    ASSERT_EQ(1, values.Cols());
    ASSERT_DOUBLE_EQ(3.0, values(0, 0));
    ASSERT_DOUBLE_EQ(4.0, values(1, 0));
    ASSERT_THROW(Dal::Interp2_Get({}, {0.0}, {0.0}, &values), Dal::Exception_);
}
