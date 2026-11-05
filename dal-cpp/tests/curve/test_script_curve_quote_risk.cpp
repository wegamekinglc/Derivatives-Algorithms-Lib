#include <gtest/gtest.h>

#include <functional>
#include <limits>

#include <dal/curve/curvescript.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/curve/ycconst.hpp>

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 21), MATURITY(2027, 10, 21);
    CurveInstrumentCalibrationSpec_ DepositSpec() {
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = TODAY;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotDates_ = {TODAY, MATURITY};
        spec.valuation_.evaluationDate_ = TODAY;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        spec.market_.routes_ = {{"USD.OIS"}};
        const auto instrument =
            NewDepositScriptInstrument(Deposit_(TODAY, MATURITY, .025, DayBasis_("ACT_360")), {"deposit", Ccy_("USD"), "USD.OIS", 100.});
        const auto quote = NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Rate"));
        spec.observations_ = {NewCalibrationObservation("o", instrument, "ParRate", quote, 1e-10)};
        return spec;
    }
    ScriptCurveQuoteRiskAccuracy_ Accuracy() { return {{1e-12}, 1e-10, 1e-12}; }
    CurveInstrumentCalibrationSpec_ TwoDepositSpec() {
        auto spec = DepositSpec();
        const Date_ later(2028, 10, 23);
        spec.geometry_.knotDates_.push_back(later);
        const auto instrument =
            NewDepositScriptInstrument(Deposit_(TODAY, later, .025, DayBasis_("ACT_360")), {"deposit2", Ccy_("USD"), "USD.OIS", 100.});
        spec.observations_.push_back(NewCalibrationObservation("o2", instrument, "ParRate",
                                                               NewCalibrationMarketQuote("q2", .04, CalibrationValueUnit_::Parse("Rate")), 1e-10));
        return spec;
    }
} // namespace

TEST(ScriptCurveQuoteRiskTest, TestOwnedStructureIsSeparateFromNumericalAvailability) {
    auto spec = DepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.quoteJacobian_.Empty());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics().size(), 1u);
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskNumericsUnavailable");
    ASSERT_EQ(snapshot.QuoteAxis().size(), 1u);
    ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
    ASSERT_EQ(snapshot.QuoteAxis()[0].ordinal_, 0);
    ASSERT_DOUBLE_EQ(snapshot.QuoteAxis()[0].value_, .035);
    ASSERT_EQ(snapshot.QuoteAxis()[0].unit_.Token(), "Rate");
    ASSERT_EQ(snapshot.ParameterAxis().size(), 1u);
    ASSERT_EQ(snapshot.ParameterAxis()[0].id_, "LOG_DISCOUNT_FACTOR:2027-10-21");
    ASSERT_EQ(snapshot.ParameterAxis()[0].unit_, "Dimensionless");
    ASSERT_EQ(snapshot.ParameterAxis()[0].value_, std::optional<double>(fit.parameters_[0]));
    ASSERT_TRUE(snapshot.ValidateContext(spec).Passed());
    const auto identity = snapshot.Identity();
    fit.parameters_[0] = 99.;
    spec.observations_.clear();
    ASSERT_EQ(snapshot.Identity(), identity);
    ASSERT_DOUBLE_EQ(snapshot.QuoteAxis()[0].value_, .035);
    ASSERT_EQ(snapshot.ResolvedSpec()->observations_.size(), 1u);
}

TEST(ScriptCurveQuoteRiskTest, TestRepeatedQuoteIdIsRejectedWithoutChangingCalibration) {
    auto spec = DepositSpec();
    const Date_ later(2028, 10, 23);
    spec.geometry_.knotDates_.push_back(later);
    const auto instrument =
        NewDepositScriptInstrument(Deposit_(TODAY, later, .025, DayBasis_("ACT_360")), {"deposit2", Ccy_("USD"), "USD.OIS", 100.});
    spec.observations_.push_back(NewCalibrationObservation("o2", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-10));
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_EQ(fit.axes_.quotes_.size(), 1u);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskDuplicateQuoteId");
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/context/observations/1/quote/id");
    ASSERT_EQ(snapshot.Diagnostics()[0].quoteId_, "q");
    ASSERT_EQ(snapshot.Diagnostics()[0].observationId_, "o2");
    ASSERT_EQ(snapshot.QuoteAxis().size(), 2u);
}

TEST(ScriptCurveQuoteRiskTest, TestPriceQuoteIsOutsideDecimalRateAndBasisScope) {
    auto spec = DepositSpec();
    spec.observations_[0] = NewCalibrationObservation("o", spec.observations_[0]->Instrument(), "Price",
                                                      NewCalibrationMarketQuote("price", 0., CalibrationValueUnit_::Parse("Price:USD")), 1e-10);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedUnit");
    ASSERT_EQ(snapshot.QuoteAxis()[0].unit_.Token(), "Price:USD");
}

TEST(ScriptCurveQuoteRiskTest, TestFitAxesAreCheckedAgainstResolvedContract) {
    auto fit = CalibrateCurveInstruments(DepositSpec());
    fit.axes_.quoteMap_(0, 0) = 0.;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/axes/quote_map/0/0");
}

TEST(ScriptCurveQuoteRiskTest, TestContextMismatchOwnsPreciseBindingDiagnostic) {
    const auto spec = DepositSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    auto changed = spec;
    const auto instrument =
        BindCalibrationInstrumentParameters(spec.observations_[0]->Instrument(), {{{"K", .031, CalibrationValueUnit_::Parse("Rate")}}, {}});
    changed.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-10);
    const auto validation = snapshot.ValidateContext(changed);
    ASSERT_FALSE(validation.Passed());
    ASSERT_EQ(validation.Diagnostics()[0].field_, "/context/observations/0/binding_identity");
    ASSERT_EQ(validation.Diagnostics()[0].observationId_, "o");
    ASSERT_EQ(validation.Diagnostics()[0].instrumentId_, "deposit");
    ASSERT_EQ(validation.Diagnostics()[0].quoteId_, "q");
    ASSERT_EQ(validation.Axes().quotes_[0].id_, "q");
}

TEST(ScriptCurveQuoteRiskTest, TestFitBooleanCannotPromoteAnEvaluationToCalibration) {
    const auto spec = DepositSpec();
    auto fit = PreparedCurveInstruments_(spec).Evaluate({-.03}, false);
    fit.fitWithinTolerance_ = true;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/report");
}

TEST(ScriptCurveQuoteRiskTest, TestResolvedContractMustMatchOriginalPreparationEvidence) {
    auto spec = DepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    const auto instrument =
        BindCalibrationInstrumentParameters(spec.observations_[0]->Instrument(), {{{"K", .031, CalibrationValueUnit_::Parse("Rate")}}, {}});
    spec.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-10);
    fit.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(spec);
    fit.axes_ = BuildCalibrationObservationAxes(spec.observations_);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/report/plans/0");
}

TEST(ScriptCurveQuoteRiskTest, TestEveryContextComponentInvalidatesSnapshot) {
    const auto spec = DepositSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    using Change_ = std::function<void(CurveInstrumentCalibrationSpec_*)>;
    const Vector_<Change_> changes = {
        [](auto* s) { s->geometry_.today_ = TODAY.AddDays(1); },
        [](auto* s) { s->geometry_.ccy_ = "EUR"; },
        [](auto* s) { s->geometry_.curveName_ = "other"; },
        [](auto* s) { s->geometry_.knotDates_[1] = MATURITY.AddDays(1); },
        [](auto* s) { s->geometry_.parameterization_ = CurveParameterization_::Value_::ZERO_RATE; },
        [](auto* s) { s->geometry_.logDfScheme_ = LogDfScheme_::Value_::LOG_CUBIC_NATURAL; },
        [](auto* s) { s->geometry_.baseCurve_.reset(new Tape::DiscountPWC_<double>("base", "USD", {MATURITY}, {.01})); },
        [](auto* s) { s->geometry_.calibrateDiscountCurve_ = false; },
        [](auto* s) { s->geometry_.targetCollateral_ = CollateralType_::Value_::GC; },
        [](auto* s) { s->geometry_.targetTenor_ = PeriodLength_("3M"); },
        [](auto* s) { s->geometry_.liborBasis_ = DayBasis_("ACT_360"); },
        [](auto* s) { s->valuation_.evaluationDate_ = TODAY.AddDays(1); },
        [](auto* s) { s->valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL; },
        [](auto* s) { s->valuation_.fixings_.reset(new MarketFixingSnapshot_({{"index", {{DateTime_(TODAY, .25), .04}}}})); },
        [](auto* s) { s->market_.routes_[0].id_ = "usd.ois"; },
        [](auto* s) { s->market_.routes_[0].projection_ = true; },
        [](auto* s) { s->market_.routes_[0].collateral_ = CollateralType_::Value_::GC; },
        [](auto* s) { s->market_.routes_[0].tenor_ = PeriodLength_("3M"); },
        [](auto* s) {
            s->market_.discountCurves_[CollateralType_::Value_::OIS].reset(new Tape::DiscountPWC_<double>("bg", "USD", {MATURITY}, {.01}));
        },
        [](auto* s) { s->market_.forwardCurves_[PeriodLength_("3M")].reset(new Tape::DiscountPWC_<double>("bg", "USD", {MATURITY}, {.01})); },
        [](auto* s) { s->solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE; },
        [](auto* s) { s->solver_.initialGuess_ = .06; },
        [](auto* s) { s->solver_.initialGuessPerNode_ = {-.04}; },
        [](auto* s) { s->solver_.parameterScales_ = {.3}; },
        [](auto* s) { s->solver_.smoothingWeight_ = .2; },
        [](auto* s) { s->solver_.maxEvaluations_ = 201; },
        [](auto* s) { s->solver_.maxRestarts_ = 21; },
        [](auto* s) { s->solver_.fitWeight_ = 1.; },
        [](auto* s) { s->solver_.fitNormTolerance_ = .01; }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = spec;
        changes[i](&changed);
        const auto validation = snapshot.ValidateContext(changed);
        ASSERT_FALSE(validation.Passed());
        ASSERT_EQ(validation.Diagnostics().size(), 1u);
        ASSERT_EQ(validation.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_NE(validation.Diagnostics()[0].field_, "/context");
        ASSERT_EQ(validation.Axes().quotes_[0].id_, "q");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestNonFiniteFitValuesAreUnavailable) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    for (const bool parameter : {true, false}) {
        auto changed = fit;
        (parameter ? changed.parameters_[0] : changed.modelMeasures_[0]) = std::numeric_limits<double>::infinity();
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskNonFinite");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, parameter ? "/fit/parameters/0" : "/fit/model_measures/0");
        ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestMalformedAccuracyUsesOwnedCalibrationError) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(ScriptCurveQuoteRiskAccuracy_*)>;
    const Vector_<Change_> changes = {[](auto* a) { a->residualAbsoluteLimits_.clear(); },
                                      [](auto* a) { a->residualAbsoluteLimits_[0] = -1.; },
                                      [](auto* a) { a->residualAbsoluteLimits_[0] = std::numeric_limits<double>::quiet_NaN(); },
                                      [](auto* a) { a->minReciprocalCondition_ = 0.; },
                                      [](auto* a) { a->minReciprocalCondition_ = 1.1; },
                                      [](auto* a) { a->responseBackwardErrorLimit_ = std::numeric_limits<double>::infinity(); },
                                      [](auto* a) { a->relativePivotTolerance_ = 1.; }};
    for (const auto& change : changes) {
        auto accuracy = Accuracy();
        change(&accuracy);
        ASSERT_THROW(NewScriptCurveQuoteRiskSnapshot(fit, accuracy), CalibrationError_);
    }
    std::optional<CalibrationDiagnostic_> retained;
    {
        auto accuracy = Accuracy();
        accuracy.residualAbsoluteLimits_[0] = -1.;
        try {
            static_cast<void>(NewScriptCurveQuoteRiskSnapshot(fit, accuracy));
        } catch (const CalibrationError_& error) {
            retained = error.Failure().diagnostic_;
        }
    }
    ASSERT_TRUE(retained);
    ASSERT_EQ(retained->field_, "/accuracy/residual_absolute_limits/0");
    ASSERT_EQ(retained->code_, "InvalidScriptCurveQuoteRiskInput");
}

TEST(ScriptCurveQuoteRiskTest, TestMalformedFitAndContextUseCalibrationError) {
    ASSERT_THROW(NewScriptCurveQuoteRiskSnapshot({}, Accuracy()), CalibrationError_);
    auto spec = DepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    fit.solveMode_ = CurveSolveMode_();
    ASSERT_THROW(NewScriptCurveQuoteRiskSnapshot(fit, Accuracy()), CalibrationError_);
    fit = CalibrateCurveInstruments(spec);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    spec.valuation_.fixings_.reset();
    ASSERT_THROW(static_cast<void>(snapshot.ValidateContext(spec)), CalibrationError_);
    spec = DepositSpec();
    spec.solver_.initialGuess_ = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(static_cast<void>(snapshot.ValidateContext(spec)), CalibrationError_);
}

TEST(ScriptCurveQuoteRiskTest, TestSnapshotOwnsMutableCurveAndResolvedSpecAliases) {
    auto spec = DepositSpec();
    auto background = std::make_shared<Tape::DiscountPWC_<double>>("background", "USD", Vector_<Date_>{MATURITY}, Vector_<>{.01});
    spec.market_.forwardCurves_[PeriodLength_("3M")] = Handle_<DiscountCurve_>(background);
    spec.market_.discountCurves_[CollateralType_::Value_::GC] = Handle_<DiscountCurve_>(background);
    spec.geometry_.baseCurve_ = Handle_<DiscountCurve_>(background);
    auto fit = CalibrateCurveInstruments(spec);
    auto alias = std::make_shared<CurveInstrumentCalibrationSpec_>(*fit.resolvedSpec_);
    fit.resolvedSpec_ = alias;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    const auto frozen = snapshot.ResolvedSpec()->market_.forwardCurves_.at(PeriodLength_("3M"));
    const auto expected = (*frozen)(TODAY, MATURITY);
    ASSERT_NE(frozen.get(), background.get());
    background->ApplyDX(Vector_<>{.02}.begin(), 1.);
    alias->market_.routes_.clear();
    ASSERT_DOUBLE_EQ((*frozen)(TODAY, MATURITY), expected);
    ASSERT_DOUBLE_EQ((*snapshot.ResolvedSpec()->geometry_.baseCurve_)(TODAY, MATURITY), expected);
    ASSERT_DOUBLE_EQ((*snapshot.ResolvedSpec()->market_.discountCurves_.at(CollateralType_::Value_::GC))(TODAY, MATURITY), expected);
    ASSERT_EQ(snapshot.ResolvedSpec()->market_.routes_.size(), 1u);
    ASSERT_FALSE(snapshot.ValidateContext(spec).Passed());
}

TEST(ScriptCurveQuoteRiskTest, TestApproximateAndBothNonSquareShapesAreRejected) {
    {
        auto spec = DepositSpec();
        spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        spec.solver_.fitWeight_ = 1.;
        spec.solver_.fitNormTolerance_ = .01;
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedMode");
    }
    for (const bool overdetermined : {true, false}) {
        auto spec = DepositSpec();
        if (overdetermined)
            spec.observations_.push_back(NewCalibrationObservation("o2", spec.observations_[0]->Instrument(), "ParRate",
                                                                   NewCalibrationMarketQuote("q2", .035, CalibrationValueUnit_::Parse("Rate")),
                                                                   1e-10));
        else
            spec.geometry_.knotDates_.push_back(Date_(2028, 10, 23));
        auto accuracy = Accuracy();
        accuracy.residualAbsoluteLimits_ = Vector_<>(spec.observations_.size(), 1e-12);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), accuracy);
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskNonSquare");
        ASSERT_EQ(snapshot.ObservationAxis().size(), overdetermined ? 2u : 1u);
        ASSERT_EQ(snapshot.ParameterAxis().size(), overdetermined ? 1u : 2u);
    }
}

TEST(ScriptCurveQuoteRiskTest, TestObservationSourceBindingQuoteAndOrderInvalidateSnapshot) {
    const auto spec = TwoDepositSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.StructureValid());
    const auto& row = *spec.observations_[0];
    const auto& instrument = row.Instrument();
    auto source = instrument->Definition()->Spec();
    source.source_.rows_.back().text_ += " ";
    const auto changedSource = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    const Vector_<Handle_<CalibrationObservation_>> changes = {
        NewCalibrationObservation("O", instrument, "ParRate", row.Quote(), row.ErrorScale()),
        NewCalibrationObservation("o", NewScriptCalibrationInstrument({"Deposit", instrument->Definition(), {}, instrument->Pillar()}), "ParRate",
                                  row.Quote(), row.ErrorScale()),
        NewCalibrationObservation("o", changedSource, "ParRate", row.Quote(), row.ErrorScale()),
        NewCalibrationObservation("o", BindCalibrationInstrumentParameters(instrument, {{{"N", 10., CalibrationValueUnit_::Parse("Price:USD")}}, {}}),
                                  "ParRate", row.Quote(), row.ErrorScale()),
        NewCalibrationObservation("o", instrument, "ParRate", NewCalibrationMarketQuote("Q", .035, CalibrationValueUnit_::Parse("Rate")),
                                  row.ErrorScale()),
        NewCalibrationObservation("o", instrument, "ParRate", NewCalibrationMarketQuote("q", .036, CalibrationValueUnit_::Parse("Rate")),
                                  row.ErrorScale()),
        NewCalibrationObservation("o", instrument, "ParRate", row.Quote(), 1e-8),
        NewCalibrationObservation("o", instrument, "Price", NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Price:USD")),
                                  row.ErrorScale())};
    for (const auto& change : changes) {
        auto changed = spec;
        changed.observations_[0] = change;
        const auto result = snapshot.ValidateContext(changed);
        ASSERT_FALSE(result.Passed());
        ASSERT_EQ(result.Diagnostics()[0].observationId_, "o");
        ASSERT_EQ(result.Diagnostics()[0].quoteId_, "q");
        ASSERT_EQ(result.Axes().quotes_[0].id_, "q");
    }
    auto reordered = spec;
    std::swap(reordered.observations_[0], reordered.observations_[1]);
    ASSERT_FALSE(snapshot.ValidateContext(reordered).Passed());
    const auto rebuilt = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(reordered), {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(rebuilt.StructureValid());
    ASSERT_EQ(rebuilt.QuoteAxis()[0].id_, "q2");
    ASSERT_EQ(rebuilt.QuoteAxis()[0].ordinal_, 0);
    ASSERT_EQ(rebuilt.ObservationAxis()[1].id_, "o");
    ASSERT_EQ(rebuilt.ParameterAxis()[0].id_, snapshot.ParameterAxis()[0].id_);
}

TEST(ScriptCurveQuoteRiskTest, TestConflictingQuoteIdsAndCaseSensitiveDistinctIds) {
    auto spec = TwoDepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    spec.observations_[1] = NewCalibrationObservation("o2", spec.observations_[1]->Instrument(), "ParRate",
                                                      NewCalibrationMarketQuote("q", .04, CalibrationValueUnit_::Parse("Rate")), 1e-10);
    fit.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(spec);
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_FALSE(rejected.StructureValid());
    ASSERT_EQ(rejected.Diagnostics()[0].code_, "ScriptCurveQuoteRiskDuplicateQuoteId");
    ASSERT_EQ(rejected.Diagnostics()[0].quoteId_, "q");
    ASSERT_DOUBLE_EQ(rejected.QuoteAxis()[1].value_, .04);
    spec.observations_[1] = NewCalibrationObservation("o2", spec.observations_[1]->Instrument(), "ParRate",
                                                      NewCalibrationMarketQuote("Q", .04, CalibrationValueUnit_::Parse("Rate")), 1e-10);
    const auto distinct = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(distinct.StructureValid());
    ASSERT_EQ(std::string(distinct.QuoteAxis()[1].id_.c_str()), "Q");
}

TEST(ScriptCurveQuoteRiskTest, TestFitLayoutAxisCandidateAndCurveTamperingIsRejected) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(CurveInstrumentCalibrationResult_*)>;
    const Vector_<Change_> changes = {[](auto* f) { f->parameters_[0] += .001; },
                                      [](auto* f) { f->definition_.dayCount_ = DayBasis_("ACT_360"); },
                                      [](auto* f) { f->freeParameters_[0].date_ = MATURITY.AddDays(1); },
                                      [](auto* f) { f->freeParameters_[0].component_ = CurveFreeParameterComponent_::Value_::ZERO_RATE; },
                                      [](auto* f) { f->parameters_.clear(); },
                                      [](auto* f) { f->axes_.quotes_.clear(); },
                                      [](auto* f) { f->axes_.quoteIndex_[0] = 1; },
                                      [](auto* f) { f->axes_.quoteMap_.Resize(0, 0); },
                                      [](auto* f) { f->candidateIdentity_ = "other"; },
                                      [](auto* f) { f->planIdentity_ = "other"; },
                                      [](auto* f) { f->curve_.reset(new Tape::DiscountPWC_<double>("other", "USD", {MATURITY}, {.01})); }};
    for (const auto& change : changes) {
        auto changed = fit;
        change(&changed);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestSnapshotAndRejectionSurviveAllOriginalLifetimes) {
    const auto retained = [] {
        auto spec = DepositSpec();
        auto fit = CalibrateCurveInstruments(spec);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        spec.market_.routes_.clear();
        return std::make_pair(snapshot, snapshot.ValidateContext(spec));
    }();
    ASSERT_TRUE(retained.first.StructureValid());
    ASSERT_FALSE(retained.second.Passed());
    ASSERT_EQ(retained.second.Axes().quotes_[0].id_, "q");
    ASSERT_EQ(retained.first.ObservationAxis()[0].instrumentId_, "deposit");
    ASSERT_EQ(retained.first.ResolvedSpec()->market_.routes_[0].id_, "USD.OIS");
    ASSERT_EQ(retained.second.Diagnostics()[0].field_, "/context/market/routes");
    ASSERT_TRUE(retained.second.Diagnostics()[0].expected_);
    ASSERT_TRUE(retained.second.Diagnostics()[0].actual_);
}

TEST(ScriptCurveQuoteRiskTest, TestUnsupportedScriptRetainsSourceDiagnostic) {
    auto spec = DepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    auto definition = spec.observations_[0]->Instrument()->Definition()->Spec();
    definition.source_.rows_.back().text_ = "PAY PAYS N*MAX(1+K*ALPHA,0)";
    const auto instrument = NewScriptCalibrationInstrument({"unsupported", NewScriptInstrumentDefinition(definition), {}, MATURITY});
    spec.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-10);
    fit.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(spec);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].instrumentId_, "unsupported");
    ASSERT_TRUE(snapshot.Diagnostics()[0].sourceRow_);
    ASSERT_TRUE(snapshot.Diagnostics()[0].field_);
    ASSERT_EQ(snapshot.ObservationAxis()[0].instrumentId_, "unsupported");
}

TEST(ScriptCurveQuoteRiskTest, TestFourNativeLayoutsHaveOwnedAxesWithoutClaimingRank) {
    for (const auto mode : {CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::ZERO_RATE,
                            CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD}) {
        auto spec = DepositSpec();
        spec.geometry_.parameterization_ = mode;
        if (mode != CurveParameterization_::Value_::LOG_DISCOUNT)
            spec.geometry_.knotDates_ = {MATURITY};
        const auto instrument = spec.observations_[0]->Instrument();
        spec.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), .1);
        if (mode == CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD)
            spec.observations_.push_back(NewCalibrationObservation("o2", instrument, "ParRate",
                                                                   NewCalibrationMarketQuote("q2", .035, CalibrationValueUnit_::Parse("Rate")), .1));
        const auto fit = CalibrateCurveInstruments(spec);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {Vector_<>(spec.observations_.size(), 1e-12), 1e-10, 1e-12});
        ASSERT_TRUE(snapshot.StructureValid());
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.ParameterAxis().size(), fit.freeParameters_.size());
        for (size_t j = 0; j < fit.freeParameters_.size(); ++j) {
            ASSERT_EQ(snapshot.ParameterAxis()[j].coordinate_.component_, fit.freeParameters_[j].component_);
            ASSERT_EQ(snapshot.ParameterAxis()[j].unit_, mode == CurveParameterization_::Value_::LOG_DISCOUNT ? "Dimensionless" : "Rate");
            ASSERT_EQ(snapshot.ParameterAxis()[j].ordinal_, static_cast<int>(j));
            ASSERT_EQ(snapshot.ParameterAxis()[j].value_, std::optional<double>(fit.parameters_[j]));
        }
    }
}

TEST(ScriptCurveQuoteRiskTest, TestDecimalBasisSpreadIsStructurallyAdmitted) {
    RateIndexConvention_ index;
    index.dayBasis_ = DayBasis_("ACT_360");
    index.businessDayConvention_ = BizDayConvention_("Unadjusted");
    RateLegConvention_ leg;
    leg.paymentFrequency_ = PeriodLength_("3M");
    leg.dayBasis_ = DayBasis_("ACT_360");
    leg.businessDayConvention_ = leg.paymentConvention_ = BizDayConvention_("Unadjusted");
    const Date_ start(2027, 10, 21), end(2028, 10, 21);
    const BasisSwap_ native(TODAY, start, end, -.001, index, leg, index, leg);
    const auto instrument = NewBasisSwapScriptInstrument(native, {"basis", Ccy_("USD"), "USD.OIS", 100.});
    auto spec = DepositSpec();
    spec.geometry_.knotDates_ = {TODAY, end};
    spec.observations_ = {NewCalibrationObservation("basis-row", instrument, "BasisSpread",
                                                    NewCalibrationMarketQuote("basis-q", 0., CalibrationValueUnit_::Parse("BasisSpread")), 1e-10)};
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.QuoteAxis()[0].unit_.Token(), "BasisSpread");
    ASSERT_EQ(snapshot.QuoteAxis()[0].convention_.id_, CalibrationQuoteConventionId_::Value_::DecimalSpread);
}

TEST(ScriptCurveQuoteRiskTest, TestFailedFitSnapshotOutlivesCalibrationException) {
    std::optional<ScriptCurveQuoteRiskSnapshot_> snapshot;
    {
        auto spec = DepositSpec();
        spec.solver_.maxEvaluations_ = 1;
        try {
            static_cast<void>(CalibrateCurveInstruments(spec));
        } catch (const CalibrationError_& error) {
            const auto candidate = handle_cast<CurveInstrumentCalibrationCandidate_>(error.Failure().candidate_);
            ASSERT_TRUE(candidate);
            snapshot = NewScriptCurveQuoteRiskSnapshot(candidate->Result(), Accuracy());
        }
    }
    ASSERT_TRUE(snapshot);
    ASSERT_FALSE(snapshot->StructureValid());
    ASSERT_EQ(snapshot->Diagnostics()[0].code_, "ScriptCurveQuoteRiskFitNotAccepted");
    ASSERT_EQ(snapshot->ObservationAxis()[0].id_, "o");
    ASSERT_EQ(snapshot->QuoteAxis()[0].id_, "q");
}

TEST(ScriptCurveQuoteRiskTest, TestActualPlanIdentityCoversQuotesAndUnusedBackgroundCurves) {
    auto spec = DepositSpec();
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.differenceStep_ = 2e-5;
    options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
    options.preparationCache_ = std::make_shared<CalibrationPreparationCache_>();
    auto fit = CalibrateCurveInstruments(spec, options);
    ASSERT_TRUE(fit.resolvedOptions_);
    ASSERT_FALSE(fit.resolvedOptions_->preparationCache_);
    ASSERT_TRUE(NewScriptCurveQuoteRiskSnapshot(fit, Accuracy()).StructureValid());
    for (const bool background : {true, false}) {
        auto changed = spec;
        if (background)
            changed.market_.forwardCurves_[PeriodLength_("3M")].reset(new Tape::DiscountPWC_<double>("unused", "USD", {MATURITY}, {.01}));
        else
            changed.observations_[0] = NewCalibrationObservation("o", spec.observations_[0]->Instrument(), "ParRate",
                                                                 NewCalibrationMarketQuote("q", .036, CalibrationValueUnit_::Parse("Rate")), 1e-10);
        auto inconsistent = fit;
        inconsistent.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(changed);
        inconsistent.axes_ = BuildCalibrationObservationAxes(changed.observations_);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(inconsistent, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestUnsupportedCalibrationDomainRetainsAxes) {
    auto fit = CalibrateCurveInstruments(DepositSpec());
    auto report = fit.report_->Data();
    report.summary_.domain_ = "Xccy";
    fit.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report));
    fit.resolvedSpec_.reset();
    fit.resolvedOptions_.reset();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedDomain");
    ASSERT_EQ(snapshot.Diagnostics()[0].actual_, "Xccy");
    ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
    ASSERT_EQ(snapshot.ParameterAxis()[0].unit_, "Dimensionless");
}

TEST(ScriptCurveQuoteRiskTest, TestInvalidQuoteOrdinalDiagnosticPreservesSignedInput) {
    auto fit = CalibrateCurveInstruments(DepositSpec());
    fit.axes_.quoteIndex_[0] = -1;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/axes/quote_index/0");
    ASSERT_EQ(snapshot.Diagnostics()[0].actual_, "-1");
}
