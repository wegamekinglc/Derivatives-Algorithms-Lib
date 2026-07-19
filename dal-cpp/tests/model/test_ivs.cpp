//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/model/ivs.hpp>

#include "../math/aad/models/flat_ivs.hpp"

using namespace Dal;

namespace {
    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    double ReferenceBlackCall(double fwd, double strike, double vol, double mat) {
        const double stdDev = vol * std::sqrt(mat);
        const double dPlus = std::log(fwd / strike) / stdDev + 0.5 * stdDev;
        const double dMinus = dPlus - stdDev;
        return fwd * NormalCdf(dPlus) - strike * NormalCdf(dMinus);
    }
} // namespace

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

TEST(ModelTest, TestFlatIVSCallMatchesBlackClosedForm) {
    const double spot = 100.0;
    const double vol = 0.20;
    const double rate = 0.05;
    const double div = 0.01;
    const AAD::FlatIVS_ ivs(spot, rate, div, vol);

    for (const double strike : {80.0, 100.0, 120.0}) {
        for (const double mat : {0.5, 1.0, 2.0}) {
            const double fwd = spot * std::exp((rate - div) * mat);
            const double expected = std::exp(-rate * mat) * ReferenceBlackCall(fwd, strike, vol, mat);
            ASSERT_NEAR(ivs.Call(strike, mat), expected, 1e-10);
        }
    }
}

TEST(ModelTest, TestFlatIVSCallDefaultsToZeroRates) {
    const AAD::FlatIVS_ ivs(100.0, 0.0, 0.0, 0.25);
    ASSERT_NEAR(ivs.Spot(), 100.0, 1e-14);

    {
        const double expected = ReferenceBlackCall(100.0, 100.0, 0.25, 1.0);
        ASSERT_NEAR(ivs.Call(100.0, 1.0), expected, 1e-10);
    }
    {
        const double expected = ReferenceBlackCall(100.0, 90.0, 0.25, 0.5);
        ASSERT_NEAR(ivs.Call(90.0, 0.5), expected, 1e-10);
    }
}

TEST(ModelTest, TestFlatIVSCallAddsRiskViewSpread) {
    const AAD::FlatIVS_ ivs(100.0, 0.0, 0.0, 0.20);
    AAD::RiskView_<double> risk_view({90.0, 100.0, 110.0}, {1.0, 2.0});
    risk_view.Bump(1, 1, 0.03);

    {
        const double expected = ReferenceBlackCall(100.0, 100.0, 0.23, 2.0);
        ASSERT_NEAR(ivs.Call(100.0, 2.0, &risk_view), expected, 1e-10);
    }
    {
        // neighbouring knots keep the base vol
        const double expected = ReferenceBlackCall(100.0, 100.0, 0.20, 1.0);
        ASSERT_NEAR(ivs.Call(100.0, 1.0, &risk_view), expected, 1e-10);
    }
    {
        // an empty view prices exactly like no view
        const AAD::RiskView_<double> empty_view;
        ASSERT_NEAR(ivs.Call(100.0, 2.0, &empty_view), ivs.Call(100.0, 2.0), 1e-14);
    }
}

TEST(ModelTest, TestFlatIVSLocalVolIsFlat) {
    {
        const AAD::FlatIVS_ ivs(100.0, 0.0, 0.0, 0.25);
        for (const double strike : {80.0, 100.0, 120.0})
            for (const double mat : {0.5, 1.0, 2.0})
                ASSERT_NEAR(ivs.LocalVol(strike, mat), 0.25, 1e-7);
    }
    {
        // deterministic rates do not tilt the flat surface
        const AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.20);
        for (const double strike : {90.0, 110.0})
            for (const double mat : {1.0, 2.0})
                ASSERT_NEAR(ivs.LocalVol(strike, mat), 0.20, 1e-7);
    }
}

TEST(ModelTest, TestMertonIVSZeroIntensityImpliedVolIsFlat) {
    const AAD::MertonIVS_ ivs(100.0, 0.20, 0.0, -0.10, 0.05);
    for (const double strike : {80.0, 100.0, 120.0})
        for (const double mat : {0.5, 1.0, 2.0})
            ASSERT_NEAR(ivs.ImpliedVol(strike, mat), 0.20, 1e-9);
}

TEST(ModelTest, TestMertonIVSZeroIntensityCallMatchesBlack) {
    const AAD::MertonIVS_ ivs(100.0, 0.20, 0.0, -0.10, 0.05);
    for (const double strike : {80.0, 100.0, 120.0})
        for (const double mat : {0.5, 1.0, 2.0}) {
            const double expected = ReferenceBlackCall(100.0, strike, 0.20, mat);
            ASSERT_NEAR(ivs.Call(strike, mat), expected, 1e-8);
        }
}
