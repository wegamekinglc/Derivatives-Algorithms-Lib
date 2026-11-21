#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>

#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/archive.hpp>

using namespace Dal;

namespace {
    class ConstantRateDeposit_ : public Deposit_ {
        struct ConstantRate_ : YCInstrument_::Rate_ {
            double value_;
            explicit ConstantRate_(double value) : value_(value) {}
            double operator()(const YieldCurve_&) const override { return value_; }
        };

    public:
        using Deposit_::Deposit_;
        Handle_<Rate_> Precompute(const Handle_<YieldCurve_>&) const override { return Handle_<Rate_>(new ConstantRate_(MarketRate())); }
    };

    class ThrowingDeposit_ : public Deposit_ {
    public:
        using Deposit_::Deposit_;
        Handle_<Rate_> Precompute(const Handle_<YieldCurve_>&) const override { throw std::domain_error("Deposit fixing outside pricing domain"); }
    };

    class ConstantDiscountCurve_ : public DiscountCurve_ {
        double value_;

    public:
        ConstantDiscountCurve_(const String_& name, const String_& ccy, double value) : DiscountCurve_(name, ccy), value_(value) {}

        double operator()(const Date_&, const Date_&) const override { return value_; }

        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }

        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}

        [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_& newName, const YCComponent_::substitutions_t&) const override {
            return std::make_unique<ConstantDiscountCurve_>(newName, ccy_.String(), value_);
        }

        void Write(Archive::Store_&) const override {}
    };

    Vector_<Handle_<YCInstrument_>> MakeCalibrationInstruments(const Date_& today) {
        const DayBasis_ basis("ACT_365F");
        return {
            Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0125, basis)),
            Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0135, basis)),
            Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0150, 6, basis)),
        };
    }

    CurveCalibrationSpec_ MakeValidSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.instruments_ = MakeCalibrationInstruments(spec.today_);
        spec.knotDates_ = {Date::AddMonths(spec.today_, 3), Date::AddMonths(spec.today_, 6), Date::AddMonths(spec.today_, 12)};
        return spec;
    }

    MultiCurveCalibrationSpec_ MakeStagedRiskSpec() {
        MultiCurveCalibrationSpec_ spec;
        spec.ccy_ = "EUR";
        for (const bool discount : {true, false}) {
            CurveCalibrationSpec_ stage;
            stage.curveName_ = discount ? "ESTR" : "Euribor 6M";
            stage.today_ = Date_(2024, 1, 15);
            stage.calibrateDiscountCurve_ = discount;
            stage.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
            stage.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
            stage.tolerance_ = discount ? 1e-13 : 5e-13;
            RateIndexConvention_ index;
            if (!discount) {
                stage.targetTenor_ = PeriodLength_("6M");
                index.forecastTenor_ = stage.targetTenor_;
                index.useProjectionCurve_ = true;
            }
            for (const int months : {6, 12}) {
                const double rate = (discount ? 0.02 : 0.03) + 0.0001 * months;
                stage.instruments_.emplace_back(new Deposit_(stage.today_, stage.today_, Date::AddMonths(stage.today_, months), rate, index));
            }
            spec.stages_.push_back(stage);
        }
        return spec;
    }

    CurveCalibrationSpec_ MakeZeroRateCalibrationSpec(LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR,
                                                      CurveSolveMode_ solveMode = CurveSolveMode_::Value_::EXACT,
                                                      const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.curveName_ = "zero_rate_calibrated";
        spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = solveMode;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-11;
        spec.fitTolerance_ = 1.0e-9;
        spec.smoothingWeight_ = 1.0;
        spec.initialGuess_ = 0.01;
        spec.logDfScheme_ = scheme;
        spec.baseCurve_ = base;
        spec.knotDates_ = {
            Date::AddMonths(spec.today_, 3),
            Date::AddMonths(spec.today_, 6),
            Date::AddMonths(spec.today_, 12),
            Date::AddMonths(spec.today_, 24),
        };

        const Vector_<> targetRates = {0.011, 0.014, 0.017, 0.021};
        const Handle_<DiscountCurve_> target(
            NewDiscountZeroRate("zero_rate_market", spec.ccy_, spec.today_, spec.knotDates_, targetRates, spec.liborBasis_, scheme, base));
        const CurveBlock_ market("zero_rate_market", spec.ccy_, {{spec.targetCollateral_, target}}, {}, spec.liborBasis_);
        Handle_<YieldCurve_> empty;
        for (const auto& maturity : spec.knotDates_) {
            const Handle_<YCInstrument_> prototype(new Deposit_(spec.today_, maturity, 0.0, spec.liborBasis_));
            const double marketRate = (*prototype->Precompute(empty))(market);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, maturity, marketRate, spec.liborBasis_)));
        }
        return spec;
    }

    void AssertZeroRateCalibration(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& result, double tolerance) {
        ASSERT_NE(result.curve_, nullptr);
        const auto* zeroRate = dynamic_cast<const DiscountZeroRate_*>(result.curve_.get());
        ASSERT_NE(zeroRate, nullptr);
        ASSERT_EQ(zeroRate->NodeDates(), spec.knotDates_);
        ASSERT_EQ(zeroRate->NodeZeroRates().size(), spec.knotDates_.size());
        ASSERT_LT(result.diagnostics_.maxAbsResidual_, tolerance);
        for (const auto& knot : spec.knotDates_) {
            const double df = (*result.curve_)(spec.today_, knot);
            ASSERT_TRUE(std::isfinite(df));
            ASSERT_GT(df, 0.0);
        }
    }
    void AssertOnlyDiscountFit(const MultiCurveCalibrationResult_& partial,
                               const StagedMultiCurveCalibrationRecord_& record,
                               const MultiCurveCalibrationResult_& plain,
                               const CurveCalibrationSpec_& forward) {
        ASSERT_EQ(record.unavailableStage_, forward.curveName_);
        ASSERT_EQ(record.stages_.size(), 1u);
        ASSERT_EQ(partial.diagnostics_.size(), 1u);
        ASSERT_EQ(partial.diagnostics_[0].residuals_, plain.diagnostics_[0].residuals_);
        ASSERT_EQ(partial.discountCurves_.size(), 1u);
        ASSERT_EQ(partial.discountCurves_.at(forward.targetCollateral_).get(), record.stages_[0].curve_.get());
        ASSERT_TRUE(partial.forwardCurves_.empty());
    }

    void AssertStagedDepositReplay(const CurveCalibrationStageRecord_& stage) {
        ASSERT_EQ(stage.spec_.knotPolicy_, CurveKnotPolicy_::Value_::INPUT);
        ASSERT_EQ(stage.freeParameters_.size(), 2u);
        ASSERT_EQ(stage.solution_, dynamic_cast<const DiscountZeroRate_&>(*stage.curve_).NodeZeroRates());
        for (int i = 0; i < 2; ++i) {
            ASSERT_EQ(stage.freeParameters_[i].date_, Date::AddMonths(stage.spec_.today_, 6 * (i + 1)));
            ASSERT_EQ(stage.spec_.knotDates_[i], stage.freeParameters_[i].date_);
            ASSERT_EQ(stage.freeParameters_[i].component_, CurveFreeParameterComponent_::Value_::ZERO_RATE);
        }
        const auto& spec = stage.spec_;
        const auto definition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.parameterization_, spec.logDfScheme_, spec.knotDates_,
                                                    spec.today_, spec.liborBasis_);
        const Handle_<DiscountCurve_> rebuilt(BuildDiscountCurveUniqueT<double>(definition, stage.solution_, spec.baseCurve_));
        auto discounts = spec.discountCurves_;
        auto forwards = spec.forwardCurves_;
        if (spec.calibrateDiscountCurve_)
            discounts[spec.targetCollateral_] = rebuilt;
        else
            forwards[spec.targetTenor_] = rebuilt;
        const CurveBlock_ replay(spec.curveName_, spec.ccy_, discounts, forwards, spec.liborBasis_);
        for (const auto& instrument : spec.instruments_)
            ASSERT_NEAR((*instrument->Precompute({}))(replay), instrument->MarketRate(), 1e-12);
    }
} // namespace

TEST(CalibrationTest, TestBuildCurveCalibrationWeightsExpandsParametersPerKnot) {
    const Vector_<Date_> knotDates = {Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};

    std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(knotDates, 2, 0.5));

    ASSERT_EQ(weights->Size(), 6);
    ASSERT_TRUE(weights->IsSymmetric());
}

TEST(CalibrationTest, TestStagedRiskRecordOwnsEffectiveRoutesSolutionAndQuoteRows) {
    for (const bool reverse : {false, true}) {
        StagedMultiCurveCalibrationRecord_ record;
        {
            auto spec = MakeStagedRiskSpec();
            if (reverse)
                for (auto& stage : spec.stages_)
                    std::swap(stage.instruments_[0], stage.instruments_[1]);
            const auto result = CalibrateMultiCurve(spec, &record);
            ASSERT_TRUE(record.unavailableReason_.empty()) << record.unavailableReason_;
            ASSERT_EQ(record.stages_.size(), 2u);
            ASSERT_EQ(record.stages_[0].curve_.get(), result.discountCurves_.at(spec.stages_[0].targetCollateral_).get());
            ASSERT_EQ(record.stages_[1].curve_.get(), result.forwardCurves_.at(spec.stages_[1].targetTenor_).get());
            for (int s = 0; s < 2; ++s) {
                const auto& stage = record.stages_[s];
                ASSERT_EQ(stage.spec_.ccy_, spec.ccy_);
                ASSERT_EQ(stage.spec_.curveName_, spec.stages_[s].curveName_);
                ASSERT_EQ(stage.spec_.tolerance_, spec.stages_[s].tolerance_);
                ASSERT_EQ(stage.residualRowByInput_, (reverse ? Vector_<int>{1, 0} : Vector_<int>{0, 1}));
                ASSERT_LE(result.diagnostics_[s].maxAbsResidual_, 1e-12);
                for (int i = 0; i < 2; ++i) {
                    const int row = stage.residualRowByInput_[i];
                    ASSERT_EQ(stage.spec_.instruments_[i]->Name(), "Deposit");
                    ASSERT_EQ(stage.spec_.instruments_[i]->MarketRate(), result.diagnostics_[s].marketRates_[row]);
                }
            }
        }
        const auto& discount = record.stages_[0];
        const auto& forward = record.stages_[1];
        ASSERT_EQ(forward.spec_.baseCurve_.get(), discount.curve_.get());
        ASSERT_EQ(forward.spec_.discountCurves_.at(forward.spec_.targetCollateral_).get(), discount.curve_.get());
        for (const auto& stage : record.stages_)
            AssertStagedDepositReplay(stage);
    }
}

TEST(CalibrationTest, TestStagedRiskRecordPreservesCurvesResidualsAndFraPv) {
    const auto spec = MakeStagedRiskSpec();
    const auto plain = CalibrateMultiCurve(spec);
    StagedMultiCurveCalibrationRecord_ record;
    const auto recorded = CalibrateMultiCurve(spec, &record);
    const auto today = spec.stages_[0].today_;
    FraTradeTerms_ terms;
    terms.notional_ = 1'000'000;
    terms.contractRate_ = 0.028;
    terms.index_.useProjectionCurve_ = true;
    terms.index_.forecastTenor_ = PeriodLength_("6M");
    terms.fixingIdentity_ = {"Euribor 6M", 11, 0};
    terms.forecastComponentKey_ = "forward";
    terms.discountComponentKey_ = "discount";
    const RateTradeDefinition_ fra{"FRA", RateInstrumentType_::Value_::FRA, today, Date::AddMonths(today, 3), Date::AddMonths(today, 9), Ccy_("EUR"),
                                   terms};
    auto price = [&](const MultiCurveCalibrationResult_& result) {
        RatePricingMarket_ market;
        market.resultCurrency_ = fra.currencyOrPair_;
        market.valuationTime_ = DateTime_(today, 10, 0);
        market.curveComponents_ = {{"forward", result.forwardCurves_.at(terms.index_.forecastTenor_)},
                                   {"discount", result.discountCurves_.at(terms.index_.collateral_)}};
        return PriceRateTrade(fra, market);
    };
    const auto plainPv = price(plain);
    const auto recordedPv = price(recorded);
    ASSERT_TRUE(plainPv.succeeded_) << plainPv.error_;
    ASSERT_TRUE(recordedPv.succeeded_) << recordedPv.error_;
    ASSERT_NE(plainPv.pv_, 0.0);
    ASSERT_EQ(recordedPv.pv_, plainPv.pv_);
    for (int s = 0; s < 2; ++s) {
        ASSERT_EQ(recorded.diagnostics_[s].residuals_, plain.diagnostics_[s].residuals_);
        const auto& stage = record.stages_[s];
        const auto& plainCurve = s == 0 ? plain.discountCurves_.at(stage.spec_.targetCollateral_) : plain.forwardCurves_.at(stage.spec_.targetTenor_);
        for (const auto& date : stage.spec_.knotDates_)
            ASSERT_EQ((*stage.curve_)(today, date), (*plainCurve)(today, date));
    }
}

TEST(CalibrationTest, TestStagedRiskRejectsDerivedDepositWithConstantRate) {
    auto spec = MakeStagedRiskSpec();
    auto& discount = spec.stages_[0];
    for (auto& input : discount.instruments_)
        input.reset(new ConstantRateDeposit_(discount.today_, input->TimeSpan().second, input->MarketRate(), spec.liborBasis_));
    const auto plain = CalibrateMultiCurve(spec);
    ASSERT_EQ(plain.diagnostics_.size(), 2u);
    ASSERT_EQ(plain.diagnostics_[0].maxAbsResidual_, 0.);

    StagedMultiCurveCalibrationRecord_ record;
    const auto recorded = CalibrateMultiCurve(spec, &record);
    ASSERT_EQ(recorded.diagnostics_.size(), 2u);
    ASSERT_EQ(recorded.discountCurves_.size(), 1u);
    ASSERT_EQ(recorded.forwardCurves_.size(), 1u);
    for (int s = 0; s < 2; ++s)
        ASSERT_EQ(recorded.diagnostics_[s].residuals_, plain.diagnostics_[s].residuals_);
    ASSERT_EQ(record.unavailableStage_, discount.curveName_);
    ASSERT_NE(record.unavailableReason_.find("pricing implementation"), String_::npos) << record.unavailableReason_;
}

TEST(CalibrationTest, TestStagedRiskAcceptsNativeOisSwapAndStir) {
    auto spec = MakeStagedRiskSpec();
    const auto today = spec.stages_[0].today_;
    const auto maturity = Date::AddMonths(today, 12);
    RateLegConvention_ leg;
    leg.paymentFrequency_ = PeriodLength_("6M");
    spec.stages_[0].instruments_[1].reset(new OISSwap_(today, today, maturity, 0.0212, leg, RateIndexConvention_(), leg));
    auto& forward = spec.stages_[1];
    RateIndexConvention_ index;
    index.useProjectionCurve_ = true;
    index.forecastTenor_ = forward.targetTenor_;
    forward.instruments_[1].reset(new STIR_(today, Date::AddMonths(today, 6), maturity, 0.0312, index));
    StagedMultiCurveCalibrationRecord_ record;
    const auto result = CalibrateMultiCurve(spec, &record);
    ASSERT_TRUE(record.unavailableReason_.empty()) << record.unavailableReason_;
    ASSERT_EQ(record.stages_.size(), 2u);
    ASSERT_EQ(result.diagnostics_.size(), 2u);
    for (const auto& diagnostics : result.diagnostics_)
        ASSERT_LE(diagnostics.maxAbsResidual_, 1e-12);
}

TEST(CalibrationTest, TestStagedRiskRejectsEmptyInstrumentAtOriginalInputIndex) {
    auto spec = MakeStagedRiskSpec();
    const auto plain = CalibrateMultiCurve(spec);
    auto& forward = spec.stages_[1];
    forward.instruments_[1].reset();

    StagedMultiCurveCalibrationRecord_ record;
    const auto partial = CalibrateMultiCurve(spec, &record);
    AssertOnlyDiscountFit(partial, record, plain, forward);
    ASSERT_NE(record.unavailableReason_.find("empty instrument at input index 1"), String_::npos) << record.unavailableReason_;
    try {
        CalibrateMultiCurve(spec);
        FAIL() << "Expected the empty instrument to be rejected";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("empty instrument at input index 1"), std::string::npos) << error.what();
    }
}

TEST(CalibrationTest, TestStagedRiskReturnsCompletedFitOnPricingDomainError) {
    auto spec = MakeStagedRiskSpec();
    const auto plain = CalibrateMultiCurve(spec);
    auto& forward = spec.stages_[1];
    const auto& input = dynamic_cast<const Deposit_&>(*forward.instruments_[1]);
    forward.instruments_[1].reset(
        new ThrowingDeposit_(forward.today_, input.TimeSpan().first, input.TimeSpan().second, input.MarketRate(), input.FloatConvention()));
    ASSERT_THROW(CalibrateMultiCurve(spec), std::domain_error);
    StagedMultiCurveCalibrationRecord_ record;
    const auto partial = CalibrateMultiCurve(spec, &record);
    AssertOnlyDiscountFit(partial, record, plain, forward);
    ASSERT_NE(record.unavailableReason_.find("Deposit fixing outside pricing domain"), String_::npos) << record.unavailableReason_;
}

TEST(CalibrationTest, TestStagedRiskRecordExplainsUnavailableStageAndKeepsValuation) {
    const auto valid = MakeStagedRiskSpec();
    auto check = [](const MultiCurveCalibrationSpec_& spec, const String_& stageName, const char* reason) {
        StagedMultiCurveCalibrationRecord_ record;
        const auto plain = CalibrateMultiCurve(spec);
        const auto result = CalibrateMultiCurve(spec, &record);
        ASSERT_EQ(record.unavailableStage_, stageName);
        ASSERT_NE(record.unavailableReason_.find(reason), String_::npos) << record.unavailableReason_;
        ASSERT_EQ(result.diagnostics_.size(), plain.diagnostics_.size());
        for (int s = 0; s < static_cast<int>(plain.diagnostics_.size()); ++s)
            ASSERT_EQ(result.diagnostics_[s].residuals_, plain.diagnostics_[s].residuals_);
        ASSERT_EQ(result.forwardCurves_.size(), plain.forwardCurves_.size());
        ASSERT_EQ(result.discountCurves_.size(), plain.discountCurves_.size());
    };
    {
        auto spec = valid;
        auto& stage = spec.stages_[1];
        stage.discountCurves_[stage.targetCollateral_].reset(new ConstantDiscountCurve_("other", "EUR", 1.0));
        check(spec, "Euribor 6M", "discount route");
    }
    {
        auto spec = valid;
        spec.stages_[1].baseCurve_.reset(new ConstantDiscountCurve_("other", "EUR", 1.0));
        check(spec, "Euribor 6M", "base");
    }
    {
        auto spec = valid;
        spec.stages_[0].solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        check(spec, "ESTR", "EXACT");
    }
    {
        auto spec = valid;
        spec.stages_[1].parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        check(spec, "Euribor 6M", "square");
    }
    {
        auto spec = valid;
        auto& stage = spec.stages_[0];
        stage.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        stage.knotDates_ = {stage.instruments_[0]->TimeSpan().second, stage.instruments_[1]->TimeSpan().second};
        stage.instruments_[1] = stage.instruments_[0];
        check(spec, "ESTR", "rank");
    }
}

TEST(CalibrationTest, TestStagedRiskRecordReturnsCompletedStagesAndReplacesOldRecord) {
    const auto valid = MakeStagedRiskSpec();
    StagedMultiCurveCalibrationRecord_ record;
    {
        auto spec = valid;
        spec.stages_[1].ccy_ = "USD";
        ASSERT_THROW(CalibrateMultiCurve(spec), Exception_);
        const auto partial = CalibrateMultiCurve(spec, &record);
        ASSERT_EQ(record.unavailableStage_, "Euribor 6M");
        ASSERT_NE(record.unavailableReason_.find("currency"), String_::npos);
        ASSERT_EQ(record.stages_.size(), 1u);
        ASSERT_EQ(partial.diagnostics_.size(), 1u);
        ASSERT_EQ(partial.discountCurves_.size(), 1u);
        ASSERT_EQ(partial.discountCurves_.begin()->second.get(), record.stages_.front().curve_.get());
        ASSERT_TRUE(partial.forwardCurves_.empty());
    }
    {
        auto spec = valid;
        std::swap(spec.stages_[0], spec.stages_[1]);
        const auto result = CalibrateMultiCurve(spec, &record);
        ASSERT_TRUE(result.diagnostics_.empty());
        ASSERT_TRUE(record.stages_.empty());
        ASSERT_EQ(record.unavailableStage_, "Euribor 6M");
        ASSERT_NE(record.unavailableReason_.find("discount stage followed by a forward stage"), String_::npos);
    }
    {
        auto spec = valid;
        spec.stages_.pop_back();
        ASSERT_EQ(CalibrateMultiCurve(spec, &record).diagnostics_.size(), 1u);
        ASSERT_EQ(record.unavailableStage_, spec.name_);
        ASSERT_NE(record.unavailableReason_.find("two stages"), String_::npos);
    }
    {
        auto spec = valid;
        spec.stages_[0].solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
        spec.stages_[1].instruments_.clear();
        const auto partial = CalibrateMultiCurve(spec, &record);
        ASSERT_EQ(partial.diagnostics_.size(), 1u);
        ASSERT_EQ(record.unavailableStage_, "Euribor 6M");
        ASSERT_NE(record.unavailableReason_.find("instrument"), String_::npos);
    }
    ASSERT_EQ(CalibrateMultiCurve(valid, &record).diagnostics_.size(), 2u);
    ASSERT_TRUE(record.unavailableStage_.empty());
    ASSERT_TRUE(record.unavailableReason_.empty());
}

TEST(CalibrationTest, TestBuildCurveCalibrationKnotsPolicies) {
    const Date_ today(2024, 1, 15);
    const Vector_<Handle_<YCInstrument_>> instruments = MakeCalibrationInstruments(today);
    const Vector_<Date_> inputKnots = {
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 18),
    };

    const Vector_<Date_> inputOnly = BuildCurveCalibrationKnots(today, instruments, inputKnots, CurveKnotPolicy_::Value_::INPUT);
    ASSERT_EQ(inputOnly.size(), 3);
    ASSERT_EQ(inputOnly[0], Date::AddMonths(today, 6));
    ASSERT_EQ(inputOnly[1], Date::AddMonths(today, 12));
    ASSERT_EQ(inputOnly[2], Date::AddMonths(today, 18));

    const Vector_<Date_> instrumentOnly = BuildCurveCalibrationKnots(today, instruments, {}, CurveKnotPolicy_::Value_::INSTRUMENTS);
    ASSERT_EQ(instrumentOnly.size(), 3);
    ASSERT_EQ(instrumentOnly[0], Date::AddMonths(today, 3));
    ASSERT_EQ(instrumentOnly[1], Date::AddMonths(today, 6));
    ASSERT_EQ(instrumentOnly[2], Date::AddMonths(today, 12));

    const Vector_<Date_> augmented = BuildCurveCalibrationKnots(today, instruments, inputKnots, CurveKnotPolicy_::Value_::AUGMENTED);
    ASSERT_EQ(augmented.size(), 4);
    ASSERT_EQ(augmented[0], Date::AddMonths(today, 3));
    ASSERT_EQ(augmented[1], Date::AddMonths(today, 6));
    ASSERT_EQ(augmented[2], Date::AddMonths(today, 12));
    ASSERT_EQ(augmented[3], Date::AddMonths(today, 18));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecAcceptsInstrumentDerivedKnots) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.knotDates_.clear();
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;

    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecRejectsInvalidConfigurations) {
    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.instruments_.clear();
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.knotDates_[0] = spec.today_;
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.knotDates_.back() = Date::AddMonths(spec.today_, 6);
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.initialGuess_ = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecAcceptsFutureOnlyZeroRateKnots) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.initialGuess_ = 0.02;

    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecRejectsZeroRateAnchorKnotWithSpecificMessage) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.knotDates_ = Vector::Join(Vector_<Date_>{spec.today_}, spec.knotDates_);

    try {
        ValidateCurveCalibrationSpec(spec);
        FAIL() << "Expected ZERO_RATE anchor knot to be rejected";
    } catch (const Dal::Exception_& e) {
        ASSERT_TRUE(std::string(e.what()).find("ZERO_RATE calibration requires every knot to be strictly after today") != std::string::npos)
            << e.what();
    }
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecRejectsPiecewiseConstantForwardPreAnchorKnotWithSpecificMessage) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    spec.knotDates_[0] = spec.today_.AddDays(-1);

    try {
        ValidateCurveCalibrationSpec(spec);
        FAIL() << "Expected PIECEWISE_CONSTANT_FWD pre-anchor knot to be rejected";
    } catch (const Dal::Exception_& e) {
        ASSERT_TRUE(std::string(e.what()).find("PIECEWISE_CONSTANT_FWD calibration requires knot 0 to be on or after today") != std::string::npos)
            << e.what();
    }
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecChecksZeroRateGuessAgainstParameterCount) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.initialGuessPerNode_ = {0.01, 0.02, std::numeric_limits<double>::infinity()};
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecChecksForwardGuessAgainstParameterCount) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecPreservesLogDiscountAnchorContract) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.knotDates_ = Vector::Join(Vector_<Date_>{spec.today_}, spec.knotDates_);
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestResolveInitialGuessMapsLogDiscountScalarByNodeDate) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2026, 1, 1);
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.initialGuess_ = 0.04;
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
    spec.knotDates_ = {
        spec.today_,
        spec.today_.AddDays(365),
        spec.today_.AddDays(730),
    };

    const Vector_<> scalar = ResolveCurveCalibrationInitialGuess(spec);

    ASSERT_EQ(scalar.size(), static_cast<size_t>(2));
    ASSERT_NEAR(scalar[0], -0.04, 1.0e-14);
    ASSERT_NEAR(scalar[1], -0.08, 1.0e-14);

    spec.initialGuessPerNode_ = {-0.03, -0.07};
    ASSERT_EQ(ResolveCurveCalibrationInitialGuess(spec), spec.initialGuessPerNode_);
}

TEST(CalibrationTest, TestValidatePositiveDiscountFactorsRejectsInvalidCurves) {
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> checkDates = {Date::AddMonths(today, 3), Date::AddMonths(today, 6)};

    const ConstantDiscountCurve_ zeroDf("zero", "USD", 0.0);
    ASSERT_THROW(ValidatePositiveDiscountFactors(zeroDf, today, checkDates), Dal::Exception_);

    const ConstantDiscountCurve_ nanDf("nan", "USD", std::numeric_limits<double>::quiet_NaN());
    ASSERT_THROW(ValidatePositiveDiscountFactors(nanDf, today, checkDates), Dal::Exception_);
}

TEST(CalibrationTest, TestZeroRateExactCalibrationRepricesAndReturnsPersistentType) {
    const CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec();
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, 1.0e-8);
    const Vector_<> expected = {0.011, 0.014, 0.017, 0.021};
    const Vector_<> actual = dynamic_cast<const DiscountZeroRate_&>(*result.curve_).NodeZeroRates();
    for (int i = 0; i < static_cast<int>(expected.size()); ++i)
        ASSERT_NEAR(actual[i], expected[i], 1.0e-8) << "node=" << i;
}

TEST(CalibrationTest, TestZeroRateApproximateCalibrationReturnsPositiveFiniteDiscountFactors) {
    CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR, CurveSolveMode_::Value_::APPROXIMATE);
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, spec.fitTolerance_);
    ASSERT_TRUE(result.diagnostics_.usedApproximateFit_);
}

TEST(CalibrationTest, TestZeroRateScalarAndPerNodeGuessesConvergeToSameCurve) {
    CurveCalibrationSpec_ scalarSpec = MakeZeroRateCalibrationSpec();
    scalarSpec.initialGuess_ = 0.03;
    CurveCalibrationSpec_ perNodeSpec = scalarSpec;
    perNodeSpec.initialGuessPerNode_ = {0.005, 0.010, 0.015, 0.020};

    const CurveCalibrationResult_ scalarResult = CalibrateYieldCurve(scalarSpec);
    const CurveCalibrationResult_ perNodeResult = CalibrateYieldCurve(perNodeSpec);
    AssertZeroRateCalibration(scalarSpec, scalarResult, 1.0e-8);
    AssertZeroRateCalibration(perNodeSpec, perNodeResult, 1.0e-8);
    for (const auto& knot : scalarSpec.knotDates_)
        ASSERT_NEAR((*scalarResult.curve_)(scalarSpec.today_, knot), (*perNodeResult.curve_)(perNodeSpec.today_, knot), 1.0e-9);
}

TEST(CalibrationTest, TestZeroRateCalibrationTreatsParametersAsSpreadsOverBase) {
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> baseKnots = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };
    const Handle_<DiscountCurve_> base(NewDiscountZeroRate("base", "USD", today, baseKnots, Vector_<>(baseKnots.size(), 0.0075),
                                                           DayBasis_("ACT_365F"), LogDfScheme_::Value_::LOG_LINEAR));
    const CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR, CurveSolveMode_::Value_::EXACT, base);
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, 1.0e-8);
    const Vector_<> expectedSpreads = {0.011, 0.014, 0.017, 0.021};
    const Vector_<> actualSpreads = dynamic_cast<const DiscountZeroRate_&>(*result.curve_).NodeZeroRates();
    for (int i = 0; i < static_cast<int>(expectedSpreads.size()); ++i)
        ASSERT_NEAR(actualSpreads[i], expectedSpreads[i], 1.0e-8) << "node=" << i;
}

TEST(CalibrationTest, TestZeroRateStageUsesSingleCurveDelegate) {
    const CurveCalibrationSpec_ stage = MakeZeroRateCalibrationSpec();
    MultiCurveCalibrationSpec_ multi;
    multi.name_ = "zero_rate_bundle";
    multi.ccy_ = stage.ccy_;
    multi.liborBasis_ = stage.liborBasis_;
    multi.stages_ = {stage};

    const MultiCurveCalibrationResult_ result = CalibrateMultiCurve(multi);
    ASSERT_EQ(result.discountCurves_.size(), 1u);
    const auto found = result.discountCurves_.find(stage.targetCollateral_);
    ASSERT_NE(found, result.discountCurves_.end());
    ASSERT_NE(dynamic_cast<const DiscountZeroRate_*>(found->second.get()), nullptr);
    ASSERT_EQ(result.diagnostics_.size(), 1u);
    ASSERT_LT(result.diagnostics_[0].maxAbsResidual_, 1.0e-8);
}
