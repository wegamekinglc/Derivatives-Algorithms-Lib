#include <gtest/gtest.h>

#include <dal-public/src/scriptcurvequoterisk.hpp>

#include "scriptquoteriskfixture.hpp"

using namespace Dal;

TEST(ScriptCurveQuoteRiskPublicTest, TestOrderedOwningRiskAndDecimalUnits) {
    const auto result = [] {
        const auto spec = Dal::Test::ScriptQuoteRiskSpec();
        const auto fit = CalibrateCurveInstruments(spec);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
        REQUIRE(snapshot.RiskAvailable(), "fixture must admit risk");
        return ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec, {{"last", "first"}});
    }();
    ASSERT_TRUE(result.Available());
    ASSERT_EQ(result.Currency(), Ccy_("USD"));
    const double accrual = 365. / 360., denominator = 1. + .035 * accrual;
    ASSERT_NEAR(result.PV(), -100. + 100. * (1. + .025 * accrual) / denominator, 1e-10);
    ASSERT_NEAR(result.RawSensitivity()[0], 0., 1e-10);
    ASSERT_NEAR(result.RawSensitivity()[1], -100. * (1. + .025 * accrual) * accrual / (denominator * denominator), 1e-8);
    ASSERT_DOUBLE_EQ(result.DV01()[1], result.RawSensitivity()[1] * 1e-4);
    const auto axis = ScriptCurveQuoteRiskTable(result, "QuoteAxis");
    ASSERT_EQ(axis.Rows(), 3);
    ASSERT_EQ(Cell::ToDouble(axis(1, 0)), 1.);
    ASSERT_EQ(Cell::ToString(axis(1, 1)), "last");
    ASSERT_EQ(Cell::ToString(axis(2, 3)), "Rate");
    ASSERT_EQ(Cell::ToString(axis(2, 4)), "DecimalRate");
    ASSERT_EQ(ScriptCurveQuoteRiskTable(result, "Diagnostics").Rows(), 1);
}

TEST(ScriptCurveQuoteRiskPublicTest, TestStaleAndMalformedContextsHaveDistinctOwningFailures) {
    auto spec = Dal::Test::ScriptQuoteRiskSpec();
    const auto fit = CalibrateCurveInstruments(spec);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
    auto stale = spec;
    stale.solver_.maxEvaluations_ += 1;
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, stale);
    ASSERT_FALSE(result.Available());
    ASSERT_THROW(static_cast<void>(result.PV()), Exception_);
    const auto summary = ScriptCurveQuoteRiskTable(result, "Summary");
    ASSERT_TRUE(Cell::IsEmpty(summary(3, 1)));
    const auto sensitivities = ScriptCurveQuoteRiskTable(result, "Sensitivity");
    ASSERT_EQ(sensitivities.Rows(), 3);
    ASSERT_TRUE(Cell::IsEmpty(sensitivities(1, 4)));
    ASSERT_TRUE(Cell::IsEmpty(sensitivities(1, 5)));
    const auto diagnostics = ScriptCurveQuoteRiskTable(result, "Diagnostics");
    ASSERT_EQ(diagnostics.Rows(), 2);
    ASSERT_EQ(Cell::ToString(diagnostics(1, 0)), "ScriptCurveQuoteRiskIdentityMismatch");
    spec.valuation_.fixings_.reset();
    try {
        static_cast<void>(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec));
        FAIL() << "malformed context must throw";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidScriptCurveQuoteRiskInput");
    }
}

TEST(ScriptCurveQuoteRiskPublicTest, TestSnapshotProjectsObservationAndParameterAxes) {
    const auto spec = Dal::Test::ScriptQuoteRiskSpec();
    const auto fit = CalibrateCurveInstruments(spec);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
    const auto observations = ScriptCurveQuoteRiskTable(snapshot, "ObservationAxis");
    const auto parameters = ScriptCurveQuoteRiskTable(snapshot, "ParameterAxis");
    ASSERT_EQ(observations.Rows(), 3);
    ASSERT_EQ(Cell::ToString(observations(1, 1)), "first");
    ASSERT_EQ(Cell::ToDouble(observations(2, 2)), 1.);
    ASSERT_EQ(parameters.Rows(), 3);
    ASSERT_EQ(Cell::ToString(parameters(1, 2)), "Dimensionless");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(parameters(1, 3)), fit.parameters_[0]);
}

TEST(ScriptCurveQuoteRiskPublicTest, TestBasisSpreadAxisUsesDecimalSpreadAndSignedDv01) {
    const auto spec = Dal::Test::ScriptBasisQuoteRiskSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), {{1e-12}, 1e-10, 1e-12});
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec);
    ASSERT_TRUE(result.Available());
    const auto quotes = ScriptCurveQuoteRiskTable(result, "QuoteAxis");
    ASSERT_EQ(Cell::ToString(quotes(1, 3)), "BasisSpread");
    ASSERT_EQ(Cell::ToString(quotes(1, 4)), "DecimalSpread");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(quotes(1, 2)), -.001);
    ASSERT_NE(result.RawSensitivity()[0], 0.);
    ASSERT_DOUBLE_EQ(result.DV01()[0], result.RawSensitivity()[0] * 1e-4);
}
