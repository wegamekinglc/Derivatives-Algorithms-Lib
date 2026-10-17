//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <algorithm>

#include <dal/model/dupire.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/platform/platform.hpp>

#include "../math/aad/models/flat_ivs.hpp"
#include "dupireriskinputs.hpp"

using Dal::CalibrateDupireWithRisk;
using Dal::Matrix_;
using Dal::Test::SmallRiskInputs;

TEST(DupireRiskTest, TestCheckedNumericSnapshotPreservesDiscreteCalibration) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const auto inputs = SmallRiskInputs();
    const auto snapshot = CalibrateDupireWithRisk(ivs, inputs, "checked");
    Dal::AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
    const auto legacy =
        Dal::AAD::DupireCalib(ivs, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
    ASSERT_EQ(snapshot.Spot(), 100.0);
    ASSERT_EQ(snapshot.Rate(), 0.05);
    ASSERT_EQ(snapshot.DividendYield(), 0.02);
    ASSERT_EQ(snapshot.Surface()->spots_, legacy.spots_);
    ASSERT_EQ(snapshot.Surface()->times_, legacy.times_);
    ASSERT_EQ(snapshot.Surface()->vols_.Rows(), legacy.lVols_.Rows());
    ASSERT_EQ(snapshot.Surface()->vols_.Cols(), legacy.lVols_.Cols());
    for (int row = 0; row < legacy.lVols_.Rows(); ++row)
        for (int column = 0; column < legacy.lVols_.Cols(); ++column) {
            ASSERT_EQ(snapshot.Surface()->vols_(row, column), legacy.lVols_(row, column));
            ASSERT_NEAR(snapshot.Surface()->vols_(row, column), 0.2, 3e-6);
        }
    ASSERT_EQ(snapshot.Inputs().quoteSpreads_.Rows(), 3);
    ASSERT_EQ(snapshot.Inputs().quoteSpreads_.Cols(), 2);
}

TEST(DupireRiskTest, TestNonzeroQuoteSnapshotPreservesLegacyValuesAndCompleteIdentity) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    auto inputs = SmallRiskInputs();
    inputs.quoteSpreads_.Fill(0.0015);
    const auto snapshot = CalibrateDupireWithRisk(ivs, inputs);
    const auto independentlyNamed = CalibrateDupireWithRisk(ivs, inputs, "another name");
    ASSERT_TRUE(snapshot.Matches(independentlyNamed));
    Dal::AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
    for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
        for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column)
            quotes.Bump(row, column, 0.0015);
    const auto legacy =
        Dal::AAD::DupireCalib(ivs, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
    ASSERT_TRUE(std::equal(legacy.lVols_.begin(), legacy.lVols_.end(), snapshot.Surface()->vols_.begin()));
    const Dal::DupireParameterAdjoints_ seeds{snapshot, Matrix_<>(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), 0.5)};
    inputs.quoteSpreads_.Fill(0.002);
    const auto changedQuotes = CalibrateDupireWithRisk(ivs, inputs);
    ASSERT_FALSE(snapshot.Matches(changedQuotes));
    ASSERT_THROW(Dal::PullbackDupireCalibration(changedQuotes, seeds), Dal::Exception_);
    const Dal::AAD::FlatIVS_ differentBase(100.0, 0.05, 0.02, 0.25);
    ASSERT_THROW(Dal::PullbackDupireCalibration(CalibrateDupireWithRisk(differentBase, snapshot.Inputs()), seeds), Dal::Exception_);
}
