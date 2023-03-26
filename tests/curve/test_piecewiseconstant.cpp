//
// Created by wegam on 2023/3/26.
//

#include <gtest/gtest.h>
#include <dal/curve/piecewiseconstant.hpp>

using namespace Dal;


TEST(CurveTest, TestPiecewiseConstant) {
    Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    Vector_<> right = {1.0, 2.0, 3.0};

    PiecewiseConstant_ pwc(knots, right);

    Date_ d(2022, 6, 26);
    ASSERT_NEAR(pwc.IntegralTo(d), 549.0, 1e-8);
}

TEST(CurveTest, TestIntegral) {
    Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26), Date_(2023, 3, 26)};
    Vector_<> right = {1.0, 2.0, 3.0};

    PiecewiseConstant_ pwc(knots, right);

    Date_ from(2022, 6, 26);
    Date_ to(2023, 6, 26);
    ASSERT_NEAR(PWC::Integral(pwc, from, to), 822, 1e-8);
}