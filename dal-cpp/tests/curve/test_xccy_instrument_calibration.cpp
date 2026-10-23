#include <gtest/gtest.h>

#include <limits>

#include "xccyinstrumentfixture.hpp"

using namespace Dal;
using namespace Dal::TestXccy;

TEST(XccyInstrumentCalibrationTest, TestNativeImportFitsBasis) {
    const auto spec = Spec();
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.computeQuoteJacobian_ = true;
    const auto result = CalibrateXccyInstruments(spec, options);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_NEAR(result.parameters_[0], .003, 1e-10);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 1);
    ASSERT_EQ(result.report_->Data().summary_.domain_, "Xccy");
    ASSERT_EQ(result.report_->Axes().observations_[0]->Id(), "basis-row");
    ASSERT_FALSE(result.report_->QuoteRiskAvailable());
}

TEST(XccyInstrumentCalibrationTest, TestResetAndMtmFitKnownBasis) {
    for (const auto mode : {XccyNotionalMode_::Value_::RESETTABLE, XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        auto spec = Spec(mode);
        spec.solver_.parameterScales_ = {.3};
        const auto fit = CalibrateXccyInstruments(spec);
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_NEAR(fit.parameters_[0], .003, 1e-10);
        ASSERT_EQ(fit.xccySpec_->context_.fixings_.get(), spec.context_.fixings_.get());
    }
}

TEST(XccyInstrumentCalibrationTest, TestRawMixedAxesAndCandidateRefresh) {
    auto spec = Spec();
    const auto row = spec.observations_[0];
    const auto price = NewCalibrationMarketQuote("price", -.22000103820950934, CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_ = {NewCalibrationObservation("price-row", row->Instrument(), "Price", price, 1e-7), row,
                          NewCalibrationObservation("shared-basis-row", row->Instrument(), "BasisSpread", row->Quote(), 2e-10)};
    spec.knotDates_ = {Date_(2027, 8, 4), END};
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const PreparedXccyInstruments_ aad(spec, options);
    options.jacobianMode_ = CalibrationDerivative_::Value_::FiniteDifference;
    options.differenceStep_ = 1e-5;
    const PreparedXccyInstruments_ fd(spec, options);
    const auto value = aad.Evaluate({.003, .003}), bumped = aad.Evaluate({.004, .002});
    const auto finite = fd.Evaluate({.003, .003});
    ASSERT_NEAR(value.modelMeasures_[0], price->Value(), 1e-10);
    ASSERT_EQ(value.axes_.quotes_.size(), 2);
    ASSERT_EQ(value.axes_.quoteIndex_, (Vector_<int>{0, 1, 1}));
    ASSERT_DOUBLE_EQ(value.axes_.quoteMap_(2, 1), 1.);
    ASSERT_EQ(value.quoteJacobian_.Rows(), 3);
    ASSERT_EQ(value.quoteJacobian_.Cols(), 2);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(value.quoteJacobian_(i, j), finite.quoteJacobian_(i, j), 1e-8);
    ASSERT_NE(value.candidateIdentity_, bumped.candidateIdentity_);
    ASSERT_NE(value.modelMeasures_[0], bumped.modelMeasures_[0]);
    ASSERT_FALSE(value.report_->Data().summary_.rawL2_);
    ASSERT_EQ(row->Quote()->Fingerprint(), spec.observations_[2]->Quote()->Fingerprint());
    ASSERT_DOUBLE_EQ(value.quoteJacobian_(1, 0), value.quoteJacobian_(2, 0));
}

TEST(XccyInstrumentCalibrationTest, TestBudgetFailureOwnsValidatedCandidate) {
    Handle_<CalibrationReport_> report;
    Handle_<Storable_> candidate;
    {
        auto spec = Spec();
        spec.solver_.maxEvaluations_ = 1;
        try {
            static_cast<void>(CalibrateXccyInstruments(spec));
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
            report = error.Failure().report_;
            candidate = error.Failure().candidate_;
        }
    }
    ASSERT_TRUE(report);
    ASSERT_EQ(report->Axes().observations_[0]->Id(), "basis-row");
    const auto typed = handle_cast<XccyInstrumentCalibrationCandidate_>(candidate);
    ASSERT_TRUE(typed);
    ASSERT_TRUE(typed->Result().curve_);
    ASSERT_EQ(typed->Result().report_.get(), report.get());
    ASSERT_FALSE(typed->Result().fitWithinTolerance_);
}

TEST(XccyInstrumentCalibrationTest, TestFiniteDifferenceSolverHonorsEvaluationBudget) {
    auto spec = Spec();
    spec.solver_.maxEvaluations_ = 2;
    try {
        static_cast<void>(CalibrateXccyInstruments(spec));
        FAIL() << "FD perturbations must consume the existing Fit evaluation budget";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        const auto candidate = handle_cast<XccyInstrumentCalibrationCandidate_>(error.Failure().candidate_);
        ASSERT_TRUE(candidate);
        ASSERT_EQ(candidate->Result().evaluations_, 2);
        ASSERT_EQ(candidate->Result().gradients_, 1);
        ASSERT_DOUBLE_EQ(candidate->Result().parameters_[0], 0.);
    }
}

TEST(XccyInstrumentCalibrationTest, TestFinalJacobianFailureOwnsCompletedCandidate) {
    Handle_<CalibrationReport_> report;
    Handle_<XccyInstrumentCalibrationCandidate_> candidate;
    CalibrationDiagnostic_ expected;
    Vector_<> measures, residuals;
    String_ planIdentity;
    {
        auto spec = Spec();
        spec.solver_.initialGuess_ = .003;
        CurveInstrumentCalibrationOptions_ options;
        options.differenceStep_ = 1e-30;
        const auto plain = CalibrateXccyInstruments(spec, options);
        ASSERT_TRUE(plain.fitWithinTolerance_);
        measures = plain.modelMeasures_;
        residuals = plain.rawResiduals_;
        planIdentity = plain.planIdentity_;
        options.computeQuoteJacobian_ = true;
        try {
            static_cast<void>(PreparedXccyInstruments_(spec, options).Evaluate({.003}));
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_TRUE(error.Failure().candidate_);
            expected = error.Failure().diagnostic_;
        }
        try {
            static_cast<void>(CalibrateXccyInstruments(spec, options));
            FAIL();
        } catch (const CalibrationError_& error) {
            const auto& d = error.Failure().diagnostic_;
            ASSERT_EQ(d.code_, expected.code_);
            ASSERT_EQ(d.stage_, expected.stage_);
            ASSERT_EQ(d.field_, expected.field_);
            ASSERT_EQ(d.message_, expected.message_);
            ASSERT_EQ(d.parameterKey_, expected.parameterKey_);
            report = error.Failure().report_;
            candidate = handle_cast<XccyInstrumentCalibrationCandidate_>(error.Failure().candidate_);
        }
    }
    ASSERT_TRUE(candidate);
    const auto& result = candidate->Result();
    ASSERT_EQ(result.report_.get(), report.get());
    ASSERT_EQ(result.parameters_, (Vector_<>{.003}));
    ASSERT_EQ(result.modelMeasures_, measures);
    ASSERT_EQ(result.rawResiduals_, residuals);
    ASSERT_EQ(result.planIdentity_, planIdentity);
    ASSERT_FALSE(result.candidateIdentity_.empty());
    ASSERT_FALSE(result.objectiveIdentity_.empty());
    ASSERT_EQ(result.evaluations_, 1);
    ASSERT_EQ(result.gradients_, 0);
    ASSERT_EQ(result.diagnosticEvaluations_, 1);
    ASSERT_EQ(result.diagnosticGradients_, 1);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 0);
    ASSERT_FALSE(report->Data().quoteJacobian_);
    ASSERT_EQ(report->Data().parameters_[0].value_, .003);
    ASSERT_EQ(report->Data().observations_[0].modelValue_, measures[0]);
    ASSERT_EQ(report->Data().observations_[0].rawResidual_, residuals[0]);
    ASSERT_EQ(report->Axes().quoteIndex_, (Vector_<int>{0}));
    ASSERT_EQ(report->Data().summary_.evaluations_, 1);
    ASSERT_EQ(report->Data().summary_.jacobianEvaluations_, 0);
    ASSERT_EQ(report->Stage(), expected.stage_);
    ASSERT_TRUE(report->Data().execution_.fixingSnapshotIdentity_);
    ASSERT_EQ(report->Data().execution_.routeIdentities_.size(), 2);
}

TEST(XccyInstrumentCalibrationTest, TestApproximateRequiresIndependentControls) {
    auto spec = Spec();
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    try {
        static_cast<void>(CalibrateXccyInstruments(spec));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationObjective");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/solver/fit_weight");
    }
}

TEST(XccyInstrumentCalibrationTest, TestContextErrorsAndMcRefusalOwnAxes) {
    for (int scenario = 0; scenario < 3; ++scenario) {
        auto spec = Spec();
        CurveInstrumentCalibrationOptions_ options;
        const char* code = "UnsupportedCalibrationFixingTime";
        if (scenario == 0)
            spec.context_.valuationTime_ = DateTime_(TODAY, 9, 0);
        if (scenario == 1) {
            spec.context_.collateralCurrency_ = Ccy_("EUR");
            code = "CalibrationContextMismatch";
        }
        if (scenario == 2) {
            options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
            code = "MCCrossCurrencyUnsupported";
        }
        try {
            static_cast<void>(CalibrateXccyInstruments(spec, options));
            FAIL();
        } catch (const CalibrationError_& error) {
            ASSERT_EQ(error.Failure().diagnostic_.code_, code);
            ASSERT_EQ(error.Failure().report_->Axes().quoteIndex_, (Vector_<int>{0}));
            ASSERT_FALSE(error.Failure().candidate_);
        }
    }
}

TEST(XccyInstrumentCalibrationTest, TestHistoricalResetAndMtmUseExactSnapshot) {
    for (const auto mode : {XccyNotionalMode_::Value_::RESETTABLE, XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        auto spec = Spec(mode);
        spec.context_.valuationTime_ = DateTime_(Date_(2027, 8, 5));
        const auto config = Config(mode);
        const auto plan = BuildXccyCashflowPlan(START, END, config);
        MarketFixingSnapshot_::values_t values;
        for (const auto& r : RequiredHistoricalFixings(plan, spec.context_.valuationTime_))
            values[r.indexName_][r.fixingTime_] = r.indexName_.substr(0, 2) == "FX" ? 1.12 : .031;
        spec.context_.fixings_.reset(new MarketFixingSnapshot_(values));
        CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.context_.valuationTime_, Ccy_("USD"),
                                    spec.context_.fixings_);
        market.SetBasisCurve(Flat("USD", .003));
        const CrossCurrencySwap_ native(TODAY, START, END, .001, config);
        const auto quote = NewCalibrationMarketQuote("historical", (*native.Precompute())(market), CalibrationValueUnit_::Parse("BasisSpread"));
        spec.observations_ = {NewCalibrationObservation("historical-row", spec.observations_[0]->Instrument(), "BasisSpread", quote, 1e-10)};
        const auto result = CalibrateXccyInstruments(spec);
        ASSERT_NEAR(result.parameters_[0], .003, 1e-10);
        spec.context_.fixings_.reset(new MarketFixingSnapshot_());
        ASSERT_THROW(CalibrateXccyInstruments(spec), CalibrationError_);
    }
}

TEST(XccyInstrumentCalibrationTest, TestApproximateMixedUnitsAndNonuniformMetric) {
    auto spec = Spec();
    const auto row = spec.observations_[0];
    const auto price = NewCalibrationMarketQuote("price", -.22000103820950934, CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_ = {NewCalibrationObservation("price-row", row->Instrument(), "Price", price, 1e-4),
                          NewCalibrationObservation("spread-row", row->Instrument(), "BasisSpread", row->Quote(), 1e-6)};
    spec.knotDates_ = {Date_(2027, 8, 4), END};
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.initialGuessPerNode_ = {.001, .002};
    spec.solver_.parameterScales_ = {.1, .3};
    spec.solver_.smoothingWeight_ = .1;
    spec.solver_.fitWeight_ = 1e5;
    spec.solver_.fitNormTolerance_ = 1e-3;
    const auto result = CalibrateXccyInstruments(spec);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_LE(result.scaledNorm_, 1e-3);
    ASSERT_NEAR(result.weights_(0, 0), .002, 1e-12);
    ASSERT_NEAR(result.weights_(0, 1), -.003, 1e-12);
    ASSERT_DOUBLE_EQ(result.weights_(0, 1), result.weights_(1, 0));
    ASSERT_NEAR(result.weights_(1, 1), .018, 1e-12);
    const double d0 = result.parameters_[0] - .001, d1 = result.parameters_[1] - .002;
    const double expectedPenalty = .1 * (d0 * d0 - d0 * d1 + d1 * d1);
    double data = 0.;
    for (double residual : result.scaledResiduals_)
        data += .5e5 * residual * residual;
    ASSERT_NEAR(*result.report_->Data().summary_.penaltyObjective_, expectedPenalty, 1e-14);
    ASSERT_NEAR(*result.report_->Data().summary_.objective_, expectedPenalty + data, 1e-14);
    ASSERT_EQ(result.initialParameters_, (Vector_<>{.001, .002}));
    ASSERT_FALSE(result.report_->Data().summary_.rawL2_);
}

TEST(XccyInstrumentCalibrationTest, TestInvalidInitialValuePreservesPrimaryError) {
    auto spec = Spec();
    spec.solver_.initialGuess_ = std::numeric_limits<double>::infinity();
    try {
        static_cast<void>(CalibrateXccyInstruments(spec));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationObjective");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/solver/initial_guess");
        ASSERT_FALSE(error.Failure().candidate_);
        ASSERT_FALSE(error.Failure().report_->Data().parameters_[0].initial_);
    }
}

TEST(XccyInstrumentCalibrationTest, TestReportObjectiveAndExecution) {
    auto spec = Spec();
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 1e5;
    spec.solver_.fitNormTolerance_ = 1e-3;
    const auto result = CalibrateXccyInstruments(spec);
    const auto& data = result.report_->Data();
    ASSERT_TRUE(data.summary_.objective_);
    ASSERT_TRUE(data.summary_.dataObjective_);
    ASSERT_TRUE(data.summary_.penaltyObjective_);
    ASSERT_NEAR(*data.summary_.objective_, *data.summary_.dataObjective_ + *data.summary_.penaltyObjective_, 1e-12);
    ASSERT_TRUE(data.execution_.fixingSnapshotIdentity_);
    ASSERT_EQ(data.execution_.routeIdentities_.size(), 2);
    ASSERT_FALSE(result.objectiveIdentity_.empty());
}

TEST(XccyInstrumentCalibrationTest, TestReportPreservesFiniteZeroAndUnavailableOverflow) {
    auto spec = Spec();
    const auto row = spec.observations_[0];
    const double initial = PreparedXccyInstruments_(spec).Evaluate({0.}, false).modelMeasures_[0];
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 1.;
    spec.solver_.maxEvaluations_ = 1;
    for (const bool overflow : {false, true}) {
        spec.solver_.fitNormTolerance_ = overflow ? 1e201 : 1e-3;
        const auto quote = NewCalibrationMarketQuote("initial", overflow ? 1e200 : initial, row->Quote()->Unit());
        spec.observations_ = {NewCalibrationObservation("row", row->Instrument(), row->MeasureId(), quote, 1.)};
        const auto result = CalibrateXccyInstruments(spec);
        ASSERT_TRUE(result.fitWithinTolerance_);
        ASSERT_EQ(result.evaluations_, 1);
        const auto& summary = result.report_->Data().summary_;
        ASSERT_EQ(summary.penaltyObjective_, 0.);
        if (overflow) {
            ASSERT_FALSE(summary.dataObjective_);
            ASSERT_FALSE(summary.objective_);
        } else {
            ASSERT_EQ(summary.dataObjective_, 0.);
            ASSERT_EQ(summary.objective_, 0.);
        }
    }
}

TEST(XccyInstrumentCalibrationTest, TestSpreadLegAndOrientationKeepBasisDirection) {
    for (bool foreign : {false, true}) {
        for (bool receive : {false, true}) {
            auto spec = Spec();
            auto config = Config();
            config.convention_.spreadOnForeignLeg_ = foreign;
            const CrossCurrencySwap_ native(TODAY, START, END, .001, config);
            const auto instrument = NewXccyScriptInstrument(native, {"direction", "USD.OIS", receive, 1e-12});
            CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.context_.valuationTime_, Ccy_("USD"),
                                        spec.context_.fixings_);
            market.SetBasisCurve(Flat("USD", .003));
            const auto quote = NewCalibrationMarketQuote("direction", (*native.Precompute())(market), CalibrationValueUnit_::Parse("BasisSpread"));
            spec.observations_ = {NewCalibrationObservation("row", instrument, "BasisSpread", quote, 1e-10)};
            const auto result = CalibrateXccyInstruments(spec);
            ASSERT_NEAR(result.parameters_[0], .003, 1e-10);
        }
    }
}

TEST(XccyInstrumentCalibrationTest, TestNonrootNativeOracleAndDifferenceRefinement) {
    for (const auto mode : {XccyNotionalMode_::Value_::FIXED, XccyNotionalMode_::Value_::RESETTABLE, XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        auto spec = Spec(mode);
        const auto row = spec.observations_[0];
        const auto quote = NewCalibrationMarketQuote("pv", 0., CalibrationValueUnit_::Parse("Price:USD"));
        spec.observations_.push_back(NewCalibrationObservation("pv-row", row->Instrument(), "Price", quote, 1e-7));
        spec.knotDates_ = {Date_(2027, 8, 4), END};
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const PreparedXccyInstruments_ plan(spec, options);
        const Vector_<> theta{.004, .002};
        const auto value = plan.Evaluate(theta);
        CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.context_.valuationTime_, Ccy_("USD"),
                                    spec.context_.fixings_);
        market.SetBasisCurve(value.curve_);
        const CrossCurrencySwap_ native(TODAY, START, END, .001, Config(mode));
        ASSERT_NEAR(value.modelMeasures_[0], (*native.Precompute())(market), 1e-12);
        options.jacobianMode_ = CalibrationDerivative_::Value_::FiniteDifference;
        for (double h : {1e-5, 5e-6}) {
            options.differenceStep_ = h;
            const auto fd = PreparedXccyInstruments_(spec, options).Evaluate(theta);
            for (int i = 0; i < 2; ++i)
                for (int j = 0; j < 2; ++j)
                    ASSERT_NEAR(value.quoteJacobian_(i, j), fd.quoteJacobian_(i, j), 1e-8);
        }
        const auto fingerprint = row->Quote()->Fingerprint();
        spec.observations_[0] = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), row->Quote(), 2e-10);
        const auto rescaled = PreparedXccyInstruments_(spec).Evaluate(theta);
        ASSERT_EQ(spec.observations_[0]->Quote()->Fingerprint(), fingerprint);
        ASSERT_NE(value.planIdentity_, rescaled.planIdentity_);
        ASSERT_DOUBLE_EQ(value.modelMeasures_[0], rescaled.modelMeasures_[0]);
    }
}
