#include <gtest/gtest.h>

#include <functional>
#include <iostream>

#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>

#include "compoundedoiscalibrationfixture.hpp"

using namespace Dal;
using namespace Dal::CompoundedOisCalibrationTest;

namespace {
    CurveInstrumentCalibrationOptions_ Options() {
        CurveInstrumentCalibrationOptions_ result;
        result.jacobianMode_ = CalibrationDerivative_("AAD");
        return result;
    }
    ScriptCurveQuoteRiskAccuracy_ Accuracy(bool pvOnly = false) { return {{1e-12, 1e-12}, 1e-10, 1e-12, 1e-12, pvOnly}; }
    Vector_<> PassiveParameters(const String_& role) {
        return role == "Discount" ? Vector_<>{-.0002, -.02} : CompoundedOisTest::BackgroundParameters();
    }
    CurveInstrumentCalibrationSpec_ RiskSpec(const String_& mode, const String_& role, bool mixed = false, bool zeroRate = false) {
        auto spec = Spec(mode, role, mixed, zeroRate);
        if (role == "Discount") {
            // Separate coupon forwards enough to make 1bp quote bumps local in the discount coordinates.
            const auto background = PassiveParameters(role);
            spec.market_.overnightCurves_["USD.SOFR"].reset(
                new DiscountLogDF_("passive", "USD", {Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 20)}, {0., background[0], background[1]},
                                   DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
            const auto row = spec.observations_[1];
            const double value = Oracle(spec.compoundedOis_[1]->Spec(), role, Parameters(zeroRate), zeroRate, spec.valuation_, background)[1];
            spec.observations_[1] =
                NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(),
                                          NewCalibrationMarketQuote(row->Quote()->Id(), value, row->Quote()->Unit()), row->ErrorScale());
        }
        return spec;
    }
    CurveInstrumentCalibrationSpec_ Bumped(CurveInstrumentCalibrationSpec_ spec, int column, double bump) {
        const auto row = spec.observations_[column];
        const auto quote = NewCalibrationMarketQuote(row->Quote()->Id(), row->Quote()->Value() + bump, row->Quote()->Unit());
        spec.observations_[column] = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), quote, row->ErrorScale());
        return spec;
    }
    void ReplaceTable(CurveInstrumentCalibrationSpec_* spec, size_t index, const CompoundedOisScriptSpec_& contract) {
        const auto id = spec->compoundedOis_[index]->Spec().instrumentId_;
        const auto tables = PrepareCompoundedOis(contract, spec->valuation_);
        spec->compoundedOis_[index] = tables;
        for (auto& row : spec->observations_)
            if (row->Instrument()->Id() == id)
                row =
                    NewCalibrationObservation(row->Id(), NewCompoundedOisScriptInstrument(tables), row->MeasureId(), row->Quote(), row->ErrorScale());
    }
    void RebuildValuation(CurveInstrumentCalibrationSpec_* spec) {
        for (size_t i = 0; i < spec->compoundedOis_.size(); ++i)
            ReplaceTable(spec, i, spec->compoundedOis_[i]->Spec());
    }
    void AssertParRateRejected(const CompoundedOisParRateResolution_& result, const String_& code) {
        ASSERT_FALSE(result.Available());
        ASSERT_TRUE(result.quotes_.empty());
        ASSERT_EQ(result.diagnostics_.size(), 1u);
        ASSERT_EQ(result.diagnostics_.front().code_, code);
    }
    void AssertUnavailable(const ScriptCurveQuoteRiskResult_& result) {
        ASSERT_FALSE(result.Available());
        ASSERT_FALSE(result.Diagnostics().empty());
        ASSERT_THROW(static_cast<void>(result.PV()), Exception_);
        ASSERT_THROW(static_cast<void>(result.RawSensitivity()), Exception_);
        ASSERT_THROW(static_cast<void>(result.DV01()), Exception_);
    }
    void AssertRoot(const CurveInstrumentCalibrationResult_& fit) {
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_EQ(fit.solveMode_, CurveSolveMode_("EXACT"));
        const auto values = PreparedCurveInstruments_(*fit.resolvedSpec_, Options()).Evaluate(fit.parameters_);
        SquareMatrix_<> matrix(2, 0.);
        Matrix_<> rhs(2, 2, 0.);
        for (int i = 0; i < 2; ++i) {
            ASSERT_LE(std::abs(values.rawResiduals_[i]), 1e-12);
            rhs(i, i) = 1.;
            for (int j = 0; j < 2; ++j)
                matrix(i, j) = values.quoteJacobian_(i, j);
        }
        const CheckedLinearSolve_ checked(matrix, rhs, {1e-12, 1e-12});
        ASSERT_GE(checked.Diagnostics().reciprocalConditionInfinity_, 1e-10);
    }
    double DirectPrice(const Handle_<CalibrationInstrument_>& instrument,
                       const Handle_<PreparedCompoundedOis_>& tables,
                       const CurveInstrumentCalibrationResult_& fit) {
        CurveScriptEvaluationRequest_ request;
        request.geometry_ = fit.resolvedSpec_->geometry_;
        request.market_ = fit.resolvedSpec_->market_;
        request.valuation_ = fit.resolvedSpec_->valuation_;
        request.compoundedOis_ = tables;
        request.parameters_ = fit.parameters_;
        return EvaluateCurveScriptInstrument(instrument, request).values_[0];
    }
    void AssertFd(const CurveInstrumentCalibrationSpec_& spec, const ScriptCurveQuoteRiskResult_& risk, const String_& role, bool zeroRate) {
        const auto& tables = spec.compoundedOis_[2];
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        const double notional = tables->Spec().notional_;
        Vector_<> previous;
        double maxError = 0., maxStability = 0.;
        for (const double h : {1e-4, 5e-5}) {
            Vector_<> fd(2);
            for (int j = 0; j < 2; ++j) {
                const auto plus = CalibrateCurveInstruments(Bumped(spec, j, h), Options());
                const auto minus = CalibrateCurveInstruments(Bumped(spec, j, -h), Options());
                ASSERT_NO_FATAL_FAILURE(AssertRoot(plus));
                ASSERT_NO_FATAL_FAILURE(AssertRoot(minus));
                const double up = DirectPrice(instrument, tables, plus), down = DirectPrice(instrument, tables, minus);
                ASSERT_NEAR(up, Oracle(tables->Spec(), role, plus.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                            1e-10 * notional);
                ASSERT_NEAR(down, Oracle(tables->Spec(), role, minus.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                            1e-10 * notional);
                fd[j] = (up - down) / (2. * h);
                const double tolerance = 1e-7 + 1e-5 * std::abs(fd[j] / notional);
                const double dvTolerance = 1e-11 + 1e-5 * std::abs(fd[j] * 1e-4 / notional);
                ASSERT_NEAR(risk.RawSensitivity()[j] / notional, fd[j] / notional, tolerance);
                ASSERT_NEAR(risk.DV01()[j] / notional, fd[j] * 1e-4 / notional, dvTolerance);
                maxError = std::max(maxError, std::abs(risk.RawSensitivity()[j] - fd[j]) / notional / tolerance);
                if (!previous.empty()) {
                    ASSERT_NEAR(fd[j] / notional, previous[j] / notional, tolerance);
                    maxStability = std::max(maxStability, std::abs(fd[j] - previous[j]) / notional / tolerance);
                }
            }
            previous = fd;
        }
        const auto mode = tables->Spec().observationConvention_ ? tables->Spec().observationConvention_->mode_.String() : "Legacy";
        std::cout << "OIS quote-FD " << tables->RuleId() << " mode=" << mode << ' ' << role << " zero_rate=" << zeroRate
                  << " today=" << spec.valuation_.todayFixingPolicy_.String() << " N=" << notional << " error/limit=" << maxError
                  << " stability/limit=" << maxStability << '\n';
    }
    void AssertModes(const CurveInstrumentCalibrationSpec_& spec, const String_& role, bool zeroRate = false) {
        const auto fit = CalibrateCurveInstruments(spec, Options());
        ASSERT_NO_FATAL_FAILURE(AssertRoot(fit));
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
        const auto full = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(true));
        ASSERT_TRUE(full.RiskAvailable());
        ASSERT_TRUE(full.MappingAvailable());
        ASSERT_TRUE(lean.RiskAvailable());
        ASSERT_FALSE(lean.MappingAvailable());
        ASSERT_FALSE(lean.ParameterQuoteMapping(spec).Available());
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, full, spec);
        const auto pvOnly = ValueCalibrationInstrumentWithQuoteRisk(instrument, lean, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_TRUE(pvOnly.Available());
        ASSERT_EQ(risk.Currency(), Ccy_("USD"));
        ASSERT_DOUBLE_EQ(risk.PV(), pvOnly.PV());
        ASSERT_NEAR(risk.PV(), Oracle(spec.compoundedOis_[2]->Spec(), role, fit.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                    1e-8);
        const auto mapping = full.ParameterQuoteMapping(spec);
        for (int j = 0; j < 2; ++j) {
            ASSERT_DOUBLE_EQ(risk.RawSensitivity()[j], pvOnly.RawSensitivity()[j]);
            ASSERT_DOUBLE_EQ(risk.DV01()[j], risk.RawSensitivity()[j] * 1e-4);
            const auto& m = mapping.ParameterResponse();
            ASSERT_NEAR(risk.RawSensitivity()[j], risk.NativeGradient()[0] * m(0, j) + risk.NativeGradient()[1] * m(1, j), 1e-8);
            ASSERT_LE(risk.Numerics().transposeBackwardErrors_[0], 1e-12);
            ASSERT_LE(mapping.Numerics().responseBackwardErrors_[j], 1e-12);
        }
        ASSERT_NO_FATAL_FAILURE(AssertFd(spec, risk, role, zeroRate));
    }
} // namespace

TEST(CompoundedOisQuoteRiskTest, TestVersionTwoFullAndPvOnlyUseFrozenNativeCapability) {
    const auto spec = Spec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
    for (const bool pvOnly : {false, true}) {
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
        ASSERT_TRUE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.MappingAvailable(), !pvOnly);
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_EQ(risk.RawSensitivity().size(), 2u);
        ASSERT_DOUBLE_EQ(risk.DV01()[0], risk.RawSensitivity()[0] * 1e-4);
        ASSERT_EQ(risk.Numerics().factorizationCount_, 1);
    }
}

TEST(CompoundedOisQuoteRiskTest, TestAllLegacyToolsRetainCompleteQuoteRecalibration) {
    for (const auto* role : {"Overnight", "Discount", "Shared"}) {
        auto spec = RiskSpec("None", role);
        for (size_t i = 0; i < 3; ++i) {
            auto legacy = Contract(i == 0, "None", true);
            legacy.instrumentId_ = spec.compoundedOis_[i]->Spec().instrumentId_;
            legacy.coupon_ = spec.compoundedOis_[i]->Spec().coupon_;
            ReplaceTable(&spec, i, legacy);
        }
        ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestQuoteSelectionSignedDv01AndActualNotionalScaling) {
    auto spec = RiskSpec("Lockout", "Shared");
    const auto primary = spec.compoundedOis_[2]->Spec();
    for (const auto* id : {"double-pv", "receive-pv"}) {
        auto variant = primary;
        variant.instrumentId_ = id;
        variant.notional_ *= String_(id) == "double-pv" ? 2. : 1.;
        variant.direction_ = String_(id) == "receive-pv" ? "ReceiveFixed" : "PayFixed";
        spec.compoundedOis_.push_back(PrepareCompoundedOis(variant, spec.valuation_));
    }
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto base = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]), snapshot, spec);
    ASSERT_TRUE(base.Available());
    for (const size_t i : {3, 4}) {
        const double scale = i == 3 ? 2. : -1.;
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[i]), snapshot, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_NEAR(risk.PV(), scale * base.PV(), 1e-10);
        for (int j = 0; j < 2; ++j) {
            ASSERT_NEAR(risk.RawSensitivity()[j], scale * base.RawSensitivity()[j], 1e-10);
            ASSERT_NEAR(risk.DV01()[j], scale * base.DV01()[j], 1e-14);
        }
    }
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const auto reordered = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"long", "short"}});
    const auto subset = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"long"}});
    ASSERT_TRUE(reordered.Available());
    ASSERT_TRUE(subset.Available());
    ASSERT_EQ(reordered.Axes().quotes_[0].ordinal_, 1);
    ASSERT_EQ(reordered.Axes().quotes_[1].ordinal_, 0);
    ASSERT_EQ(subset.Axes().quotes_[0].ordinal_, 1);
    ASSERT_EQ(subset.RawSensitivity().size(), 1u);
    ASSERT_DOUBLE_EQ(reordered.RawSensitivity()[0], base.RawSensitivity()[1]);
    ASSERT_DOUBLE_EQ(reordered.RawSensitivity()[1], base.RawSensitivity()[0]);
    ASSERT_DOUBLE_EQ(subset.RawSensitivity()[0], base.RawSensitivity()[1]);
}

TEST(CompoundedOisQuoteRiskTest, TestResolveRegisteredParRateOnBothSnapshotModes) {
    auto spec = RiskSpec("ObservationShift", "Shared", true);
    auto receiving = spec.compoundedOis_.back()->Spec();
    receiving.direction_ = "ReceiveFixed";
    receiving.minAbsAnnuity_ = .002;
    spec.compoundedOis_.back() = PrepareCompoundedOis(receiving, spec.valuation_);
    const auto fit = CalibrateCurveInstruments(spec, Options());
    for (const auto& tables : {spec.compoundedOis_.front(), spec.compoundedOis_.back()}) {
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        String_ planIdentity;
        for (const bool pvOnly : {false, true}) {
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto result = snapshot.ResolveCompoundedOisParRate(instrument, tables, spec);
            ASSERT_TRUE(result.Available());
            ASSERT_TRUE(result.diagnostics_.empty());
            const auto& plan = *result.plan_;
            ASSERT_EQ(plan.Measure().id_, "ParRate");
            ASSERT_EQ(plan.Measure().kind_, CalibrationMeasureKind_::Value_::ParAffineRate);
            ASSERT_EQ(plan.Measure().annuitySign_, tables->Spec().direction_ == "PayFixed" ? -1 : 1);
            ASSERT_EQ(plan.Measure().minAbsAnnuity_, tables->Spec().minAbsAnnuity_);
            ASSERT_EQ(plan.Capability().ruleId_, tables->RuleId());
            ASSERT_EQ(plan.Instrument()->InstrumentIdentity(), instrument->InstrumentIdentity());
            if (pvOnly)
                ASSERT_EQ(plan.PlanIdentity(), planIdentity);
            planIdentity = plan.PlanIdentity();
            ASSERT_EQ(result.quotes_.size(), 2u);
            ASSERT_EQ(result.quotes_[0].id_, "short");
            ASSERT_EQ(result.quotes_[0].ordinal_, 0);
            ASSERT_EQ(result.quotes_[1].id_, "long");
            ASSERT_EQ(result.quotes_[1].ordinal_, 1);
        }
    }
}

TEST(CompoundedOisQuoteRiskTest, TestParRateChecksTableIdentityWithColdAndWarmPreparationCache) {
    const auto spec = RiskSpec("Lookback", "Shared");
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    auto other = tables->Spec();
    other.coupons_.front().couponId_ = "renamed-coupon";
    const auto different = PrepareCompoundedOis(other, spec.valuation_);
    const auto sameScript = NewCompoundedOisScriptInstrument(different);
    ASSERT_EQ(sameScript->InstrumentIdentity(), instrument->InstrumentIdentity());
    ASSERT_NE(different->ObservationScheduleIdentity(), tables->ObservationScheduleIdentity());
    auto options = Options();
    options.preparationCache_ = std::make_shared<CalibrationPreparationCache_>();
    String_ planIdentity;
    for (const auto* cache : {"cold", "warm"}) {
        SCOPED_TRACE(cache);
        const auto fit = CalibrateCurveInstruments(spec, options);
        for (const bool pvOnly : {false, true}) {
            SCOPED_TRACE(pvOnly);
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto accepted = snapshot.ResolveCompoundedOisParRate(instrument, tables, spec);
            ASSERT_TRUE(accepted.Available());
            if (!planIdentity.empty())
                ASSERT_EQ(accepted.plan_->PlanIdentity(), planIdentity);
            planIdentity = accepted.plan_->PlanIdentity();
            ASSERT_NO_FATAL_FAILURE(
                AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(sameScript, different, spec), "CompoundedOisSourceMismatch"));
        }
    }
}

TEST(CompoundedOisQuoteRiskTest, TestParRateSelectsSnapshotQuotesAndRejectsInvalidRequests) {
    const auto spec = RiskSpec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    auto unknown = tables->Spec();
    unknown.instrumentId_ = "unregistered";
    const auto other = PrepareCompoundedOis(unknown, spec.valuation_);
    auto source = instrument->Definition()->Spec();
    source.source_.rows_.back().text_ += " ";
    const auto differentSource = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", .045, CalibrationValueUnit_::Parse("Rate")}}, {}});
    auto stale = spec;
    auto values = stale.valuation_.fixings_->Values();
    values["USD-SOFR"][DateTime_(Date_(2024, 2, 29))] += .001;
    stale.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
    for (const bool pvOnly : {false, true}) {
        SCOPED_TRACE(pvOnly);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
        const auto resolve = [&](const CompoundedOisParRateRequest_& request) {
            return snapshot.ResolveCompoundedOisParRate(instrument, tables, spec, request);
        };
        const auto reversed = resolve({{"long", "short"}});
        ASSERT_TRUE(reversed.Available());
        ASSERT_EQ(reversed.quotes_.size(), 2u);
        ASSERT_EQ(reversed.quotes_[0].id_, "long");
        ASSERT_EQ(reversed.quotes_[0].ordinal_, 1);
        ASSERT_EQ(reversed.quotes_[1].id_, "short");
        ASSERT_EQ(reversed.quotes_[1].ordinal_, 0);
        const auto subset = resolve({{"long"}});
        ASSERT_TRUE(subset.Available());
        ASSERT_EQ(subset.quotes_.size(), 1u);
        ASSERT_EQ(subset.quotes_.front().ordinal_, 1);
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{"long", "long"}}), "ScriptCurveQuoteRiskDuplicateSelection"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{"short", "LONG"}}), "ScriptCurveQuoteRiskUnknownSelection"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{}, true}), "ScriptCurveQuoteRiskLiveQuoteUnsupported"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(NewCompoundedOisScriptInstrument(other), other, spec),
                                                      "CompoundedOisUnregisteredInstrument"));
        for (const auto& changed : {differentSource, rebound})
            ASSERT_NO_FATAL_FAILURE(
                AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(changed, tables, spec), "CompoundedOisSourceMismatch"));
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(instrument, tables, stale), "ScriptCurveQuoteRiskIdentityMismatch"));
        const auto price = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{}, "ParRate"});
        ASSERT_FALSE(price.Available());
        ASSERT_EQ(price.Diagnostics().size(), 1u);
        ASSERT_EQ(price.Diagnostics().front().code_, "ScriptCurveQuoteRiskUnsupportedMeasure");
    }
}

TEST(CompoundedOisQuoteRiskTest, TestFixedContractRequestsAndIdentityMismatchesStayUnavailable) {
    const auto spec = RiskSpec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    for (const ScriptCurveQuoteRiskRequest_ request :
         {ScriptCurveQuoteRiskRequest_{{"unknown"}}, {{"short", "short"}}, {{"SHORT"}}, {{}, "ParRate"}, {{}, "Price", true}})
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, request)));
    auto unregistered = spec.compoundedOis_[2]->Spec();
    unregistered.instrumentId_ = "unknown-pv";
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(
        NewCompoundedOisScriptInstrument(PrepareCompoundedOis(unregistered, spec.valuation_)), snapshot, spec)));
    auto changed = spec.compoundedOis_[2]->Spec();
    changed.coupon_ += .001;
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(
        ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(PrepareCompoundedOis(changed, spec.valuation_)), snapshot, spec)));
    auto source = instrument->Definition()->Spec();
    source.source_.rows_.back().text_ += " ";
    const auto changedSource = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(changedSource, snapshot, spec)));
    const auto clone =
        ValueCalibrationInstrumentWithQuoteRisk(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(instrument)), snapshot, spec);
    ASSERT_TRUE(clone.Available());
}

TEST(CompoundedOisQuoteRiskTest, TestEveryFrozenOisContextDependencyIsChecked) {
    const auto spec = RiskSpec("Lookback", "Discount");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const Vector_<std::function<void(CurveInstrumentCalibrationSpec_*)>> changes{
        [](auto* s) {
            s->market_.overnightCurves_["USD.SOFR"].reset(new DiscountLogDF_("changed", "USD", {Date_(2024, 3, 1), Date_(2024, 3, 20)}, {0., -.03},
                                                                             DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        },
        [](auto* s) { s->market_.routes_.back().useTargetCurve_ = true; },
        [](auto* s) { s->valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical"); },
        [](auto* s) {
            auto values = s->valuation_.fixings_->Values();
            values["USD-SOFR"][DateTime_(Date_(2024, 2, 29))] = .04;
            s->valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        },
        [](auto* s) {
            auto c = s->compoundedOis_[2]->Spec();
            c.calendar_ = HolidayCenterData_("other-calendar", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
            ReplaceTable(s, 2, c);
        },
        [](auto* s) { s->compoundedOis_.pop_back(); },
        [](auto* s) { *s = Bumped(*s, 0, 1e-4); }};
    for (const auto& change : changes) {
        auto context = spec;
        change(&context);
        ASSERT_FALSE(snapshot.ValidateContext(context).Passed());
        ASSERT_FALSE(snapshot.ParameterQuoteMapping(context).Available());
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, context)));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestMissingTablesCannotEnterEitherRiskEndpoint) {
    const auto original = RiskSpec("ObservationShift", "Discount");
    auto spec = original;
    spec.observations_ = {spec.observations_[0]};
    spec.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 20)};
    spec.solver_.initialGuessPerNode_ = {-.002};
    spec.compoundedOis_.clear();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), {{1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.RiskAvailable());
    for (const bool legacy : {false, true}) {
        auto contract = Contract(false, "None", legacy);
        const auto instrument = NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_));
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec)));
    }
    auto fit = CalibrateCurveInstruments(original, Options());
    auto wrong = *fit.resolvedSpec_;
    wrong.compoundedOis_.clear();
    fit.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(wrong);
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(rejected.RiskAvailable());
    ASSERT_FALSE(rejected.Diagnostics().empty());
}

TEST(CompoundedOisQuoteRiskTest, TestSnapshotAndResultsOwnAllInputsBeyondTheirOwners) {
    for (const bool pvOnly : {false, true}) {
        std::optional<ScriptCurveQuoteRiskSnapshot_> snapshot;
        std::optional<ScriptCurveQuoteRiskResult_> available, unavailable;
        {
            auto spec = RiskSpec("ObservationShift", "Discount");
            auto curve = std::make_shared<DiscountLogDF_>("passive", "USD", spec.geometry_.knotDates_, Vector_<>{0., -.0002, -.02},
                                                          DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
            spec.market_.overnightCurves_["USD.SOFR"] = Handle_<DiscountCurve_>(curve);
            auto quote = std::make_shared<CalibrationMarketQuote_>(*spec.observations_[1]->Quote());
            const auto row = spec.observations_[1];
            spec.observations_[1] =
                NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), Handle_<CalibrationMarketQuote_>(quote), row->ErrorScale());
            const auto fit = CalibrateCurveInstruments(spec, Options());
            snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
            available = ValueCalibrationInstrumentWithQuoteRisk(instrument, *snapshot, spec);
            ASSERT_TRUE(available->Available());
            auto contract = spec.compoundedOis_[2]->Spec();
            contract.instrumentId_ = "unregistered";
            unavailable = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_)),
                                                                  *snapshot, spec);
            const Vector_<> delta{.01, .01};
            curve->ApplyDX(delta.begin(), 1.);
            *quote = *NewCalibrationMarketQuote(quote->Id(), quote->Value() + .01, quote->Unit());
            spec.compoundedOis_.clear();
            const auto again = ValueCalibrationInstrumentWithQuoteRisk(instrument, *snapshot, *snapshot->ResolvedSpec());
            ASSERT_TRUE(again.Available());
            ASSERT_DOUBLE_EQ(again.PV(), available->PV());
            ASSERT_EQ(again.RawSensitivity(), available->RawSensitivity());
        }
        ASSERT_TRUE(snapshot->RiskAvailable());
        ASSERT_EQ(snapshot->MappingAvailable(), !pvOnly);
        snapshot.reset();
        ASSERT_TRUE(std::isfinite(available->PV()));
        ASSERT_EQ(available->RawSensitivity().size(), 2u);
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(*unavailable));
        ASSERT_EQ(unavailable->Diagnostics()[0].instrumentId_, std::optional<String_>("unregistered"));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestUnobservablePwcAndPwlfRemainUnavailable) {
    for (const auto* parameterization : {"PIECEWISE_CONSTANT_FWD", "PIECEWISE_LINEAR_FWD"}) {
        auto spec = RiskSpec("ObservationShift", "Overnight");
        spec.geometry_.parameterization_ = CurveParameterization_(parameterization);
        spec.geometry_.knotDates_ = {Date_(2024, 3, 5), Date_(2024, 3, 20)};
        spec.solver_.initialGuessPerNode_.clear();
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[1]);
        const PreparedCurveInstruments_ initial(spec, Options());
        const Vector_<> theta(initial.Describe().freeParameters_.size(), .02);
        const double quote = initial.Evaluate(theta, false).modelMeasures_[1];
        spec.observations_.clear();
        for (size_t i = 0; i < theta.size(); ++i) {
            const auto id = String::FromInt(static_cast<int>(i));
            spec.observations_.push_back(NewCalibrationObservation(
                "o" + id, instrument, "ParRate", NewCalibrationMarketQuote("q" + id, quote, CalibrationValueUnit_::Parse("Rate")), 1e-13));
        }
        spec.solver_.initialGuessPerNode_ = theta;
        const auto fit = CalibrateCurveInstruments(spec, Options());
        ASSERT_TRUE(fit.fitWithinTolerance_);
        auto accuracy = Accuracy();
        accuracy.residualAbsoluteLimits_ = Vector_<>(theta.size(), 1e-12);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_FALSE(snapshot.MappingAvailable());
        ASSERT_FALSE(snapshot.Diagnostics().empty());
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec)));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestRootConditionAndApproximateModeCannotUseFitSuccessAlone) {
    auto spec = RiskSpec("None", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    auto strict = Accuracy();
    strict.minReciprocalCondition_ = 1.;
    const auto conditioned = NewScriptCurveQuoteRiskSnapshot(fit, strict);
    ASSERT_FALSE(conditioned.RiskAvailable());
    ASSERT_EQ(conditioned.Diagnostics()[0].code_, "ScriptCurveQuoteRiskConditionRejected");
    for (auto& row : spec.observations_)
        row = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), row->Quote(), .1);
    const auto looseFit = CalibrateCurveInstruments(spec, Options());
    ASSERT_TRUE(looseFit.fitWithinTolerance_);
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    for (const bool pvOnly : {false, true}) {
        const auto loose = NewScriptCurveQuoteRiskSnapshot(looseFit, Accuracy(pvOnly));
        ASSERT_FALSE(loose.RiskAvailable());
        ASSERT_EQ(loose.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(loose.ResolveCompoundedOisParRate(instrument, tables, spec), "ScriptCurveQuoteRiskRootResidual"));
    }
    spec.solver_.solveMode_ = CurveSolveMode_("APPROXIMATE");
    spec.solver_.fitWeight_ = 1.;
    spec.solver_.fitNormTolerance_ = .01;
    const auto approximateFit = CalibrateCurveInstruments(spec, Options());
    for (const bool pvOnly : {false, true}) {
        const auto approximate = NewScriptCurveQuoteRiskSnapshot(approximateFit, Accuracy(pvOnly));
        ASSERT_FALSE(approximate.RiskAvailable());
        ASSERT_EQ(approximate.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedMode");
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(approximate.ResolveCompoundedOisParRate(instrument, tables, spec), "ScriptCurveQuoteRiskUnsupportedMode"));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestTodayHistoryAndModelFutureIsolationUseFrozenFixings) {
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        auto spec = RiskSpec(mode.String(), "Discount");
        spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
        RebuildValuation(&spec);
        const double value =
            Oracle(spec.compoundedOis_[1]->Spec(), "Discount", Parameters(), false, spec.valuation_, PassiveParameters("Discount"))[1];
        spec = Bumped(spec, 1, value - spec.observations_[1]->Quote()->Value());
        ASSERT_NO_FATAL_FAILURE(AssertModes(spec, "Discount"));
    }
    auto original = RiskSpec("ObservationShift", "Shared");
    const auto before = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(original, Options()), Accuracy());
    const auto pv = NewCompoundedOisScriptInstrument(original.compoundedOis_[2]);
    const auto first = ValueCalibrationInstrumentWithQuoteRisk(pv, before, original);
    auto values = original.valuation_.fixings_->Values();
    values["USD-SOFR"][DateTime_(Date_(2024, 3, 1))] = -.5;
    values["USD-SOFR"][DateTime_(Date_(2024, 3, 5))] = -.4;
    original.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
    RebuildValuation(&original);
    const auto after = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(original, Options()), Accuracy());
    const auto second = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(original.compoundedOis_[2]), after, original);
    ASSERT_TRUE(first.Available());
    ASSERT_TRUE(second.Available());
    ASSERT_NE(before.ContextIdentity(), after.ContextIdentity());
    ASSERT_DOUBLE_EQ(first.PV(), second.PV());
    ASSERT_EQ(first.RawSensitivity(), second.RawSensitivity());
}

TEST(CompoundedOisQuoteRiskTest, TestHistoricalRatesRetainDiscountRiskAndSettledPriceHasZeroRisk) {
    for (const auto* role : {"Overnight", "Discount", "Shared"}) {
        auto spec = RiskSpec("ObservationShift", role);
        auto values = spec.valuation_.fixings_->Values();
        values["USD-SOFR"][DateTime_(Date_(2024, 2, 27))] = .02;
        values["USD-SOFR"][DateTime_(Date_(2024, 2, 28))] = .021;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        RebuildValuation(&spec);
        auto contract = Contract(true, "None");
        contract.instrumentId_ = "history-pv";
        contract.coupons_ = {{"past-accrual", Date_(2024, 2, 27), Date_(2024, 2, 29), Date_(2024, 3, 6)}};
        contract.fixedLeg_ = {{Date_(2024, 2, 27), Date_(2024, 2, 29), Date_(2024, 3, 6), 2. / 365.}};
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        contract.instrumentId_ = "settled-pv";
        contract.coupons_ = {{"no-history-query", Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29)}};
        contract.fixedLeg_ = {{Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29), 1. / 365.}};
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        contract = spec.compoundedOis_[3]->Spec();
        contract.instrumentId_ = "today-pv";
        contract.coupons_[0].payment_ = contract.fixedLeg_[0].payment_ = Date_(2024, 3, 1);
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        const auto fit = CalibrateCurveInstruments(spec, Options());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        const auto historical = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[3]), snapshot, spec);
        const auto settled = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[4]), snapshot, spec);
        const auto today = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[5]), snapshot, spec);
        ASSERT_TRUE(historical.Available());
        ASSERT_TRUE(settled.Available());
        ASSERT_TRUE(today.Available());
        ASSERT_DOUBLE_EQ(settled.PV(), 0.);
        ASSERT_EQ(settled.RawSensitivity(), (Vector_<>{0., 0.}));
        ASSERT_EQ(settled.DV01(), (Vector_<>{0., 0.}));
        const auto discount = String_(role) == "Overnight" ? PassiveParameters(role) : fit.parameters_;
        const double amount = 100. * ((1. + .02 / 360.) * (1. + .021 / 360.) - 1. - .02 * 2. / 365.);
        ASSERT_NEAR(historical.PV(), amount * std::exp(LogDiscount(Date_(2024, 3, 6), discount)), 1e-10);
        ASSERT_NEAR(today.PV(), amount, 1e-10);
        ASSERT_EQ(today.RawSensitivity(), (Vector_<>{0., 0.}));
        const double targetDiscount = String_(role) == "Overnight" ? 0. : 1.;
        ASSERT_NEAR(historical.NativeGradient()[0], targetDiscount * historical.PV() * 14. / 15., 1e-12);
        ASSERT_NEAR(historical.NativeGradient()[1], targetDiscount * historical.PV() / 15., 1e-12);
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[3]);
        for (const double h : {1e-4, 5e-5})
            for (int j = 0; j < 2; ++j) {
                const auto plus = CalibrateCurveInstruments(Bumped(spec, j, h), Options());
                const auto minus = CalibrateCurveInstruments(Bumped(spec, j, -h), Options());
                ASSERT_NO_FATAL_FAILURE(AssertRoot(plus));
                ASSERT_NO_FATAL_FAILURE(AssertRoot(minus));
                const double fd =
                    (DirectPrice(instrument, spec.compoundedOis_[3], plus) - DirectPrice(instrument, spec.compoundedOis_[3], minus)) / (2. * h);
                ASSERT_NEAR(historical.RawSensitivity()[j] / 100., fd / 100., 1e-7 + 1e-5 * std::abs(fd / 100.));
                ASSERT_NEAR(historical.DV01()[j] / 100., fd * 1e-4 / 100., 1e-11 + 1e-5 * std::abs(fd * 1e-4 / 100.));
            }
        const double magnitude = std::abs(historical.RawSensitivity()[0]) + std::abs(historical.RawSensitivity()[1]);
        ASSERT_EQ(magnitude == 0., String_(role) == "Overnight");
    }
}

TEST(CompoundedOisQuoteRiskTest, TestHistoricalLockoutAnchorCannotSupplyMissingOnTargetRank) {
    auto spec = RiskSpec("Lockout", "Overnight");
    spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    RebuildValuation(&spec);
    for (size_t i = 0; i < 2; ++i) {
        const double value = Oracle(spec.compoundedOis_[i]->Spec(), "Overnight", Parameters(), false, spec.valuation_)[1];
        spec = Bumped(spec, static_cast<int>(i), value - spec.observations_[i]->Quote()->Value());
    }
    spec.solver_.initialGuessPerNode_ = Parameters();
    const auto fit = CalibrateCurveInstruments(spec, Options());
    ASSERT_TRUE(fit.fitWithinTolerance_);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_FALSE(snapshot.MappingAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskJacobianRejected");
}

TEST(CompoundedOisQuoteRiskTest, TestAllModesThreeTargetsAndZeroRateMatchCompleteQuoteRecalibration) {
    for (const bool zeroRate : {false, true})
        for (const auto& mode : CompoundedOisObservationModeListAll())
            for (const auto* role : {"Overnight", "Discount", "Shared"}) {
                SCOPED_TRACE(mode.String());
                SCOPED_TRACE(role);
                ASSERT_NO_FATAL_FAILURE(AssertModes(RiskSpec(mode.String(), role, false, zeroRate), role, zeroRate));
            }
}

TEST(CompoundedOisQuoteRiskTest, TestMixedLegacyToolsAndLegacyPvMatchCompleteQuoteRecalibration) {
    for (const auto& mode : CompoundedOisObservationModeListAll())
        for (const auto* role : {"Overnight", "Discount", "Shared"}) {
            auto spec = RiskSpec(mode.String(), role, true);
            ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
            auto pv = Contract(false, "None", true);
            pv.instrumentId_ = "registered-pv";
            pv.coupon_ = .031;
            spec.compoundedOis_[2] = PrepareCompoundedOis(pv, spec.valuation_);
            ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
        }
}
