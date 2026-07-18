//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/model/ivs.hpp>

using namespace Dal;

TEST(ModelTest, TestEmptyRiskViewHasZeroSpread) {
    const AAD::RiskView_<double> risk_view;
    ASSERT_TRUE(risk_view.IsEmpty());
    ASSERT_NEAR(risk_view.Spread(100.0, 1.0), 0.0, 1e-10);
    ASSERT_NEAR(risk_view.Spread(50.0, 5.0), 0.0, 1e-10);
}

TEST(ModelTest, TestRiskViewGridAccessors) {
    const Vector_<> strikes{90.0, 100.0, 110.0};
    const Vector_<> mats{1.0, 2.0};
    const AAD::RiskView_<double> risk_view(strikes, mats);
    ASSERT_FALSE(risk_view.IsEmpty());
    ASSERT_EQ(risk_view.Rows(), 3);
    ASSERT_EQ(risk_view.Cols(), 2);
    ASSERT_EQ(risk_view.Strikes(), strikes);
    ASSERT_EQ(risk_view.Mats(), mats);
}

TEST(ModelTest, TestRiskViewBumpAccumulatesIntoSpreads) {
    AAD::RiskView_<double> risk_view({90.0, 100.0, 110.0}, {1.0, 2.0});
    ASSERT_NEAR(risk_view.Spread(100.0, 2.0), 0.0, 1e-10);

    risk_view.Bump(1, 1, 0.02);
    risk_view.Bump(1, 1, 0.01);
    ASSERT_NEAR(risk_view.Risks()(1, 1), 0.03, 1e-10);
    ASSERT_NEAR(risk_view.Spread(100.0, 2.0), 0.03, 1e-10);
}

TEST(ModelTest, TestRiskViewSpreadInterpolatesBilinearly) {
    AAD::RiskView_<double> risk_view({90.0, 100.0, 110.0}, {1.0, 2.0});
    risk_view.Bump(1, 1, 0.02);

    // on-grid neighbours of the bumped knot stay at zero
    ASSERT_NEAR(risk_view.Spread(100.0, 1.0), 0.0, 1e-10);
    ASSERT_NEAR(risk_view.Spread(90.0, 2.0), 0.0, 1e-10);
    ASSERT_NEAR(risk_view.Spread(110.0, 2.0), 0.0, 1e-10);

    // halfway in both coordinates picks up one quarter of the bumped knot
    ASSERT_NEAR(risk_view.Spread(95.0, 1.5), 0.005, 1e-10);
    ASSERT_NEAR(risk_view.Spread(105.0, 1.5), 0.005, 1e-10);
}
