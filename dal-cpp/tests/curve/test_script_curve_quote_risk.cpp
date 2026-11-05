#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <limits>

#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/time/dateutils.hpp>

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

namespace {
    CurveInstrumentCalibrationSpec_ ReplaceQuote(CurveInstrumentCalibrationSpec_ spec, int i, double value, double errorScale = 1e-13) {
        const auto row = spec.observations_[i];
        const auto quote = NewCalibrationMarketQuote(row->Quote()->Id(), value, row->Quote()->Unit(), row->Quote()->Convention());
        spec.observations_[i] = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), quote, errorScale);
        return spec;
    }
    CurveInstrumentCalibrationSpec_ TightFitSpec(CurveInstrumentCalibrationSpec_ spec) {
        for (int i = 0; i < static_cast<int>(spec.observations_.size()); ++i) {
            const auto value = spec.observations_[i]->Quote()->Value();
            spec = ReplaceQuote(std::move(spec), i, value);
        }
        return spec;
    }
    CurveInstrumentCalibrationOptions_ AadOptions() {
        CurveInstrumentCalibrationOptions_ result;
        result.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        return result;
    }
    ScriptCurveQuoteRiskAccuracy_ MappingAccuracy(int count) { return {Vector_<>(count, 1e-12), 1e-10, 1e-12}; }
    RateIndexConvention_ MappingIndex(bool projected = false, const String_& tenor = "3M") {
        RateIndexConvention_ result;
        result.dayBasis_ = DayBasis_("ACT_360");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.useProjectionCurve_ = projected;
        result.forecastTenor_ = PeriodLength_(tenor);
        return result;
    }
    RateLegConvention_ MappingLeg(const String_& tenor) {
        RateLegConvention_ result;
        result.paymentFrequency_ = PeriodLength_(tenor);
        result.dayBasis_ = DayBasis_("ACT_360");
        result.businessDayConvention_ = result.paymentConvention_ = BizDayConvention_("Unadjusted");
        return result;
    }
    Handle_<CalibrationInstrument_> MappingInstrument(const String_& family, const Date_& end, const String_& id) {
        const CurveScriptImportSettings_ settings{id, Ccy_("USD"), "USD.OIS", 1.};
        if (family == "Deposit")
            return NewDepositScriptInstrument(Deposit_(TODAY, end, .025, DayBasis_("ACT_360")), settings);
        if (family == "IRS")
            return NewSwapScriptInstrument(Swap_(TODAY, TODAY, end, .025, MappingLeg("12M"), MappingIndex(), MappingLeg("3M")), settings);
        return NewBasisSwapScriptInstrument(
            BasisSwap_(TODAY, TODAY, end, -.001, MappingIndex(true), MappingLeg("3M"), MappingIndex(true, "6M"), MappingLeg("6M")), settings);
    }
    CurveInstrumentCalibrationSpec_ MappingSample(const String_& family, int count) {
        auto spec = DepositSpec();
        spec.geometry_.knotDates_ = {TODAY};
        spec.observations_.clear();
        if (family == "Basis") {
            spec.geometry_.calibrateDiscountCurve_ = false;
            spec.geometry_.targetTenor_ = PeriodLength_("3M");
            spec.market_.discountCurves_[CollateralType_("OIS")].reset(new Tape::DiscountPWC_<double>("discount", "USD", {TODAY}, {.02}));
            spec.market_.forwardCurves_[PeriodLength_("6M")].reset(new Tape::DiscountPWC_<double>("six-month", "USD", {TODAY}, {.045}));
            for (const auto& tenor : {"3M", "6M"})
                spec.market_.routes_.push_back(
                    {CurveScriptProjectionRoute(Ccy_("USD"), PeriodLength_(tenor)), true, CollateralType_("OIS"), PeriodLength_(tenor)});
        }
        const auto unit = CalibrationValueUnit_::Parse(family == "Basis" ? "BasisSpread" : "Rate");
        for (int i = 0; i < count; ++i) {
            const auto end = Date::AddMonths(TODAY, 12 * (i + 1));
            const auto id = String::FromInt(i);
            spec.geometry_.knotDates_.push_back(end);
            spec.observations_.push_back(NewCalibrationObservation(
                "o" + id, MappingInstrument(family, end, "i" + id), family == "Basis" ? "BasisSpread" : "ParRate",
                NewCalibrationMarketQuote("q" + id, family == "Basis" ? -.001 + i * .00001 : .025 + i * .0005, unit), 1e-13));
        }
        return spec;
    }
    Matrix_<> RecalibratedResponse(const CurveInstrumentCalibrationSpec_& spec, double step) {
        const int n = static_cast<int>(spec.observations_.size());
        Matrix_<> result(n, n);
        for (int j = 0; j < n; ++j) {
            const double quote = spec.observations_[j]->Quote()->Value();
            const auto plus = CalibrateCurveInstruments(ReplaceQuote(spec, j, quote + step), AadOptions());
            const auto minus = CalibrateCurveInstruments(ReplaceQuote(spec, j, quote - step), AadOptions());
            for (int i = 0; i < n; ++i)
                result(i, j) = (plus.parameters_[i] - minus.parameters_[i]) / (2. * step);
        }
        return result;
    }
    double PeakAbsolute(const Vector_<>& values) {
        double result = 0.;
        for (const double value : values)
            result = std::max(result, std::abs(value));
        return result;
    }
    void AssertMappingReference(const CurveInstrumentCalibrationSpec_& spec, const String_& label) {
        const auto fit = CalibrateCurveInstruments(spec, AadOptions());
        ASSERT_TRUE(fit.quoteJacobian_.Empty());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(static_cast<int>(spec.observations_.size())));
        ASSERT_TRUE(snapshot.MappingAvailable()) << snapshot.Diagnostics().front().message_;
        const auto map = snapshot.ParameterQuoteMapping(spec);
        ASSERT_TRUE(map.Available());
        const auto coarse = RecalibratedResponse(spec, 1e-4), fine = RecalibratedResponse(spec, 5e-5);
        double maxError = 0., maxStability = 0.;
        for (int i = 0; i < fine.Rows(); ++i)
            for (int j = 0; j < fine.Cols(); ++j) {
                ASSERT_TRUE(std::isfinite(coarse(i, j)));
                ASSERT_TRUE(std::isfinite(fine(i, j)));
                ASSERT_TRUE(std::isfinite(map.ParameterResponse()(i, j)));
                const double limit = 1e-7 + 1e-5 * std::abs(fine(i, j));
                maxError = std::max(maxError, std::abs(map.ParameterResponse()(i, j) - fine(i, j)) / limit);
                maxStability = std::max(maxStability, std::abs(coarse(i, j) - fine(i, j)) / limit);
            }
        ASSERT_LE(maxStability, 1.) << label;
        ASSERT_LE(maxError, 1.) << label;
        ASSERT_EQ(snapshot.Numerics().factorizationCount_, 1);
        std::cout << "mapping-reference " << label << " n=" << fine.Rows() << " error/limit=" << maxError << " stability/limit=" << maxStability
                  << " rcond=" << *snapshot.Numerics().reciprocalCondition_ << " raw_residual=" << PeakAbsolute(snapshot.Numerics().rawResiduals_)
                  << " transpose_error=" << PeakAbsolute(snapshot.Numerics().transposeBackwardErrors_)
                  << " response_error=" << PeakAbsolute(snapshot.Numerics().responseBackwardErrors_) << '\n';
    }
} // namespace

namespace {
    template <class F_> std::optional<CalibrationDiagnostic_> InputDiagnostic(const F_& action) {
        try {
            action();
        } catch (const CalibrationError_& error) {
            return error.Failure().diagnostic_;
        }
        return std::nullopt;
    }
    void AssertMalformedPvContext(const std::function<void(CurveInstrumentCalibrationSpec_*)>& change, const String_& field) {
        const auto spec = TightFitSpec(DepositSpec());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
        ASSERT_TRUE(snapshot.MappingAvailable());
        auto invalid = spec;
        change(&invalid);
        const auto mode = AAD::SetNumResultsForAAD(true, 3);
        const auto mappingError = InputDiagnostic([&] { static_cast<void>(snapshot.ParameterQuoteMapping(invalid)); });
        const auto pvError = InputDiagnostic(
            [&] { static_cast<void>(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, invalid)); });
        ASSERT_TRUE(mappingError);
        ASSERT_EQ(mappingError->code_, "InvalidScriptCurveQuoteRiskInput");
        ASSERT_EQ(mappingError->field_, field);
        ASSERT_TRUE(pvError);
        ASSERT_EQ(pvError->code_, mappingError->code_);
        ASSERT_EQ(pvError->field_, mappingError->field_);
        ASSERT_EQ(pvError->stage_, mappingError->stage_);
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
        ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
        ASSERT_TRUE(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec).Available());
    }
} // namespace

TEST(ScriptCurveQuoteRiskTest, TestPvAndMappingRejectMissingFixingContext) {
    AssertMalformedPvContext([](auto* spec) { spec->valuation_.fixings_.reset(); }, "/context/valuation/fixings");
}

TEST(ScriptCurveQuoteRiskTest, TestPvAndMappingRejectMissingValuationDateContext) {
    AssertMalformedPvContext([](auto* spec) { spec->valuation_.evaluationDate_.reset(); }, "/context/valuation/evaluation_date");
}

TEST(ScriptCurveQuoteRiskTest, TestPvAndMappingRejectNanSolverInitialGuessContext) {
    AssertMalformedPvContext([](auto* spec) { spec->solver_.initialGuess_ = std::numeric_limits<double>::quiet_NaN(); },
                             "/context/solver/initial_guess");
}

TEST(ScriptCurveQuoteRiskTest, TestMalformedPvContextPreservesOuterRecording) {
    const auto spec = TightFitSpec(DepositSpec());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
    auto invalid = spec;
    invalid.valuation_.fixings_.reset();
    AAD::RecordingScope_ outer;
    AAD::Number_ x;
    outer.RegisterInput(x, 2.);
    outer.StartRecording();
    const auto nodes = AAD::MeasureTape(*AAD::Tape()).nodes_;
    const auto multi = AAD::Tape()->multi_;
    const auto width = AAD::Tape()->numAdj_;
    const auto mappingError = InputDiagnostic([&] { static_cast<void>(snapshot.ParameterQuoteMapping(invalid)); });
    const auto pvError =
        InputDiagnostic([&] { static_cast<void>(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, invalid)); });
    ASSERT_TRUE(mappingError);
    ASSERT_TRUE(pvError);
    ASSERT_EQ(pvError->code_, "InvalidScriptCurveQuoteRiskInput");
    ASSERT_EQ(pvError->field_, mappingError->field_);
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, nodes);
    ASSERT_EQ(AAD::Tape()->multi_, multi);
    ASSERT_EQ(AAD::Tape()->numAdj_, width);
    auto y = AAD::Number_(x * x);
    outer.FinishRecording();
    AAD::Adjoint(y) = 1.;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(x), 4.);
    outer.Close();
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    ASSERT_TRUE(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec).Available());
}

TEST(ScriptCurveQuoteRiskTest, TestStaleAndNumericalContextsKeepOwnedUnavailableResults) {
    const auto multi = AAD::Tape()->multi_;
    const auto width = AAD::Tape()->numAdj_;
    for (const bool numerical : {false, true}) {
        SCOPED_TRACE(numerical);
        std::optional<ScriptCurveQuoteMapping_> mapping;
        std::optional<ScriptCurveQuoteRiskResult_> pv;
        {
            const auto spec = numerical ? ReplaceQuote(DepositSpec(), 0, .035, .1) : TightFitSpec(DepositSpec());
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
            ASSERT_TRUE(snapshot.StructureValid());
            ASSERT_EQ(snapshot.MappingAvailable(), !numerical);
            auto context = spec;
            if (!numerical)
                ++context.solver_.maxEvaluations_;
            const auto mode = AAD::SetNumResultsForAAD(true, 3);
            mapping = snapshot.ParameterQuoteMapping(context);
            pv = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, context);
            ASSERT_TRUE(AAD::Tape()->multi_);
            ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
            ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
        }
        const auto code = numerical ? "ScriptCurveQuoteRiskRootResidual" : "ScriptCurveQuoteRiskIdentityMismatch";
        ASSERT_TRUE(mapping);
        ASSERT_TRUE(pv);
        ASSERT_FALSE(mapping->Available());
        ASSERT_FALSE(pv->Available());
        ASSERT_EQ(mapping->Diagnostics()[0].code_, code);
        ASSERT_EQ(pv->Diagnostics()[0].code_, code);
        ASSERT_EQ(pv->Diagnostics()[0].field_, mapping->Diagnostics()[0].field_);
        ASSERT_EQ(mapping->Axes().quotes_[0].id_, "q");
        ASSERT_EQ(pv->Axes().quotes_[0].id_, "q");
        ASSERT_THROW(static_cast<void>(mapping->ParameterResponse()), Exception_);
        ASSERT_THROW(static_cast<void>(pv->PV()), Exception_);
        ASSERT_EQ(AAD::Tape()->multi_, multi);
        ASSERT_EQ(AAD::Tape()->numAdj_, width);
        ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
        ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
    }
}

TEST(ScriptCurveQuoteRiskTest, TestDepositPvRiskPreservesFixedCouponNotionalAndSign) {
    const auto spec = TightFitSpec(DepositSpec());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec);
    ASSERT_TRUE(result.Available());
    const double alpha = (MATURITY - TODAY) / 360.;
    const double df = 1. / (1. + .035 * alpha);
    ASSERT_NEAR(result.PV(), 100. * ((1. + .025 * alpha) * df - 1.), 1e-10);
    ASSERT_NEAR(result.RawSensitivity()[0], -100. * (1. + .025 * alpha) * alpha * df * df, 1e-10);
    ASSERT_DOUBLE_EQ(result.DV01()[0], result.RawSensitivity()[0] * 1e-4);
    ASSERT_EQ(result.Currency(), Ccy_("USD"));
    ASSERT_TRUE(snapshot.RiskAvailable());
}

namespace {
    double
    FixedContractPrice(const Handle_<CalibrationInstrument_>& instrument, const CurveInstrumentCalibrationSpec_& spec, const Vector_<>& parameters) {
        const auto prepared = CurveInstrumentInternal::PrepareCurvePrice(spec, instrument, "Price");
        const CandidateCurveProvider_<double> provider(prepared.geometry_, parameters);
        CurveScriptInternal::EvaluationScratch_<double> scratch;
        return CurveInstrumentInternal::CashflowMeasure(prepared.plan_, provider, spec.geometry_.today_, &scratch);
    }
    Vector_<> RecalibratedPriceRisk(const Handle_<CalibrationInstrument_>& instrument, const CurveInstrumentCalibrationSpec_& spec, double step) {
        Vector_<> result;
        for (int j = 0; j < static_cast<int>(spec.observations_.size()); ++j) {
            const double quote = spec.observations_[j]->Quote()->Value();
            const auto up = ReplaceQuote(spec, j, quote + step), down = ReplaceQuote(spec, j, quote - step);
            const auto plus = CalibrateCurveInstruments(up, AadOptions()), minus = CalibrateCurveInstruments(down, AadOptions());
            result.push_back((FixedContractPrice(instrument, up, plus.parameters_) - FixedContractPrice(instrument, down, minus.parameters_)) /
                             (2. * step));
        }
        return result;
    }
    void AssertPriceReference(const CurveInstrumentCalibrationSpec_& spec, const String_& label) {
        const auto fit = CalibrateCurveInstruments(spec, AadOptions());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(static_cast<int>(spec.observations_.size())));
        const auto instrument = spec.observations_.back()->Instrument();
        const auto binding = instrument->BindingIdentity(), identity = snapshot.Identity(), context = snapshot.ContextIdentity();
        const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
        ASSERT_TRUE(result.Available()) << result.Diagnostics().front().message_;
        const auto coarse = RecalibratedPriceRisk(instrument, spec, 1e-4), fine = RecalibratedPriceRisk(instrument, spec, 5e-5);
        double maxError = 0., maxStability = 0.;
        for (size_t j = 0; j < fine.size(); ++j) {
            ASSERT_TRUE(std::isfinite(fine[j]));
            ASSERT_TRUE(std::isfinite(coarse[j]));
            const double limit = 1e-7 + 1e-5 * std::abs(fine[j]);
            maxStability = std::max(maxStability, std::abs(coarse[j] - fine[j]) / limit);
            maxError = std::max(maxError, std::abs(result.RawSensitivity()[j] - fine[j]) / limit);
            ASSERT_DOUBLE_EQ(result.DV01()[j], result.RawSensitivity()[j] * 1e-4);
        }
        ASSERT_LE(maxStability, 1.) << label;
        ASSERT_LE(maxError, 1.) << label;
        ASSERT_EQ(instrument->BindingIdentity(), binding);
        ASSERT_EQ(snapshot.Identity(), identity);
        ASSERT_EQ(snapshot.ContextIdentity(), context);
        ASSERT_TRUE(snapshot.ValidateContext(spec).Passed());
        ASSERT_EQ(result.SourceIdentity(), instrument->Definition()->SourceIdentity());
        ASSERT_EQ(result.BindingIdentity(), binding);
        ASSERT_EQ(result.Numerics().factorizationCount_, 1);
        ASSERT_LE(result.Numerics().transposeBackwardErrors_[0], snapshot.Accuracy().responseBackwardErrorLimit_);
        ASSERT_NEAR(result.PV(), FixedContractPrice(instrument, spec, fit.parameters_), 1e-10);
        std::cout << "pv-reference " << label << " n=" << fine.size() << " error/limit=" << maxError << " stability/limit=" << maxStability << '\n';
    }
} // namespace

TEST(ScriptCurveQuoteRiskTest, TestPvRiskAgainstIndependentFullRecalibration) {
    for (const auto& family : {"Deposit", "IRS", "Basis"})
        for (const int count : {1, 5, 15}) {
            SCOPED_TRACE(String_(family) + String::FromInt(count));
            AssertPriceReference(MappingSample(family, count), family);
        }
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskRejectsInvalidRequestsAndOwnsDiagnostics) {
    const auto spec = MappingSample("IRS", 5);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(5));
    const auto instrument = spec.observations_.back()->Instrument();
    const Vector_<ScriptCurveQuoteRiskRequest_> requests = {{{"q0", "q0"}}, {{"Q0"}}, {{}, "ParRate"}, {{}, "price"}, {{}, "Price", true}};
    const Vector_<String_> codes = {"ScriptCurveQuoteRiskDuplicateSelection", "ScriptCurveQuoteRiskUnknownSelection",
                                    "ScriptCurveQuoteRiskUnsupportedMeasure", "ScriptCurveQuoteRiskUnsupportedMeasure",
                                    "ScriptCurveQuoteRiskLiveQuoteUnsupported"};
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, requests[i]);
        ASSERT_FALSE(result.Available());
        ASSERT_EQ(result.Diagnostics()[0].code_, codes[i]);
        ASSERT_EQ(result.Diagnostics()[0].instrumentId_, instrument->Id());
        ASSERT_TRUE(result.Diagnostics()[0].field_);
        ASSERT_THROW(static_cast<void>(result.PV()), Exception_);
        ASSERT_THROW(static_cast<void>(result.RawSensitivity()), Exception_);
        ASSERT_THROW(static_cast<void>(result.DV01()), Exception_);
        ASSERT_THROW(static_cast<void>(result.NativeGradient()), Exception_);
        ASSERT_EQ(result.Axes().quotes_.size(), 5u);
    }
    const auto selected = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"q4", "q0"}});
    const auto all = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_TRUE(selected.Available());
    ASSERT_EQ(selected.Axes().quotes_[0].ordinal_, 4);
    ASSERT_EQ(selected.Axes().quotes_[1].ordinal_, 0);
    ASSERT_DOUBLE_EQ(selected.RawSensitivity()[0], all.RawSensitivity()[4]);
    ASSERT_DOUBLE_EQ(selected.RawSensitivity()[1], all.RawSensitivity()[0]);
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskNestedRecordingHasExplicitDiagnosticAndPreservesOuterTape) {
    const auto spec = TightFitSpec(DepositSpec());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
    AAD::RecordingScope_ outer;
    AAD::Number_ x;
    outer.RegisterInput(x, 2.);
    outer.StartRecording();
    const auto before = AAD::MeasureTape(*AAD::Tape()).nodes_;
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec);
    ASSERT_FALSE(result.Available());
    ASSERT_EQ(result.Diagnostics()[0].code_, "ScriptCurveQuoteRiskNestedRecording");
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, before);
    auto y = AAD::Number_(x * x);
    outer.FinishRecording();
    AAD::Adjoint(y) = 1.;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(x), 4.);
    outer.Close();
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskScalingAndRebuiltQuotePermutation) {
    const auto spec = MappingSample("IRS", 5);
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(5));
    const auto instrument = spec.observations_.back()->Instrument();
    const auto original = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_TRUE(original.Available());
    const auto contract = DumpCalibrationInstrumentJson(instrument);
    for (const double scale : {1e-4, 1., 10.}) {
        auto changed = spec;
        changed.solver_.parameterScales_ = Vector_<>(5, scale);
        changed.solver_.initialGuessPerNode_ = fit.parameters_;
        for (int i = 0; i < 5; ++i)
            changed = ReplaceQuote(changed, i, changed.observations_[i]->Quote()->Value(), .01 * (i + 1));
        const auto scaled = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(changed, AadOptions()), MappingAccuracy(5));
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, scaled, changed);
        ASSERT_TRUE(risk.Available());
        for (int j = 0; j < 5; ++j)
            ASSERT_NEAR(risk.RawSensitivity()[j], original.RawSensitivity()[j], 1e-10);
    }
    auto reordered = spec;
    std::reverse(reordered.observations_.begin(), reordered.observations_.end());
    const auto stale = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, reordered);
    ASSERT_FALSE(stale.Available());
    ASSERT_EQ(stale.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    const auto rebuilt = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(reordered, AadOptions()), MappingAccuracy(5));
    const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, rebuilt, reordered);
    ASSERT_TRUE(risk.Available());
    for (int j = 0; j < 5; ++j) {
        ASSERT_EQ(risk.Axes().quotes_[j].id_, original.Axes().quotes_[4 - j].id_);
        ASSERT_NEAR(risk.RawSensitivity()[j], original.RawSensitivity()[4 - j], 1e-10);
    }
    ASSERT_EQ(DumpCalibrationInstrumentJson(instrument), contract);
    ASSERT_TRUE(snapshot.ValidateContext(spec).Passed());
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskFrozenQuoteCopyIsNotLiveDependence) {
    const auto spec = TightFitSpec(DepositSpec());
    const auto& quote = spec.observations_[0]->Quote();
    const auto instrument = BindCalibrationInstrumentParameters(spec.observations_[0]->Instrument(),
                                                                {{{"K", quote->Value(), quote->Unit()}}, {{"K", quote->Id(), quote->Fingerprint()}}});
    const auto before = DumpCalibrationInstrumentJson(instrument);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
    const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_TRUE(risk.Available());
    ASSERT_NEAR(risk.PV(), 0., 1e-10);
    ASSERT_LT(risk.RawSensitivity()[0], -90.);
    const auto fd = RecalibratedPriceRisk(instrument, spec, 5e-5);
    ASSERT_NEAR(risk.RawSensitivity()[0], fd[0], 1e-7 + 1e-5 * std::abs(fd[0]));
    ASSERT_FALSE(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{}, "Price", true}).Available());
    ASSERT_EQ(DumpCalibrationInstrumentJson(instrument), before);
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskSuccessFailureRestoreModeAndActiveNodes) {
    auto spec = TightFitSpec(DepositSpec());
    spec = ReplaceQuote(spec, 0, -.4);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), Accuracy());
    ASSERT_TRUE(snapshot.RiskAvailable());
    const auto instrument = spec.observations_[0]->Instrument();
    const auto huge = BindCalibrationInstrumentParameters(instrument, {{{"N", 1.4e308, CalibrationValueUnit_::Parse("Price:USD")}}, {}});
    const auto mode = AAD::SetNumResultsForAAD(true, 3);
    const auto good = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_TRUE(good.Available());
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
    const auto bad = ValueCalibrationInstrumentWithQuoteRisk(huge, snapshot, spec);
    ASSERT_FALSE(bad.Available());
    ASSERT_EQ(bad.Diagnostics()[0].code_, "ScriptCurveQuoteRiskNonFinite");
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
    ASSERT_TRUE(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec).Available());
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskOwnsResultsAndReusesSnapshotConcurrently) {
    const auto spec = MappingSample("Deposit", 5);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(5));
    const auto instrument = spec.observations_.back()->Instrument();
    auto future = std::async(std::launch::async, [&] { return ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec); });
    const auto expected = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    for (int i = 0; i < 100; ++i) {
        const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
        ASSERT_TRUE(result.Available());
        ASSERT_EQ(result.RawSensitivity(), expected.RawSensitivity());
        ASSERT_EQ(result.Numerics().factorizationCount_, 1);
    }
    const auto other = future.get();
    ASSERT_TRUE(other.Available());
    ASSERT_EQ(other.RawSensitivity(), expected.RawSensitivity());
    const auto owned = [] {
        const auto s = TightFitSpec(DepositSpec());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(s, AadOptions()), Accuracy());
        return ValueCalibrationInstrumentWithQuoteRisk(s.observations_[0]->Instrument(), snapshot, s);
    }();
    ASSERT_TRUE(owned.Available());
    ASSERT_EQ(owned.Axes().quotes_[0].id_, "q");
    ASSERT_FALSE(owned.BindingIdentity().empty());
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
}

TEST(ScriptCurveQuoteRiskTest, TestPvFixedBackgroundProducesZeroNativeAndQuoteRisk) {
    const auto spec = MappingSample("Basis", 5);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(5));
    const auto instrument = MappingInstrument("Deposit", spec.geometry_.knotDates_.back(), "background-only");
    const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
    ASSERT_TRUE(result.Available());
    for (const double g : result.NativeGradient())
        ASSERT_DOUBLE_EQ(g, 0.);
    for (const double g : result.RawSensitivity())
        ASSERT_DOUBLE_EQ(g, 0.);
}

TEST(ScriptCurveQuoteRiskTest, TestPvRiskPropagatesRejectedSnapshotAndUnsupportedContract) {
    const auto spec = DepositSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    const auto unavailable = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec);
    ASSERT_FALSE(unavailable.Available());
    ASSERT_EQ(unavailable.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
    ASSERT_EQ(unavailable.Axes().quotes_[0].id_, "q");
    const auto tight = TightFitSpec(spec);
    const auto admitted = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(tight, AadOptions()), Accuracy());
    auto definition = spec.observations_[0]->Instrument()->Definition()->Spec();
    definition.source_.rows_.back().text_ = "PAY PAYS N*MAX(1+K*ALPHA,0)";
    const auto instrument = NewScriptCalibrationInstrument({"unsupported", NewScriptInstrumentDefinition(definition), {}, MATURITY});
    const auto rejected = ValueCalibrationInstrumentWithQuoteRisk(instrument, admitted, tight);
    ASSERT_FALSE(rejected.Available());
    ASSERT_EQ(rejected.Diagnostics()[0].instrumentId_, "unsupported");
    ASSERT_TRUE(rejected.Diagnostics()[0].sourceRow_);
    ASSERT_TRUE(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), admitted, tight).Available());
}

TEST(ScriptCurveQuoteRiskTest, TestOneFiveFifteenDepositResponsesAgainstFullRecalibration) {
    for (const int count : {1, 5, 15})
        AssertMappingReference(MappingSample("Deposit", count), "Deposit");
}

TEST(ScriptCurveQuoteRiskTest, TestOneFiveFifteenIrsResponsesAgainstFullRecalibration) {
    for (const int count : {1, 5, 15})
        AssertMappingReference(MappingSample("IRS", count), "IRS");
}

TEST(ScriptCurveQuoteRiskTest, TestOneFiveFifteenBasisResponsesAgainstFullRecalibration) {
    for (const int count : {1, 5, 15})
        AssertMappingReference(MappingSample("Basis", count), "Basis");
}

TEST(ScriptCurveQuoteRiskTest, TestMappingIgnoresOptionalFitJacobianAndOwnsFailure) {
    const auto spec = TightFitSpec(DepositSpec());
    auto fit = CalibrateCurveInstruments(spec, AadOptions());
    fit.quoteJacobian_ = Matrix_<>(1, 1, std::numeric_limits<double>::quiet_NaN());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.MappingAvailable());
    const auto rejected = [&] {
        auto changed = spec;
        changed.market_.routes_.clear();
        return snapshot.ParameterQuoteMapping(changed);
    }();
    ASSERT_FALSE(rejected.Available());
    ASSERT_EQ(rejected.Axes().parameters_[0].id_, snapshot.ParameterAxis()[0].id_);
    ASSERT_EQ(rejected.Diagnostics()[0].field_, "/context/market/routes");
    ASSERT_THROW(static_cast<void>(rejected.ParameterResponse()), Exception_);
    ASSERT_TRUE(snapshot.ParameterQuoteMapping(spec).Available());
}

TEST(ScriptCurveQuoteRiskTest, TestRankAndConditionGatesAreIndependentOfRootResidual) {
    auto spec = TwoDepositSpec();
    spec.observations_[1] = NewCalibrationObservation("o2", spec.observations_[0]->Instrument(), "ParRate",
                                                      NewCalibrationMarketQuote("q2", .035, CalibrationValueUnit_::Parse("Rate")), 1e-13);
    spec = TightFitSpec(spec);
    const auto singular = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(2));
    ASSERT_TRUE(singular.StructureValid());
    ASSERT_FALSE(singular.MappingAvailable());
    ASSERT_EQ(singular.Diagnostics()[0].code_, "ScriptCurveQuoteRiskJacobianRejected");
    ASSERT_EQ(singular.Numerics().nativeJacobian_.Rows(), 2);
    ASSERT_LE(std::abs(singular.Numerics().rawResiduals_[0]), 1e-12);
    const auto regularSpec = TightFitSpec(TwoDepositSpec());
    const auto fit = CalibrateCurveInstruments(regularSpec, AadOptions());
    const auto regular = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(2));
    ASSERT_TRUE(regular.MappingAvailable());
    auto strict = MappingAccuracy(2);
    strict.minReciprocalCondition_ = 1.;
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, strict);
    ASSERT_FALSE(rejected.MappingAvailable());
    ASSERT_EQ(rejected.Diagnostics()[0].code_, "ScriptCurveQuoteRiskConditionRejected");
    ASSERT_EQ(rejected.Numerics().reciprocalCondition_, regular.Numerics().reciprocalCondition_);
    ASSERT_EQ(rejected.Numerics().factorizationCount_, 1);
}

TEST(ScriptCurveQuoteRiskTest, TestResponseAccuracyRejectsAfterRootAndRankPass) {
    const auto spec = TightFitSpec(TwoDepositSpec());
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    const auto accepted = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(2));
    ASSERT_TRUE(accepted.MappingAvailable());
    double error = 0.;
    for (const double value : accepted.Numerics().transposeBackwardErrors_)
        error = std::max(error, value);
    for (const double value : accepted.Numerics().responseBackwardErrors_)
        error = std::max(error, value);
    ASSERT_GT(error, 0.);
    auto accuracy = MappingAccuracy(2);
    accuracy.responseBackwardErrorLimit_ = error / 2.;
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
    ASSERT_TRUE(rejected.StructureValid());
    ASSERT_FALSE(rejected.MappingAvailable());
    ASSERT_EQ(rejected.Diagnostics()[0].code_, "ScriptCurveQuoteRiskResponseAccuracy");
    const auto result = rejected.ParameterQuoteMapping(spec);
    ASSERT_FALSE(result.Available());
    ASSERT_EQ(result.Numerics().factorizationCount_, 1);
    ASSERT_EQ(result.Numerics().responseBackwardErrors_.size(), 2u);
    ASSERT_THROW(static_cast<void>(result.ParameterResponse()), Exception_);
}

TEST(ScriptCurveQuoteRiskTest, TestReorderedContextRejectsOldMapAndPermutesRebuiltColumns) {
    auto spec = TightFitSpec(TwoDepositSpec());
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(2));
    const auto original = snapshot.ParameterQuoteMapping(spec);
    ASSERT_TRUE(original.Available());
    std::swap(spec.observations_[0], spec.observations_[1]);
    ASSERT_FALSE(snapshot.ParameterQuoteMapping(spec).Available());
    const auto reordered =
        NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(2)).ParameterQuoteMapping(spec);
    ASSERT_TRUE(reordered.Available());
    ASSERT_EQ(reordered.Axes().quotes_[0].id_, original.Axes().quotes_[1].id_);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(reordered.ParameterResponse()(i, j), original.ParameterResponse()(i, 1 - j), 1e-11);
}

namespace {
    CurveInstrumentCalibrationSpec_ LayoutSpec(CurveParameterization_ mode) {
        auto spec = TightFitSpec(TwoDepositSpec());
        spec.geometry_.parameterization_ = mode;
        if (mode == CurveParameterization_::Value_::ZERO_RATE)
            spec.geometry_.knotDates_.erase(spec.geometry_.knotDates_.begin());
        if (mode == CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD) {
            spec = TightFitSpec(DepositSpec());
            spec.geometry_.parameterization_ = mode;
            spec.geometry_.knotDates_ = {MATURITY};
        }
        return spec;
    }
    void AssertScaleInvariance(const CurveInstrumentCalibrationSpec_& original) {
        const int count = static_cast<int>(original.observations_.size());
        const auto base = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(original, AadOptions()), MappingAccuracy(count));
        ASSERT_TRUE(base.MappingAvailable()) << base.Diagnostics().front().message_;
        const auto mapping = base.ParameterQuoteMapping(original);
        for (const double scale : {1e-4, 1., 10.}) {
            auto spec = original;
            spec.solver_.parameterScales_ = Vector_<>(count, scale);
            for (int i = 0; i < count; ++i)
                spec = ReplaceQuote(spec, i, spec.observations_[i]->Quote()->Value(), (3. + 2. * i) * 1e-13);
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(count));
            ASSERT_TRUE(snapshot.MappingAvailable()) << snapshot.Diagnostics().front().message_;
            const auto scaled = snapshot.ParameterQuoteMapping(spec);
            ASSERT_TRUE(scaled.Available());
            for (int i = 0; i < count; ++i)
                for (int j = 0; j < count; ++j)
                    ASSERT_NEAR(scaled.ParameterResponse()(i, j), mapping.ParameterResponse()(i, j), 1e-10);
        }
    }
} // namespace

TEST(ScriptCurveQuoteRiskTest, TestFullRankNativeLayoutsAreInvariantToParameterAndErrorScales) {
    for (const auto mode : {CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::ZERO_RATE,
                            CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD}) {
        const auto spec = LayoutSpec(mode);
        AssertScaleInvariance(spec);
        AssertMappingReference(spec, CurveParameterization_(mode).String());
    }
}

TEST(ScriptCurveQuoteRiskTest, TestWideErrorScalesDoNotChangeAnAccurateNativeResponse) {
    for (const auto mode : {CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::ZERO_RATE,
                            CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD}) {
        auto spec = LayoutSpec(mode);
        const int count = static_cast<int>(spec.observations_.size());
        const auto fit = CalibrateCurveInstruments(spec, AadOptions());
        const auto original = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(count)).ParameterQuoteMapping(spec);
        ASSERT_TRUE(original.Available());
        spec.solver_.initialGuessPerNode_ = fit.parameters_;
        for (int i = 0; i < count; ++i)
            spec = ReplaceQuote(spec, i, spec.observations_[i]->Quote()->Value(), (i + 1.) * .01);
        const auto scaled =
            NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(count)).ParameterQuoteMapping(spec);
        ASSERT_TRUE(scaled.Available());
        for (int i = 0; i < count; ++i)
            for (int j = 0; j < count; ++j)
                ASSERT_NEAR(scaled.ParameterResponse()(i, j), original.ParameterResponse()(i, j), 1e-12);
    }
}

TEST(ScriptCurveQuoteRiskTest, TestCachedMappingIsPassiveAndConcurrentAndSurvivesInputs) {
    const auto spec = TightFitSpec(TwoDepositSpec());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(2));
    ASSERT_TRUE(snapshot.MappingAvailable());
    auto future = std::async(std::launch::async, [&] { return snapshot.ParameterQuoteMapping(spec); });
    AAD::RecordingScope_ outer;
    AAD::Number_ x;
    outer.RegisterInput(x, 2.);
    outer.StartRecording();
    const auto passive = snapshot.ParameterQuoteMapping(spec);
    ASSERT_TRUE(passive.Available());
    auto y = AAD::Number_(x * x);
    outer.FinishRecording();
    AAD::Adjoint(y) = 1.;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(x), 4.);
    outer.Close();
    const auto concurrent = future.get();
    ASSERT_TRUE(concurrent.Available());
    ASSERT_EQ(concurrent.Numerics().factorizationCount_, 1);
    ASSERT_DOUBLE_EQ(passive.ParameterResponse()(0, 0), concurrent.ParameterResponse()(0, 0));
    const auto owned = [] {
        const auto local = TightFitSpec(DepositSpec());
        return NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(local, AadOptions()), Accuracy()).ParameterQuoteMapping(local);
    }();
    ASSERT_TRUE(owned.Available());
    ASSERT_EQ(owned.Axes().quotes_[0].id_, "q");
    ASSERT_EQ(owned.Numerics().nativeJacobian_.Rows(), 1);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(ScriptCurveQuoteRiskTest, TestFiveFifteenSnapshotsReuseOneFactorizationForRepeatedCheckedMaps) {
    for (const int count : {5, 15}) {
        const auto spec = MappingSample("IRS", count);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, AadOptions()), MappingAccuracy(count));
        ASSERT_TRUE(snapshot.MappingAvailable());
        const auto start = std::chrono::steady_clock::now();
        for (int repeat = 0; repeat < 100; ++repeat) {
            const auto result = snapshot.ParameterQuoteMapping(spec);
            ASSERT_TRUE(result.Available());
            ASSERT_EQ(result.Numerics().factorizationCount_, 1);
            ASSERT_EQ(result.Numerics().responseBackwardErrors_.size(), static_cast<size_t>(count));
        }
        std::cout << "mapping-cache n=" << count << " calls=100 factorizations=" << snapshot.Numerics().factorizationCount_
                  << " elapsed_ms=" << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << '\n';
    }
}

TEST(ScriptCurveQuoteRiskTest, TestLooseInitialFitIsRejectedWithoutRepairAndRetainsEvidence) {
    auto spec = DepositSpec();
    spec = ReplaceQuote(spec, 0, .035, .1);
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_EQ(fit.evaluations_, 1);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.MappingAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
    ASSERT_EQ(snapshot.Diagnostics()[0].observationId_, "o");
    ASSERT_EQ(snapshot.Diagnostics()[0].quoteId_, "q");
    ASSERT_GT(std::abs(snapshot.Numerics().rawResiduals_[0]), 1e-3);
    ASSERT_EQ(snapshot.Numerics().factorizationCount_, 0);
    ASSERT_EQ(snapshot.Parameters(), fit.parameters_);
    ASSERT_FALSE(snapshot.ParameterQuoteMapping(spec).Available());
}

TEST(ScriptCurveQuoteRiskTest, TestExplicitPivotGateRejectsWithoutPublishingAResponse) {
    const auto spec = TightFitSpec(TwoDepositSpec());
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    auto accuracy = MappingAccuracy(2);
    accuracy.relativePivotTolerance_ = .9;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.MappingAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskJacobianRejected");
    ASSERT_EQ(snapshot.Numerics().nativeJacobian_.Rows(), 2);
    ASSERT_FALSE(snapshot.Numerics().reciprocalCondition_);
    ASSERT_THROW(static_cast<void>(snapshot.ParameterQuoteMapping(spec).ParameterResponse()), Exception_);
}

TEST(ScriptCurveQuoteRiskTest, TestNestedSnapshotRecordingRejectsAndOuterRecordingSurvives) {
    const auto spec = TightFitSpec(DepositSpec());
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    {
        AAD::RecordingScope_ outer;
        AAD::Number_ x;
        outer.RegisterInput(x, 3.);
        outer.StartRecording();
        const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        ASSERT_TRUE(rejected.StructureValid());
        ASSERT_FALSE(rejected.MappingAvailable());
        ASSERT_EQ(rejected.QuoteAxis()[0].id_, "q");
        ASSERT_EQ(rejected.Numerics().modelMeasures_.size(), 1u);
        ASSERT_EQ(rejected.Numerics().rawResiduals_.size(), 1u);
        ASSERT_LE(std::abs(rejected.Numerics().rawResiduals_[0]), 1e-12);
        auto y = AAD::Number_(x * x);
        outer.FinishRecording();
        AAD::Adjoint(y) = 1.;
        outer.Reverse();
        ASSERT_DOUBLE_EQ(AAD::Adjoint(x), 6.);
    }
    ASSERT_TRUE(NewScriptCurveQuoteRiskSnapshot(fit, Accuracy()).MappingAvailable());
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(ScriptCurveQuoteRiskTest, TestNativeLayoutsGiveTheSamePhysicalDepositResponse) {
    for (const auto mode : {CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::ZERO_RATE,
                            CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD}) {
        const auto spec = LayoutSpec(mode);
        const auto fit = CalibrateCurveInstruments(spec, AadOptions());
        const int n = static_cast<int>(spec.observations_.size());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(n));
        const auto map = snapshot.ParameterQuoteMapping(spec);
        ASSERT_TRUE(map.Available());
        Matrix_<> logDfJacobian(n, n);
        for (int row = 0; row < n; ++row) {
            const auto end = *spec.observations_[row]->Instrument()->Pillar();
            for (int parameter = 0; parameter < n; ++parameter) {
                auto up = fit.parameters_, down = fit.parameters_;
                up[parameter] += 1e-6;
                down[parameter] -= 1e-6;
                const auto plus = BuildDiscountCurveT<double>(fit.definition_, up), minus = BuildDiscountCurveT<double>(fit.definition_, down);
                logDfJacobian(row, parameter) = (std::log((*plus)(TODAY, end)) - std::log((*minus)(TODAY, end))) / 2e-6;
            }
        }
        for (int i = 0; i < n; ++i) {
            const double alpha = (*spec.observations_[i]->Instrument()->Pillar() - TODAY) / 360.;
            const auto risk = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[i]->Instrument(), snapshot, spec);
            ASSERT_TRUE(risk.Available());
            const double df = 1. / (1. + alpha * spec.observations_[i]->Quote()->Value());
            for (int j = 0; j < n; ++j) {
                double actual = 0.;
                for (int parameter = 0; parameter < n; ++parameter)
                    actual += logDfJacobian(i, parameter) * map.ParameterResponse()(parameter, j);
                const double expected = i == j ? -alpha * df : 0.;
                ASSERT_NEAR(actual, expected, 1e-9);
                ASSERT_NEAR(risk.RawSensitivity()[j], 100. * (1. + .025 * alpha) * df * expected, 1e-9);
            }
        }
    }
}

namespace {
    void AssertUnobservedFinalForward(CurveParameterization_ mode) {
        for (const double scale : {1e-4, 1., 10.}) {
            auto spec = TightFitSpec(TwoDepositSpec());
            spec.geometry_.parameterization_ = mode;
            spec.geometry_.knotDates_ = {*spec.observations_[1]->Instrument()->Pillar()};
            if (mode == CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD)
                spec.geometry_.knotDates_ = {MATURITY, *spec.observations_[1]->Instrument()->Pillar()};
            spec.solver_.initialGuessPerNode_ = {.03, .04};
            spec.solver_.parameterScales_ = {scale, scale};
            const auto initial = PreparedCurveInstruments_(spec).Evaluate({.03, .04}, false);
            for (int i = 0; i < 2; ++i)
                spec = ReplaceQuote(spec, i, initial.modelMeasures_[i], (3. + i) * 1e-13);
            const auto fit = CalibrateCurveInstruments(spec, AadOptions());
            ASSERT_EQ(fit.evaluations_, 1);
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(2));
            ASSERT_TRUE(snapshot.StructureValid());
            ASSERT_FALSE(snapshot.MappingAvailable());
            ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskJacobianRejected");
            for (int i = 0; i < 2; ++i) {
                ASSERT_LE(std::abs(snapshot.Numerics().rawResiduals_[i]), 1e-12);
                ASSERT_DOUBLE_EQ(snapshot.Numerics().nativeJacobian_(i, 1), 0.);
            }
        }
    }
} // namespace

TEST(ScriptCurveQuoteRiskTest, TestPwlfUnobservedFinalRightForwardIsRejectedForAllScales) {
    AssertUnobservedFinalForward(CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD);
}

TEST(ScriptCurveQuoteRiskTest, TestMultiKnotPwcUnobservedFinalRightForwardIsRejectedForAllScales) {
    AssertUnobservedFinalForward(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD);
}

TEST(ScriptCurveQuoteRiskTest, TestFraAndPeriodForwardOisParameterResponses) {
    auto spec = TightFitSpec(TwoDepositSpec());
    const auto end = *spec.observations_[1]->Instrument()->Pillar();
    const CurveScriptImportSettings_ settings{"forward", Ccy_("USD"), "USD.OIS", 1.};
    const Vector_<Handle_<CalibrationInstrument_>> instruments = {
        NewFraScriptInstrument(FRA_(TODAY, MATURITY, end, .025, MappingIndex()), end, settings),
        NewPeriodForwardOisScriptInstrument(OISSwap_(TODAY, MATURITY, end, .025, MappingLeg("12M"), MappingIndex(), MappingLeg("3M")), settings)};
    for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
        const auto& instrument = instruments[i];
        spec.observations_[1] = NewCalibrationObservation("forward-o", instrument, "ParRate",
                                                          NewCalibrationMarketQuote("forward-q", .04, CalibrationValueUnit_::Parse("Rate")), 1e-13);
        AssertMappingReference(spec, i == 0 ? "FRA" : "PeriodForwardOIS");
        AssertPriceReference(spec, i == 0 ? "FRA" : "PeriodForwardOIS");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestNumericalRejectionOutlivesSnapshotAndFit) {
    const auto retained = [] {
        const auto spec = DepositSpec();
        return NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy()).ParameterQuoteMapping(spec);
    }();
    ASSERT_FALSE(retained.Available());
    ASSERT_EQ(retained.Axes().quotes_[0].id_, "q");
    ASSERT_EQ(retained.Axes().parameters_[0].id_, "LOG_DISCOUNT_FACTOR:2027-10-21");
    ASSERT_EQ(retained.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
    ASSERT_GT(std::abs(retained.Numerics().rawResiduals_[0]), 1e-12);
    ASSERT_EQ(retained.Numerics().nativeJacobian_.Rows(), 1);
    ASSERT_THROW(static_cast<void>(retained.ParameterResponse()), Exception_);
}

TEST(ScriptCurveQuoteRiskTest, TestIllConditionedFullRankRootIsRejectedWithOwnedConditionEvidence) {
    auto spec = DepositSpec();
    const auto end = MATURITY.AddDays(1);
    const auto instrument = NewDepositScriptInstrument(Deposit_(TODAY, end, .025, DayBasis_("ACT_360")), {"nearby", Ccy_("USD"), "USD.OIS", 1.});
    spec.geometry_.knotDates_.push_back(Date_(2126, 10, 21));
    spec.observations_.push_back(NewCalibrationObservation("nearby-o", instrument, "ParRate",
                                                           NewCalibrationMarketQuote("nearby-q", .035, CalibrationValueUnit_::Parse("Rate")), 1e-13));
    spec.solver_.initialGuessPerNode_ = {-.03, -.5};
    const auto initial = PreparedCurveInstruments_(spec).Evaluate({-.03, -.5}, false);
    for (int i = 0; i < 2; ++i)
        spec = ReplaceQuote(spec, i, initial.modelMeasures_[i]);
    const auto fit = CalibrateCurveInstruments(spec, AadOptions());
    const auto accepted = NewScriptCurveQuoteRiskSnapshot(fit, MappingAccuracy(2));
    ASSERT_TRUE(accepted.MappingAvailable());
    ASSERT_LT(*accepted.Numerics().reciprocalCondition_, 1e-4);
    auto accuracy = MappingAccuracy(2);
    accuracy.minReciprocalCondition_ = 1e-4;
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, accuracy).ParameterQuoteMapping(spec);
    ASSERT_FALSE(rejected.Available());
    ASSERT_EQ(rejected.Diagnostics()[0].code_, "ScriptCurveQuoteRiskConditionRejected");
    ASSERT_EQ(rejected.Numerics().reciprocalCondition_, accepted.Numerics().reciprocalCondition_);
    ASSERT_EQ(rejected.Axes().quotes_[1].id_, "nearby-q");
    std::cout << "mapping-condition rcond=" << *accepted.Numerics().reciprocalCondition_ << " rejected_min=0.0001\n";
}

TEST(ScriptCurveQuoteRiskTest, TestAccurateFitHasNoNumericalRejectionWithoutFitJacobian) {
    auto spec = DepositSpec();
    const auto& row = *spec.observations_[0];
    spec.observations_[0] = NewCalibrationObservation(row.Id(), row.Instrument(), row.MeasureId(), row.Quote(), 1e-13);
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.quoteJacobian_.Empty());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_TRUE(snapshot.Diagnostics().empty()) << snapshot.Diagnostics().front().message_;
    ASSERT_TRUE(snapshot.MappingAvailable());
    const auto mapping = snapshot.ParameterQuoteMapping(spec);
    ASSERT_TRUE(mapping.Available());
    const double alpha = (MATURITY - TODAY) / 360.;
    ASSERT_NEAR(mapping.ParameterResponse()(0, 0), -alpha / (1. + .035 * alpha), 1e-12);
}

TEST(ScriptCurveQuoteRiskTest, TestOwnedStructureIsSeparateFromNumericalAvailability) {
    auto spec = DepositSpec();
    auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.quoteJacobian_.Empty());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics().size(), 1u);
    ASSERT_FALSE(snapshot.MappingAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
    ASSERT_GT(std::abs(snapshot.Numerics().rawResiduals_[0]), snapshot.Accuracy().residualAbsoluteLimits_[0]);
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

TEST(ScriptCurveQuoteRiskTest, TestWritableObservationAliasCannotChangeSnapshot) {
    auto spec = DepositSpec();
    const auto alias = std::make_shared<CalibrationObservation_>(*spec.observations_[0]);
    spec.observations_[0] = Handle_<CalibrationObservation_>(alias);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    const auto identity = snapshot.Identity();
    *alias = *NewCalibrationObservation("changed", alias->Instrument(), "ParRate",
                                        NewCalibrationMarketQuote("changed-q", .07, CalibrationValueUnit_::Parse("Rate")), 1e-8);
    ASSERT_EQ(snapshot.ResolvedSpec()->observations_[0]->Id(), "o");
    ASSERT_DOUBLE_EQ(snapshot.ResolvedSpec()->observations_[0]->Quote()->Value(), .035);
    ASSERT_DOUBLE_EQ(snapshot.QuoteAxis()[0].value_, .035);
    ASSERT_EQ(snapshot.Identity(), identity);
    ASSERT_TRUE(snapshot.ValidateContext(*snapshot.ResolvedSpec()).Passed());
    ASSERT_FALSE(snapshot.ValidateContext(spec).Passed());
}

TEST(ScriptCurveQuoteRiskTest, TestWritableQuoteAliasCannotChangeSnapshot) {
    auto spec = DepositSpec();
    const auto alias = std::make_shared<CalibrationMarketQuote_>(*spec.observations_[0]->Quote());
    spec.observations_[0] =
        NewCalibrationObservation("o", spec.observations_[0]->Instrument(), "ParRate", Handle_<CalibrationMarketQuote_>(alias), 1e-10);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec), Accuracy());
    ASSERT_TRUE(snapshot.StructureValid());
    const auto identity = snapshot.Identity();
    *alias = *NewCalibrationMarketQuote("q", .08, CalibrationValueUnit_::Parse("Rate"));
    ASSERT_DOUBLE_EQ(snapshot.ResolvedSpec()->observations_[0]->Quote()->Value(), .035);
    ASSERT_DOUBLE_EQ(snapshot.QuoteAxis()[0].value_, .035);
    ASSERT_EQ(snapshot.Identity(), identity);
    ASSERT_TRUE(snapshot.ValidateContext(*snapshot.ResolvedSpec()).Passed());
    ASSERT_FALSE(snapshot.ValidateContext(spec).Passed());
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

TEST(ScriptCurveQuoteRiskTest, TestReplacedParameterScalesCannotReuseSolveEvidence) {
    auto fit = CalibrateCurveInstruments(DepositSpec());
    auto changed = std::make_shared<CurveInstrumentCalibrationSpec_>(*fit.resolvedSpec_);
    changed->solver_.parameterScales_ = {10.};
    fit.resolvedSpec_ = changed;
    ASSERT_DOUBLE_EQ(fit.parameterScales_[0], 1.);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/configuration_identity");
    ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
}

TEST(ScriptCurveQuoteRiskTest, TestReplacedBudgetCannotReuseSolveEvidence) {
    auto fit = CalibrateCurveInstruments(DepositSpec());
    auto changed = std::make_shared<CurveInstrumentCalibrationSpec_>(*fit.resolvedSpec_);
    changed->solver_.maxEvaluations_ = 1;
    fit.resolvedSpec_ = changed;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.StructureValid());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/configuration_identity");
    ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
}

TEST(ScriptCurveQuoteRiskTest, TestEverySolverSettingIsBoundToPreparationEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(CurveInstrumentSolver_*)>;
    const Vector_<Change_> changes = {[](auto* s) { s->initialGuess_ = .06; },     [](auto* s) { s->initialGuessPerNode_ = {-.04}; },
                                      [](auto* s) { s->parameterScales_ = {.3}; }, [](auto* s) { s->smoothingWeight_ = .2; },
                                      [](auto* s) { s->maxEvaluations_ = 201; },   [](auto* s) { s->maxRestarts_ = 21; },
                                      [](auto* s) { s->fitWeight_ = 1.; },         [](auto* s) { s->fitNormTolerance_ = .01; }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = fit;
        auto spec = std::make_shared<CurveInstrumentCalibrationSpec_>(*fit.resolvedSpec_);
        changes[i](&spec->solver_);
        changed.resolvedSpec_ = spec;
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestEveryOptionIsBoundToPreparationEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(CurveInstrumentCalibrationOptions_*)>;
    const Vector_<Change_> changes = {[](auto* o) { o->evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic; },
                                      [](auto* o) { o->jacobianMode_ = CalibrationDerivative_::Value_::AAD; },
                                      [](auto* o) { o->differenceStep_ = 2e-5; }, [](auto* o) { o->computeQuoteJacobian_ = true; }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = fit;
        auto options = std::make_shared<CurveInstrumentCalibrationOptions_>(*fit.resolvedOptions_);
        changes[i](options.get());
        changed.resolvedOptions_ = options;
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestNonDefaultSolverOptionsAndInstrumentKnotsRemainValid) {
    for (const double scale : {1e-4, 1., 10.}) {
        SCOPED_TRACE(scale);
        auto spec = DepositSpec();
        spec.geometry_.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
        spec.solver_.parameterScales_ = {scale};
        spec.solver_.maxEvaluations_ = 250;
        spec.solver_.maxRestarts_ = 25;
        CurveInstrumentCalibrationOptions_ options;
        options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        options.differenceStep_ = 2e-5;
        options.computeQuoteJacobian_ = true;
        options.preparationCache_ = std::make_shared<CalibrationPreparationCache_>();
        const auto fit = CalibrateCurveInstruments(spec, options);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        ASSERT_TRUE(snapshot.StructureValid());
        ASSERT_TRUE(snapshot.ValidateContext(spec).Passed());
        ASSERT_TRUE(snapshot.ValidateContext(*snapshot.ResolvedSpec()).Passed());
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_DOUBLE_EQ(fit.parameterScales_[0], scale);
        ASSERT_FALSE(fit.resolvedOptions_->preparationCache_);
        ASSERT_FALSE(fit.quoteJacobian_.Empty());
    }
}

TEST(ScriptCurveQuoteRiskTest, TestSnapshotIdentityIncludesOriginalOptions) {
    const auto spec = DepositSpec();
    const auto firstFit = CalibrateCurveInstruments(spec);
    CurveInstrumentCalibrationOptions_ options;
    options.computeQuoteJacobian_ = true;
    const auto secondFit = CalibrateCurveInstruments(spec, options);
    ASSERT_EQ(firstFit.parameters_, secondFit.parameters_);
    const auto first = NewScriptCurveQuoteRiskSnapshot(firstFit, Accuracy());
    const auto second = NewScriptCurveQuoteRiskSnapshot(secondFit, Accuracy());
    ASSERT_TRUE(first.StructureValid());
    ASSERT_TRUE(second.StructureValid());
    ASSERT_NE(first.Identity(), second.Identity());
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

TEST(ScriptCurveQuoteRiskTest, TestObjectiveValuesMustMatchOriginalSolveEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(CurveInstrumentCalibrationResult_*)>;
    const Vector_<Change_> changes = {[](auto* f) { f->parameterScales_[0] = 10.; }, [](auto* f) { f->initialParameters_[0] += .01; },
                                      [](auto* f) { f->fitWeight_ = 2.; }, [](auto* f) { f->fitNormTolerance_ = .01; }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = fit;
        changes[i](&changed);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/objective_identity");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestReportObjectiveMustMatchOriginalSolveEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    using Change_ = std::function<void(CalibrationReportSnapshot_*)>;
    const Vector_<Change_> changes = {[](auto* r) { r->objective_.parameterScales_[0] = 10.; },
                                      [](auto* r) { r->objective_.theta0_[0] += .01; },
                                      [](auto* r) { r->objective_.lambda_ = 2.; },
                                      [](auto* r) { r->objective_.eta_ = .01; },
                                      [](auto* r) { (*r->objective_.weightMatrix_)(0, 0) *= 2.; },
                                      [](auto* r) { r->objective_.nativeLayout_[0] = "other"; },
                                      [](auto* r) { r->parameters_[0].scale_ = 10.; },
                                      [](auto* r) { r->parameters_[0].initial_ = .01; },
                                      [](auto* r) { r->parameters_[0].value_ = .01; }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = fit;
        auto report = fit.report_->Data();
        changes[i](&report);
        changed.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report));
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/report/objective");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestJointMetricEditsCannotReuseOriginalSolveEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(0, 0), 1.);
    for (const double weight : {-1., 2.}) {
        SCOPED_TRACE(weight);
        auto changed = fit;
        auto report = fit.report_->Data();
        changed.nativeWeights_(0, 0) = weight;
        (*report.objective_.weightMatrix_)(0, 0) = weight;
        changed.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report));
        ASSERT_EQ(changed.evidence_, fit.evidence_);
        ASSERT_EQ(changed.objectiveIdentity_, fit.objectiveIdentity_);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/native_weights");
        ASSERT_EQ(snapshot.QuoteAxis()[0].id_, "q");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestEachMetricCopyIsCheckedAgainstOriginalSolveEvidence) {
    const auto fit = CalibrateCurveInstruments(DepositSpec());
    for (const bool reportOnly : {false, true}) {
        SCOPED_TRACE(reportOnly);
        auto changed = fit;
        auto report = fit.report_->Data();
        auto& metric = reportOnly ? *report.objective_.weightMatrix_ : changed.nativeWeights_;
        metric(0, 0) = 2.;
        changed.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report));
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, Accuracy());
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, reportOnly ? "/fit/report/objective" : "/fit/native_weights");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestMetricShapeAndNonFiniteEditsCannotReuseSolveEvidence) {
    const auto fit = CalibrateCurveInstruments(TwoDepositSpec());
    using Change_ = std::function<void(Matrix_<>*)>;
    const Vector_<Change_> changes = {[](auto* metric) {
                                          Matrix_<> reshaped(1, 4);
                                          for (int j = 0; j < 4; ++j)
                                              reshaped(0, j) = (*metric)(j / 2, j % 2);
                                          *metric = std::move(reshaped);
                                      },
                                      [](auto* metric) { (*metric)(0, 0) = std::numeric_limits<double>::quiet_NaN(); },
                                      [](auto* metric) { (*metric)(0, 0) = std::numeric_limits<double>::infinity(); }};
    for (size_t i = 0; i < changes.size(); ++i) {
        SCOPED_TRACE(i);
        auto changed = fit;
        changes[i](&changed.nativeWeights_);
        auto report = fit.report_->Data();
        report.objective_.weightMatrix_ = changed.nativeWeights_;
        if (std::isfinite(changed.nativeWeights_(0, 0)))
            changed.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report));
        else
            ASSERT_THROW(CalibrationReport_(CalibrationStage_::Value_::Solve, fit.axes_, {}, report), CalibrationError_);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(changed, {{1e-12, 1e-12}, 1e-10, 1e-12});
        ASSERT_FALSE(snapshot.StructureValid());
        ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskIdentityMismatch");
        ASSERT_EQ(snapshot.Diagnostics()[0].field_, "/fit/native_weights");
    }
}

TEST(ScriptCurveQuoteRiskTest, TestOriginalMetricWithNegativeOffDiagonalRemainsValid) {
    auto spec = TwoDepositSpec();
    spec.solver_.smoothingWeight_ = .1;
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(0, 0), .2);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(0, 1), -.1);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(1, 0), -.1);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(1, 1), .2);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, {{1e-12, 1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.StructureValid());
    ASSERT_TRUE(snapshot.ValidateContext(spec).Passed());
    ASSERT_TRUE(snapshot.RiskAvailable());
    ASSERT_TRUE(ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec).Available());
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

TEST(ScriptCurveQuoteRiskTest, TestResponseAccuracyCannotExceedUnitBackwardError) {
    auto accuracy = Accuracy();
    accuracy.responseBackwardErrorLimit_ = 1.01;
    ASSERT_THROW(static_cast<void>(NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(DepositSpec()), accuracy)), CalibrationError_);
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
