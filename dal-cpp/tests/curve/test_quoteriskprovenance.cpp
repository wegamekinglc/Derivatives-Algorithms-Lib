//
// Created by dal-implementer on 2026/8/31.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <type_traits>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using SingleCurveFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CurveCalibrationSpec_&,
                                                                       const Dal::CurveCalibrationResult_&,
                                                                       const Dal::CurveCalibrationOptions_&,
                                                                       const Dal::RatePricingMarket_&,
                                                                       const Dal::RateQuoteRiskProvenanceConfig_&);
using JointXccyFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::JointXccyCalibrationSpec_&,
                                                                     const Dal::JointXccyCalibrationResult_&,
                                                                     const Dal::JointXccyCalibrationOptions_&,
                                                                     const Dal::RatePricingMarket_&,
                                                                     const Dal::RateQuoteRiskProvenanceConfig_&);
using StagedXccyFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CrossCurrencyCalibrationSpec_&,
                                                                      const Dal::CrossCurrencyCalibrationResult_&,
                                                                      const Dal::CrossCurrencyCalibrationOptions_&,
                                                                      const Dal::RatePricingMarket_&,
                                                                      const Dal::RateQuoteRiskProvenanceConfig_&);

static_assert(std::is_same_v<decltype(&Dal::BuildSingleCurveQuoteRiskProvenance), SingleCurveFactorySignature_>);
static_assert(std::is_same_v<decltype(&Dal::BuildJointXccyQuoteRiskProvenance), JointXccyFactorySignature_>);
static_assert(std::is_same_v<decltype(&Dal::BuildStagedXccyBasisQuoteRiskProvenance), StagedXccyFactorySignature_>);

namespace {
    class ScopedQuoteRiskSweepFailure_ {
        Dal::String_ componentKey_;

    public:
        explicit ScopedQuoteRiskSweepFailure_(const Dal::String_& componentKey) : componentKey_(componentKey) {
            Dal::RateCashflowPricingInternal::g_quoteRiskForcedSweepFailureComponent.store(&componentKey_, std::memory_order_relaxed);
        }

        ~ScopedQuoteRiskSweepFailure_() {
            Dal::RateCashflowPricingInternal::g_quoteRiskForcedSweepFailureComponent.store(nullptr, std::memory_order_relaxed);
        }
    };

    class CyclicDiscountCurve_ : public Dal::DiscountCurve_ {
        Dal::Handle_<Dal::DiscountCurve_> base_;

    public:
        explicit CyclicDiscountCurve_(const Dal::String_& name) : DiscountCurve_(name, "USD") {}

        void SetBase(const Dal::Handle_<Dal::DiscountCurve_>& base) { base_ = base; }
        void ClearBase() { base_ = Dal::Handle_<Dal::DiscountCurve_>(); }
        double operator()(const Dal::Date_&, const Dal::Date_&) const override { return 1.0; }
        void Poll(Dal::Vector_<const Dal::YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const Dal::YCComponent_*, Dal::Handle_<Dal::YCComponent_>>*) const override {}
        [[nodiscard]] std::unique_ptr<Dal::YCComponent_> Clone(const Dal::String_& newName,
                                                               const Dal::YCComponent_::substitutions_t&) const override {
            return std::make_unique<CyclicDiscountCurve_>(newName);
        }
        void Write(Dal::Archive::Store_& dst) const override {
            dst.SetType("CyclicDiscountCurve_TestOnly");
            Dal::Archive::Utils::Set(dst, "base", base_);
            dst.Done();
        }
    };

    Dal::RateIndexConvention_ SingleIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("6M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Dal::Holidays::None();
        result.accrualHolidays_ = Dal::Holidays::None();
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    struct SingleProvenanceInput_ {
        Dal::CurveCalibrationSpec_ spec_;
        Dal::CurveCalibrationOptions_ options_;
        Dal::CurveCalibrationResult_ result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    SingleProvenanceInput_ MakeSingleInput(Dal::CurveJacobianMode_ mode) {
        SingleProvenanceInput_ result;
        result.spec_.today_ = Dal::Date_(2025, 1, 2);
        result.spec_.ccy_ = "USD";
        result.spec_.curveName_ = "single_quote_risk";
        result.spec_.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        result.spec_.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        result.spec_.knotPolicy_ = Dal::CurveKnotPolicy_::Value_::INPUT;
        result.spec_.solveMode_ = Dal::CurveSolveMode_::Value_::EXACT;
        result.spec_.liborBasis_ = Dal::DayBasis_("ACT_365F");
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.initialGuess_ = 0.01;
        result.spec_.knotDates_ = {Dal::Date::AddMonths(result.spec_.today_, 6), Dal::Date::AddMonths(result.spec_.today_, 12)};

        const Dal::Handle_<Dal::DiscountCurve_> known(
            Dal::NewDiscountPWC("single_quote_risk_known", "USD", Dal::PiecewiseConstant_(result.spec_.knotDates_, Dal::Vector_<>{0.02, 0.025})));
        const Dal::CurveBlock_ knownBlock(known, result.spec_.liborBasis_);
        const Dal::RateIndexConvention_ index = SingleIndex();
        for (const auto& maturity : result.spec_.knotDates_) {
            const Dal::Handle_<Dal::YCInstrument_> prototype(new Dal::Deposit_(result.spec_.today_, result.spec_.today_, maturity, 0.0, index));
            const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(knownBlock);
            result.spec_.instruments_.push_back(
                Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(result.spec_.today_, result.spec_.today_, maturity, quote, index)));
        }

        result.options_.jacobianMode_ = mode;
        result.result_ = Dal::CalibrateYieldCurve(result.spec_, result.options_);
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), result.result_.curve_.get());
        result.market_.valuationTime_ = Dal::DateTime_(result.spec_.today_, 9, 0);
        result.market_.resultCurrency_ = Dal::Ccy_("USD");
        result.market_.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
        result.market_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        result.config_.calibrationId_ = "single-calibration";
        result.config_.componentKeyByParameterBlock_[result.spec_.curveName_] = "discount";
        return result;
    }

    bool IsSha256Fingerprint(const Dal::String_& value) {
        if (value.size() != 71 || value.substr(0, 7) != "sha256:")
            return false;
        for (std::size_t i = 7; i < value.size(); ++i) {
            const char ch = value[i];
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                return false;
        }
        return true;
    }

    void ExpectSingleFailure(SingleProvenanceInput_* input, const std::string& token) {
        try {
            static_cast<void>(
                Dal::BuildSingleCurveQuoteRiskProvenance(input->spec_, input->result_, input->options_, input->market_, input->config_));
            FAIL() << "Expected quote-risk provenance construction to fail with " << token;
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find(token), std::string::npos) << exception.what();
        }
    }

    Dal::RateQuoteRiskProvenance_ BuildSingle(const SingleProvenanceInput_& input) {
        return Dal::BuildSingleCurveQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    }

    Dal::RateTradeDefinition_ SingleDepositTrade(const SingleProvenanceInput_& input, const Dal::String_& instrumentId = "single-deposit") {
        Dal::DepositTradeTerms_ terms;
        terms.notional_ = 1'000'000.0;
        terms.contractRate_ = 0.02;
        terms.lend_ = true;
        terms.index_ = SingleIndex();
        terms.discountComponentKey_ = "discount";
        return {instrumentId,
                Dal::RateInstrumentType_::Value_::DEPOSIT,
                input.spec_.today_,
                input.spec_.today_,
                input.spec_.knotDates_.back(),
                Dal::Ccy_("USD"),
                terms};
    }

    Dal::Handle_<Dal::DiscountCurve_> QuoteOraclePwcCurve(const Dal::CurveCalibrationSpec_& spec) {
        const int nKnots = static_cast<int>(spec.knotDates_.size());
        Dal::Vector_<> forwards(nKnots);
        for (int i = 0; i < nKnots; ++i)
            forwards[i] = 0.012 + 0.0004 * i;
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountPWC("quote_oracle_known", spec.ccy_, Dal::PiecewiseConstant_(spec.knotDates_, forwards)));
    }

    Dal::Handle_<Dal::DiscountCurve_> QuoteOraclePwlfCurve(const Dal::CurveCalibrationSpec_& spec) {
        const int nKnots = static_cast<int>(spec.knotDates_.size());
        Dal::Vector_<> left(nKnots), right(nKnots);
        for (int i = 0; i < nKnots; ++i) {
            left[i] = 0.011 + 0.0005 * i;
            right[i] = left[i] + 0.0003;
        }
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountPWLF("quote_oracle_known", spec.ccy_, Dal::PiecewiseLinear_(spec.knotDates_, left, right)));
    }

    Dal::Handle_<Dal::DiscountCurve_> QuoteOracleLogDiscountCurve(const Dal::CurveCalibrationSpec_& spec) {
        const int nKnots = static_cast<int>(spec.knotDates_.size());
        Dal::Vector_<> logDfs(nKnots);
        for (int i = 0; i < nKnots; ++i)
            logDfs[i] = -0.018 * spec.liborBasis_(spec.today_, spec.knotDates_[i], nullptr);
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountLogDF("quote_oracle_known", spec.ccy_, spec.knotDates_, logDfs, spec.liborBasis_, spec.logDfScheme_));
    }

    Dal::Handle_<Dal::DiscountCurve_> QuoteOracleZeroRateCurve(const Dal::CurveCalibrationSpec_& spec) {
        const int nKnots = static_cast<int>(spec.knotDates_.size());
        Dal::Vector_<> zeroRates(nKnots);
        for (int i = 0; i < nKnots; ++i)
            zeroRates[i] = 0.016 + 0.0002 * i;
        return Dal::Handle_<Dal::DiscountCurve_>(
            Dal::NewDiscountZeroRate("quote_oracle_known", spec.ccy_, spec.today_, spec.knotDates_, zeroRates, spec.liborBasis_, spec.logDfScheme_));
    }

    Dal::Handle_<Dal::DiscountCurve_> QuoteOracleCurve(const Dal::CurveCalibrationSpec_& spec) {
        switch (spec.parameterization_.Switch()) {
        case Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
            return QuoteOraclePwcCurve(spec);
        case Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
            return QuoteOraclePwlfCurve(spec);
        case Dal::CurveParameterization_::Value_::LOG_DISCOUNT:
            return QuoteOracleLogDiscountCurve(spec);
        case Dal::CurveParameterization_::Value_::ZERO_RATE:
            return QuoteOracleZeroRateCurve(spec);
        }
        THROW("Unsupported quote oracle parameterization");
    }

    Dal::Vector_<Dal::Date_> QuoteOracleMaturities(const Dal::Date_& today, int quoteCount) {
        Dal::Vector_<Dal::Date_> maturities;
        for (int i = 1; i <= quoteCount; ++i)
            maturities.push_back(Dal::Date::AddMonths(today, 6 * i));
        return maturities;
    }

    void ConfigureQuoteOracleKnots(Dal::CurveParameterization_ parameterization,
                                   const Dal::Vector_<Dal::Date_>& maturities,
                                   Dal::CurveCalibrationSpec_* spec) {
        if (parameterization == Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD) {
            spec->knotDates_.push_back(spec->today_);
            for (const auto& maturity : maturities)
                spec->knotDates_.push_back(maturity);
            return;
        }
        if (parameterization == Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD) {
            const int knotCount = (static_cast<int>(maturities.size()) + 1) / 2 + 1;
            spec->knotDates_.push_back(Dal::Date::AddMonths(spec->today_, 3));
            for (int i = 1; i < knotCount; ++i)
                spec->knotDates_.push_back(Dal::Date::AddMonths(spec->today_, 12 * i));
            return;
        }
        spec->knotDates_ = maturities;
        if (parameterization == Dal::CurveParameterization_::Value_::LOG_DISCOUNT) {
            Dal::Vector_<Dal::Date_> anchored = {spec->today_};
            for (const auto& maturity : maturities)
                anchored.push_back(maturity);
            spec->knotDates_ = std::move(anchored);
        }
    }

    void ConfigureQuoteOraclePwcInitialGuesses(Dal::CurveCalibrationSpec_* spec) {
        for (int i = 0; i < static_cast<int>(spec->knotDates_.size()); ++i)
            spec->initialGuessPerNode_.push_back(0.012 + 0.0004 * i);
    }

    void ConfigureQuoteOraclePwlfInitialGuesses(Dal::CurveCalibrationSpec_* spec) {
        for (int i = 0; i < static_cast<int>(spec->knotDates_.size()); ++i) {
            spec->initialGuessPerNode_.push_back(0.011 + 0.0005 * i);
            spec->initialGuessPerNode_.push_back(0.0113 + 0.0005 * i);
        }
    }

    void ConfigureQuoteOracleLogDiscountInitialGuesses(Dal::CurveCalibrationSpec_* spec) {
        for (int i = 1; i < static_cast<int>(spec->knotDates_.size()); ++i)
            spec->initialGuessPerNode_.push_back(-0.018 * spec->liborBasis_(spec->today_, spec->knotDates_[i], nullptr));
    }

    void ConfigureQuoteOracleZeroRateInitialGuesses(Dal::CurveCalibrationSpec_* spec) {
        for (int i = 0; i < static_cast<int>(spec->knotDates_.size()); ++i)
            spec->initialGuessPerNode_.push_back(0.016 + 0.0002 * i);
    }

    void ConfigureQuoteOracleInitialGuesses(Dal::CurveParameterization_ parameterization, Dal::CurveCalibrationSpec_* spec) {
        switch (parameterization.Switch()) {
        case Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
            return ConfigureQuoteOraclePwcInitialGuesses(spec);
        case Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
            return ConfigureQuoteOraclePwlfInitialGuesses(spec);
        case Dal::CurveParameterization_::Value_::LOG_DISCOUNT:
            return ConfigureQuoteOracleLogDiscountInitialGuesses(spec);
        case Dal::CurveParameterization_::Value_::ZERO_RATE:
            return ConfigureQuoteOracleZeroRateInitialGuesses(spec);
        }
    }

    void ConfigureQuoteOracleInstruments(const Dal::Vector_<Dal::Date_>& maturities,
                                         const Dal::CurveBlock_& knownBlock,
                                         Dal::CurveCalibrationSpec_* spec) {
        const Dal::RateIndexConvention_ index = SingleIndex();
        Dal::Handle_<Dal::YieldCurve_> empty;
        for (const auto& maturity : maturities) {
            const Dal::Handle_<Dal::YCInstrument_> prototype(new Dal::Deposit_(spec->today_, spec->today_, maturity, 0.0, index));
            const double quote = (*prototype->Precompute(empty))(knownBlock);
            spec->instruments_.push_back(Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(spec->today_, spec->today_, maturity, quote, index)));
        }
    }

    void CalibrateAndBindQuoteOracle(Dal::CurveJacobianMode_ mode, SingleProvenanceInput_* input) {
        input->options_.jacobianMode_ = mode;
        try {
            input->result_ = Dal::CalibrateYieldCurve(input->spec_, input->options_);
        } catch (const std::exception& exception) {
            THROW("Quote oracle baseline calibration failed: " + Dal::String_(exception.what()));
        }
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), input->result_.curve_.get());
        input->market_.valuationTime_ = Dal::DateTime_(input->spec_.today_, 9, 0);
        input->market_.resultCurrency_ = Dal::Ccy_("USD");
        input->market_.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
        input->market_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        input->config_.calibrationId_ = "single-quote-oracle";
        input->config_.componentKeyByParameterBlock_[input->spec_.curveName_] = "discount";
    }

    SingleProvenanceInput_ MakeQuoteOracleInput(Dal::CurveParameterization_ parameterization, int quoteCount, Dal::CurveJacobianMode_ mode) {
        SingleProvenanceInput_ input;
        input.spec_.today_ = Dal::Date_(2025, 1, 2);
        input.spec_.ccy_ = "USD";
        input.spec_.curveName_ = "single_quote_oracle";
        input.spec_.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        input.spec_.parameterization_ = parameterization;
        input.spec_.knotPolicy_ = Dal::CurveKnotPolicy_::Value_::INPUT;
        input.spec_.solveMode_ = Dal::CurveSolveMode_::Value_::EXACT;
        input.spec_.liborBasis_ = Dal::DayBasis_("ACT_365F");
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        input.spec_.smoothingWeight_ = 1.0;
        input.spec_.initialGuess_ = 0.018;
        input.spec_.logDfScheme_ = Dal::LogDfScheme_::Value_::LOG_LINEAR;

        const auto maturities = QuoteOracleMaturities(input.spec_.today_, quoteCount);
        ConfigureQuoteOracleKnots(parameterization, maturities, &input.spec_);
        const auto known = QuoteOracleCurve(input.spec_);
        const Dal::CurveBlock_ knownBlock(known, input.spec_.liborBasis_);
        ConfigureQuoteOracleInitialGuesses(parameterization, &input.spec_);
        ConfigureQuoteOracleInstruments(maturities, knownBlock, &input.spec_);
        CalibrateAndBindQuoteOracle(mode, &input);
        return input;
    }

    struct QuoteOracleObservation_ {
        double netPv_ = 0.0;
        double grossAbsPv_ = 0.0;
    };

    QuoteOracleObservation_ PriceQuoteOracleTrades(const Dal::Vector_<Dal::RateTradeDefinition_>& trades, const Dal::RatePricingMarket_& market) {
        QuoteOracleObservation_ observation;
        for (const auto& priced : Dal::PriceRateTrades(trades, market)) {
            REQUIRE(priced.succeeded_ && std::isfinite(priced.pv_), "Quote oracle trade failed to price");
            observation.netPv_ += priced.pv_;
            observation.grossAbsPv_ += std::abs(priced.pv_);
        }
        return observation;
    }

    QuoteOracleObservation_ RecalibrateAndPriceQuoteBump(const SingleProvenanceInput_& input,
                                                         const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                                         int quoteIndex,
                                                         double bump) {
        Dal::CurveCalibrationSpec_ bumped = input.spec_;
        const auto* original = dynamic_cast<const Dal::Deposit_*>(input.spec_.instruments_[quoteIndex].get());
        REQUIRE(original, "Quote oracle expected deposit calibration instruments");
        const auto span = original->TimeSpan();
        bumped.instruments_[quoteIndex] = Dal::Handle_<Dal::YCInstrument_>(
            new Dal::Deposit_(original->TradeDate(), span.first, span.second, original->MarketRate() + bump, original->FloatConvention()));
        Dal::CurveCalibrationResult_ calibrated;
        try {
            calibrated = Dal::CalibrateYieldCurve(bumped, input.options_);
        } catch (const std::exception& exception) {
            THROW("Quote oracle bumped calibration failed for quote " + Dal::String::FromInt(quoteIndex) + " and bump " +
                  Dal::String::FromDouble(bump) + ": " + Dal::String_(exception.what()));
        }
        REQUIRE(calibrated.diagnostics_.maxAbsResidual_ <= input.spec_.fitTolerance_, "Quote oracle calibration did not satisfy the fit gate");
        Dal::RatePricingMarket_ market = input.market_;
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), calibrated.curve_.get());
        market.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
        return PriceQuoteOracleTrades(trades, market);
    }

    void AssertQuoteOracleZeroRateMetadata(const SingleProvenanceInput_& input, Dal::CurveParameterization_ parameterization) {
        if (parameterization != Dal::CurveParameterization_::Value_::ZERO_RATE)
            return;
        const auto* zero = dynamic_cast<const Dal::DiscountZeroRate_*>(input.result_.curve_.get());
        ASSERT_NE(zero, nullptr);
        ASSERT_EQ(zero->Name(), input.spec_.curveName_);
        ASSERT_EQ(zero->ccy_.String(), input.spec_.ccy_);
        ASSERT_EQ(zero->AnchorDate(), input.spec_.today_);
        ASSERT_EQ(zero->NodeDates(), input.spec_.knotDates_);
        ASSERT_EQ(zero->Scheme(), input.spec_.logDfScheme_);
        ASSERT_EQ(zero->DayCount(), input.spec_.liborBasis_);
    }

    double QuoteOracleTolerance(int quoteCount) {
        if (quoteCount <= 5)
            return 5.0e-6;
        if (quoteCount <= 10)
            return 1.0e-4;
        return 1.0e-3;
    }

    double RelativeError(double absoluteError, double scale) {
        if (scale != 0.0)
            return absoluteError / scale;
        return absoluteError == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();
    }

    struct QuoteOracleRisk_ {
        double priceScale_ = 0.0;
        double derivative_ = 0.0;
        double dv01_ = 0.0;
    };

    template <class Reprice_> QuoteOracleRisk_ FullRecalibrationQuoteRisk(const QuoteOracleObservation_& baseline, Reprice_&& reprice) {
        constexpr double h = 1.0e-6;
        constexpr double b = 1.0e-4;
        const auto plusH = reprice(h);
        const auto minusH = reprice(-h);
        const auto plusB = reprice(b);
        const auto minusB = reprice(-b);
        const double priceScale = std::max({1.0, baseline.grossAbsPv_, plusH.grossAbsPv_, minusH.grossAbsPv_, plusB.grossAbsPv_, minusB.grossAbsPv_});
        return {priceScale, (plusH.netPv_ - minusH.netPv_) / (2.0 * h), (plusB.netPv_ - minusB.netPv_) / 2.0};
    }

    QuoteOracleRisk_ QuoteRiskOracle(const SingleProvenanceInput_& input,
                                     const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                     const QuoteOracleObservation_& baseline,
                                     int quoteIndex) {
        return FullRecalibrationQuoteRisk(baseline, [&](double bump) { return RecalibrateAndPriceQuoteBump(input, trades, quoteIndex, bump); });
    }

    void AssertQuoteRiskBucket(const Dal::RateQuoteRiskBucket_& bucket,
                               const QuoteOracleRisk_& oracle,
                               double tolerance,
                               const Dal::String_& domain,
                               Dal::CurveJacobianMode_ mode,
                               int quoteCount,
                               int quoteIndex) {
        constexpr double b = 1.0e-4;
        constexpr double epsilon = std::numeric_limits<double>::epsilon();
        ASSERT_TRUE(std::isfinite(bucket.dPvDDecimalQuote_));
        ASSERT_TRUE(std::isfinite(bucket.dv01_));
        ASSERT_TRUE(std::isfinite(oracle.derivative_));
        ASSERT_TRUE(std::isfinite(oracle.dv01_));
        const double unitError = std::abs(bucket.dv01_ - b * bucket.dPvDDecimalQuote_);
        const double unitScale = std::max({oracle.priceScale_ * b, std::abs(bucket.dv01_), b * std::abs(bucket.dPvDDecimalQuote_)});
        ASSERT_LE(unitError, 64.0 * epsilon * unitScale);
        const double derivativeError = std::abs(bucket.dPvDDecimalQuote_ - oracle.derivative_);
        const double derivativeRelative = RelativeError(derivativeError, std::max(std::abs(bucket.dPvDDecimalQuote_), std::abs(oracle.derivative_)));
        ASSERT_TRUE(derivativeError <= tolerance * oracle.priceScale_ || derivativeRelative <= tolerance)
            << "domain=" << domain << " mode=" << mode.String() << " N=" << quoteCount << " quote=" << quoteIndex
            << " api=" << bucket.dPvDDecimalQuote_ << " oracle=" << oracle.derivative_ << " absError=" << derivativeError
            << " relError=" << derivativeRelative;
        const double dv01Error = std::abs(bucket.dv01_ - oracle.dv01_);
        const double dv01Relative = RelativeError(dv01Error, std::max(std::abs(bucket.dv01_), std::abs(oracle.dv01_)));
        ASSERT_TRUE(dv01Error <= tolerance * b * oracle.priceScale_ || dv01Relative <= tolerance)
            << "domain=" << domain << " mode=" << mode.String() << " N=" << quoteCount << " quote=" << quoteIndex << " api=" << bucket.dv01_
            << " oracle=" << oracle.dv01_ << " absError=" << dv01Error << " relError=" << dv01Relative;
    }

    void AssertQuoteRiskOracle(Dal::CurveParameterization_ parameterization, int quoteCount, Dal::CurveJacobianMode_ mode) {
        const auto input = MakeQuoteOracleInput(parameterization, quoteCount, mode);
        AssertQuoteOracleZeroRateMetadata(input, parameterization);
        const auto provenance = BuildSingle(input);
        const Dal::Vector_<Dal::RateTradeDefinition_> trades = {SingleDepositTrade(input, "oracle-deposit")};
        const auto aggregate = Dal::AggregateRatePortfolioQuoteRisk(trades, input.market_, {provenance});
        ASSERT_EQ(static_cast<int>(aggregate.buckets_.size()), quoteCount);
        const auto baseline = PriceQuoteOracleTrades(trades, input.market_);
        const double tolerance = QuoteOracleTolerance(quoteCount);
        for (int i = 0; i < quoteCount; ++i)
            AssertQuoteRiskBucket(aggregate.buckets_[i], QuoteRiskOracle(input, trades, baseline, i), tolerance, parameterization.String(), mode,
                                  quoteCount, i);
    }

    void RecalibrateAndBindSingle(SingleProvenanceInput_* input) {
        input->result_ = Dal::CalibrateYieldCurve(input->spec_, input->options_);
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), input->result_.curve_.get());
        input->market_.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
    }

    Dal::RateIndexConvention_ JointIndex(bool projection) {
        auto result = SingleIndex();
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.useProjectionCurve_ = projection;
        result.fixingLag_ = 0;
        return result;
    }

    Dal::Handle_<Dal::DiscountCurve_>
    JointPwc(const Dal::String_& name, const Dal::Ccy_& ccy, const Dal::Vector_<Dal::Date_>& knots, const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC(name, ccy.String(), Dal::PiecewiseConstant_(knots, parameters)));
    }

    Dal::Handle_<Dal::CurveBlock_> JointBlock(const Dal::String_& name,
                                              const Dal::Ccy_& ccy,
                                              const Dal::Vector_<Dal::Date_>& knots,
                                              const Dal::Vector_<>& discountParameters,
                                              const Dal::Vector_<>& forwardParameters) {
        return Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_(
            name, ccy.String(), {{Dal::CollateralType_(Dal::CollateralType_::Value_::OIS), JointPwc(name + "_ois", ccy, knots, discountParameters)}},
            {{Dal::PeriodLength_("3M"), JointPwc(name + "_3m", ccy, knots, forwardParameters)}}, Dal::DayBasis_("ACT_365F")));
    }

    Dal::Handle_<Dal::YCInstrument_>
    JointDeposit(const Dal::Date_& today, const Dal::Date_& maturity, const Dal::RateIndexConvention_& index, const Dal::CurveBlock_& block) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(new Dal::Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(block);
        return Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(today, today, maturity, quote, index));
    }

    Dal::JointCurrencyCurveSpec_ JointCurrencySpec(const Dal::Date_& today,
                                                   const Dal::Ccy_& ccy,
                                                   const Dal::Vector_<Dal::Date_>& knots,
                                                   const Dal::Vector_<Dal::Date_>& maturities,
                                                   const Dal::Handle_<Dal::CurveBlock_>& market) {
        Dal::JointCurveDeclaration_ discount;
        discount.curveName_ = Dal::String_(ccy.String()) + "_ois_provenance";
        discount.knotDates_ = knots;
        discount.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        discount.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);

        Dal::JointCurveDeclaration_ forward;
        forward.curveName_ = Dal::String_(ccy.String()) + "_3m_provenance";
        forward.knotDates_ = knots;
        forward.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        forward.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        forward.targetTenor_ = Dal::PeriodLength_("3M");
        forward.calibrateDiscountCurve_ = false;
        for (const auto& maturity : maturities) {
            discount.instruments_.push_back(JointDeposit(today, maturity, JointIndex(false), *market));
            forward.instruments_.push_back(JointDeposit(today, maturity, JointIndex(true), *market));
        }

        Dal::JointCurrencyCurveSpec_ result;
        result.ccy_ = ccy;
        result.curves_ = {discount, forward};
        return result;
    }

    Dal::CrossCurrencySwapConfig_ JointXccyConfig(const Dal::CurrencyPair_& pair) {
        Dal::RateLegConvention_ leg;
        leg.paymentFrequency_ = Dal::PeriodLength_("3M");
        leg.dayBasis_ = Dal::DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Dal::Holidays::None();
        leg.paymentHolidays_ = Dal::Holidays::None();
        Dal::CrossCurrencySwapConfig_ result;
        result.pair_ = pair;
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = JointIndex(true);
        result.convention_.foreignIndex_ = JointIndex(true);
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = Dal::XccyNotionalMode_::Value_::FIXED;
        return result;
    }

    struct JointProvenanceInput_ {
        Dal::JointXccyCalibrationSpec_ spec_;
        Dal::JointXccyCalibrationOptions_ options_;
        Dal::JointXccyCalibrationResult_ result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    Dal::Handle_<Dal::DiscountCurve_> AliasCurve(const Dal::DiscountCurve_& curve) {
        return Dal::Handle_<Dal::DiscountCurve_>(std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), &curve));
    }

    Dal::Handle_<Dal::DiscountCurve_> IndependentCurveClone(const Dal::DiscountCurve_& curve) {
        std::unique_ptr<Dal::YCComponent_> clone = curve.Clone(curve.Name(), {});
        auto* discount = dynamic_cast<Dal::DiscountCurve_*>(clone.get());
        REQUIRE(discount, "Expected a cloned discount curve");
        clone.release();
        return Dal::Handle_<Dal::DiscountCurve_>(discount);
    }

    template <class Factory_>
    void AssertClonedXccyBindingFailsClosed(const Factory_& factory,
                                            const Dal::RatePricingMarket_& market,
                                            const Dal::RateTradeDefinition_& trade,
                                            const char* domain) {
        try {
            const auto provenance = factory();
            const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, market, {provenance});
            ASSERT_EQ(result.meta_.size(), 1U);
            ASSERT_FALSE(result.meta_.front().eligible_ && result.meta_.front().structuralZero_)
                << domain << " accepted equal-content clone bindings as an eligible structural zero";
            FAIL() << domain << " accepted curve bindings that do not route through the bound XCCY market";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH"), std::string::npos) << exception.what();
        }
    }

    Dal::RatePricingMarket_ JointPricingMarket(const Dal::JointXccyCalibrationSpec_& spec,
                                               const Dal::JointXccyCalibrationResult_& result,
                                               const Dal::RateQuoteRiskProvenanceConfig_& config) {
        const Dal::Vector_<Dal::Handle_<Dal::DiscountCurve_>> curves = {
            AliasCurve(result.domesticCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(result.domesticCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(result.foreignCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(result.foreignCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            result.basisCurve_,
        };
        REQUIRE(curves.size() == result.parameterRanges_.size(), "Unexpected joint XCCY parameter range count");
        Dal::RatePricingMarket_ market;
        for (int i = 0; i < static_cast<int>(result.parameterRanges_.size()); ++i)
            market.curveComponents_[config.componentKeyByParameterBlock_.at(result.parameterRanges_[i].name_)] = curves[i];
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.pair_.domestic_;
        market.fixings_ = result.fixings_;
        auto xccy = std::make_shared<Dal::CrossCurrencyMarket_>(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_,
                                                                spec.valuationTime_, spec.collateralCurrency_, result.fixings_);
        xccy->SetBasisCurve(result.basisCurve_);
        market.xccyMarket_ = xccy;
        return market;
    }

    JointProvenanceInput_
    MakeJointInputWithWidths(Dal::CurveJacobianMode_ mode, int curveQuoteCount, int basisQuoteCount, bool computeInverse = true) {
        REQUIRE(curveQuoteCount > 0 && basisQuoteCount > 0, "Joint quote-risk fixture widths must be positive");
        JointProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        Dal::Vector_<Dal::Date_> maturities, knots, basisMaturities, basisKnots;
        Dal::Vector_<> domesticDiscount, domesticForward, foreignDiscount, foreignForward, basisParameters;
        for (int i = 0; i < curveQuoteCount; ++i) {
            maturities.push_back(Dal::Date::AddMonths(today, 12 * (i + 1)));
            knots.push_back(Dal::Date::AddMonths(today, 6 + 12 * i));
            domesticDiscount.push_back(0.015 + 0.0005 * i);
            domesticForward.push_back(0.024 + 0.0005 * i);
            foreignDiscount.push_back(0.010 + 0.0004 * i);
            foreignForward.push_back(0.019 + 0.0004 * i);
        }
        for (int i = 0; i < basisQuoteCount; ++i) {
            basisMaturities.push_back(Dal::Date::AddMonths(today, 12 * (i + 1) + 6));
            basisKnots.push_back(Dal::Date::AddMonths(today, 12 * (i + 1)));
            basisParameters.push_back(0.0010 + 0.0002 * i);
        }
        const auto domestic = JointBlock("usd_true_provenance", Dal::Ccy_("USD"), knots, domesticDiscount, domesticForward);
        const auto foreign = JointBlock("eur_true_provenance", Dal::Ccy_("EUR"), knots, foreignDiscount, foreignForward);
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("basis_true_provenance", pair.domestic_, basisKnots, basisParameters));

        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.pair_ = pair;
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.domestic_ = JointCurrencySpec(today, pair.domestic_, knots, maturities, domestic);
        input.spec_.foreign_ = JointCurrencySpec(today, pair.foreign_, knots, maturities, foreign);
        input.spec_.basis_.curveName_ = "usd_eur_basis_provenance";
        input.spec_.basis_.knotDates_ = basisKnots;
        input.spec_.fixings_ = fixings;
        input.spec_.initialGuess_ = 0.005;
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        const auto config = JointXccyConfig(pair);
        for (const auto& maturity : basisMaturities) {
            const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
            input.spec_.basis_.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        input.options_.jacobianMode_ = mode;
        input.options_.computeEffJacobianInverse_ = computeInverse;
        input.result_ = Dal::CalibrateJointXccyMarket(input.spec_, input.options_);

        input.config_.calibrationId_ = "joint-xccy-calibration";
        for (int i = 0; i < static_cast<int>(input.result_.parameterRanges_.size()); ++i) {
            const Dal::String_ component = "joint-component-" + Dal::String::FromInt(i);
            input.config_.componentKeyByParameterBlock_[input.result_.parameterRanges_[i].name_] = component;
        }
        input.market_ = JointPricingMarket(input.spec_, input.result_, input.config_);
        return input;
    }

    JointProvenanceInput_ MakeJointInput(Dal::CurveJacobianMode_ mode, bool computeInverse = true) {
        return MakeJointInputWithWidths(mode, 2, 2, computeInverse);
    }

    struct StagedProvenanceInput_ {
        Dal::CrossCurrencyCalibrationSpec_ spec_;
        Dal::CrossCurrencyCalibrationOptions_ options_;
        std::unique_ptr<Dal::CrossCurrencyCalibrationResult_> result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    StagedProvenanceInput_
    MakeStagedInputWithQuoteCount(Dal::CurveJacobianMode_ mode, int quoteCount, double dependencyShift = 0.0, bool computeInverse = true) {
        REQUIRE(quoteCount > 0, "Staged quote-risk fixture width must be positive");
        StagedProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        Dal::Vector_<Dal::Date_> knots;
        Dal::Vector_<> domesticDiscount, domesticForward, foreignDiscount, foreignForward, basisParameters;
        for (int i = 0; i < quoteCount; ++i) {
            knots.push_back(Dal::Date::AddMonths(today, 12 * (i + 1)));
            domesticDiscount.push_back(0.015 + 0.0003 * i + dependencyShift);
            domesticForward.push_back(0.024 + 0.0003 * i + dependencyShift);
            foreignDiscount.push_back(0.010 + 0.0002 * i - dependencyShift);
            foreignForward.push_back(0.019 + 0.0002 * i - dependencyShift);
            basisParameters.push_back(0.0010 + 0.0001 * i);
        }
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        const auto domestic = JointBlock("usd_staged_provenance", pair.domestic_, knots, domesticDiscount, domesticForward);
        const auto foreign = JointBlock("eur_staged_provenance", pair.foreign_, knots, foreignDiscount, foreignForward);
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const auto config = JointXccyConfig(pair);
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("known_staged_provenance", pair.domestic_, knots, basisParameters));

        input.spec_.today_ = today;
        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fixings_ = fixings;
        input.spec_.basisPair_ = pair;
        input.spec_.domesticCurveBlock_ = domestic;
        input.spec_.foreignCurveBlock_ = foreign;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.knotDates_ = knots;
        input.spec_.initialGuess_ = 0.001;
        input.spec_.tolerance_ = 1.0e-10;
        for (const auto& knot : knots) {
            const Dal::Date_ maturity = Dal::Date::AddMonths(knot, 6);
            const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
            input.spec_.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        input.options_.jacobianMode_ = mode;
        input.options_.computeEffJacobianInverse_ = computeInverse;
        input.result_ = std::make_unique<Dal::CrossCurrencyCalibrationResult_>(Dal::CalibrateCrossCurrencyMarket(input.spec_, input.options_));
        const auto basis = input.result_->basisCurves_.at(pair);
        const Dal::String_ parameterBlock = Dal::String_("basis:xccy_basis_") + pair.domestic_.String();
        input.config_.calibrationId_ = "staged-xccy-calibration";
        input.config_.componentKeyByParameterBlock_[parameterBlock] = "staged-basis-component";
        input.market_.valuationTime_ = input.spec_.valuationTime_;
        input.market_.resultCurrency_ = pair.domestic_;
        input.market_.curveComponents_["staged-basis-component"] = basis;
        input.market_.fixings_ = input.result_->market_.Fixings();
        input.market_.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(input.result_->market_);
        return input;
    }

    StagedProvenanceInput_ MakeStagedInput(Dal::CurveJacobianMode_ mode, double dependencyShift = 0.0, bool computeInverse = true) {
        return MakeStagedInputWithQuoteCount(mode, 2, dependencyShift, computeInverse);
    }

    Dal::RateTradeDefinition_
    XccyRiskTrade(const Dal::Date_& today, const Dal::Date_& maturity, const Dal::CurrencyPair_& pair, const Dal::String_& instrumentId) {
        Dal::XccyTradeTerms_ terms;
        terms.positionCount_ = 10'000.0;
        terms.contractSpread_ = 0.0015;
        terms.spreadOnForeignLeg_ = true;
        terms.receiveNonSpreadPaySpread_ = true;
        terms.config_ = JointXccyConfig(pair);
        return {instrumentId, Dal::RateInstrumentType_::Value_::XCCY, today, today, maturity, pair.domestic_, terms};
    }

    Dal::RateTradeDefinition_
    JointDepositRiskTrade(const JointProvenanceInput_& input, int parameterRange, const Dal::Ccy_& currency, const Dal::String_& instrumentId) {
        Dal::DepositTradeTerms_ terms;
        terms.notional_ = 1'000'000.0;
        terms.contractRate_ = 0.02;
        terms.lend_ = true;
        terms.index_ = SingleIndex();
        const auto& range = input.result_.parameterRanges_[parameterRange];
        terms.discountComponentKey_ = input.config_.componentKeyByParameterBlock_.at(range.name_);
        return {instrumentId,
                Dal::RateInstrumentType_::Value_::DEPOSIT,
                input.spec_.valuationTime_.Date(),
                input.spec_.valuationTime_.Date(),
                input.spec_.basis_.knotDates_.back(),
                currency,
                terms};
    }

    bool BumpJointCurveQuote(int quoteIndex, int* offset, Dal::Vector_<Dal::Handle_<Dal::YCInstrument_>>* instruments, double bump) {
        const int nextOffset = *offset + static_cast<int>(instruments->size());
        if (quoteIndex >= nextOffset) {
            *offset = nextOffset;
            return false;
        }
        const int localIndex = quoteIndex - *offset;
        REQUIRE(localIndex >= 0, "Joint quote-risk oracle quote index is invalid");
        const auto* original = dynamic_cast<const Dal::Deposit_*>((*instruments)[localIndex].get());
        REQUIRE(original, "Joint quote-risk oracle expected deposit calibration instruments");
        const auto span = original->TimeSpan();
        (*instruments)[localIndex] = Dal::Handle_<Dal::YCInstrument_>(
            new Dal::Deposit_(original->TradeDate(), span.first, span.second, original->MarketRate() + bump, original->FloatConvention()));
        return true;
    }

    Dal::JointXccyCalibrationSpec_ BumpJointQuote(const Dal::JointXccyCalibrationSpec_& spec, int quoteIndex, double bump) {
        Dal::JointXccyCalibrationSpec_ result = spec;
        int offset = 0;
        for (auto& declaration : result.domestic_.curves_)
            if (BumpJointCurveQuote(quoteIndex, &offset, &declaration.instruments_, bump))
                return result;
        for (auto& declaration : result.foreign_.curves_)
            if (BumpJointCurveQuote(quoteIndex, &offset, &declaration.instruments_, bump))
                return result;
        const int basisIndex = quoteIndex - offset;
        REQUIRE(basisIndex >= 0 && basisIndex < static_cast<int>(result.basis_.instruments_.size()),
                "Joint quote-risk oracle quote index is invalid");
        const auto& original = result.basis_.instruments_[basisIndex];
        const auto span = original->TimeSpan();
        result.basis_.instruments_[basisIndex] = Dal::Handle_<Dal::CrossCurrencySwap_>(
            new Dal::CrossCurrencySwap_(span.first, span.first, span.second, original->MarketRate() + bump, original->Config()));
        return result;
    }

    Dal::CrossCurrencyCalibrationSpec_ BumpStagedQuote(const Dal::CrossCurrencyCalibrationSpec_& spec, int quoteIndex, double bump) {
        Dal::CrossCurrencyCalibrationSpec_ result = spec;
        REQUIRE(quoteIndex >= 0 && quoteIndex < static_cast<int>(result.instruments_.size()), "Staged quote-risk oracle quote index is invalid");
        const auto& original = result.instruments_[quoteIndex];
        const auto span = original->TimeSpan();
        result.instruments_[quoteIndex] = Dal::Handle_<Dal::CrossCurrencySwap_>(
            new Dal::CrossCurrencySwap_(span.first, span.first, span.second, original->MarketRate() + bump, original->Config()));
        return result;
    }

    Dal::RatePricingMarket_ StagedPricingMarket(const StagedProvenanceInput_& input, const Dal::CrossCurrencyCalibrationResult_& result) {
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = input.spec_.valuationTime_;
        market.resultCurrency_ = input.spec_.basisPair_.domestic_;
        market.fixings_ = result.market_.Fixings();
        const Dal::String_& component = input.config_.componentKeyByParameterBlock_.begin()->second;
        market.curveComponents_[component] = result.basisCurves_.at(input.spec_.basisPair_);
        market.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(result.market_);
        return market;
    }

    Dal::Vector_<> PwcParameters(const Dal::DiscountCurve_& curve) {
        const auto* typed = dynamic_cast<const Dal::Tape::DiscountPWC_<double>*>(&curve);
        REQUIRE(typed, "Quote-risk oracle expected a piecewise-constant curve");
        return typed->FRight();
    }

    void SeedJointInitialGuess(const Dal::JointXccyCalibrationResult_& baseline, Dal::JointXccyCalibrationSpec_* spec) {
        const Dal::CollateralType_ collateral(Dal::CollateralType_::Value_::OIS);
        spec->domestic_.curves_[0].initialGuessPerNode_ = PwcParameters(baseline.domesticCurveBlock_->Discount(collateral));
        spec->domestic_.curves_[1].initialGuessPerNode_ = PwcParameters(baseline.domesticCurveBlock_->Forward(Dal::PeriodLength_("3M"), collateral));
        spec->foreign_.curves_[0].initialGuessPerNode_ = PwcParameters(baseline.foreignCurveBlock_->Discount(collateral));
        spec->foreign_.curves_[1].initialGuessPerNode_ = PwcParameters(baseline.foreignCurveBlock_->Forward(Dal::PeriodLength_("3M"), collateral));
        spec->basis_.initialGuessPerNode_ = PwcParameters(*baseline.basisCurve_);
    }

    QuoteOracleObservation_ RecalibrateAndPriceJointQuoteBump(const JointProvenanceInput_& input,
                                                              const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                                              int quoteIndex,
                                                              double bump) {
        auto bumped = BumpJointQuote(input.spec_, quoteIndex, bump);
        SeedJointInitialGuess(input.result_, &bumped);
        auto options = input.options_;
        options.computeForwardJacobian_ = false;
        options.computeEffJacobianInverse_ = false;
        try {
            const auto calibrated = Dal::CalibrateJointXccyMarket(bumped, options);
            return PriceQuoteOracleTrades(trades, JointPricingMarket(bumped, calibrated, input.config_));
        } catch (const std::exception& exception) {
            THROW("Joint quote-risk oracle recalibration failed for quote " + Dal::String::FromInt(quoteIndex) + ": " +
                  Dal::String_(exception.what()));
        }
    }

    QuoteOracleObservation_ RecalibrateAndPriceStagedQuoteBump(const StagedProvenanceInput_& input,
                                                               const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                                               int quoteIndex,
                                                               double bump) {
        auto bumped = BumpStagedQuote(input.spec_, quoteIndex, bump);
        bumped.initialGuessPerNode_ = PwcParameters(*input.result_->basisCurves_.at(input.spec_.basisPair_));
        auto options = input.options_;
        options.computeForwardJacobian_ = false;
        options.computeEffJacobianInverse_ = false;
        try {
            const auto calibrated = Dal::CalibrateCrossCurrencyMarket(bumped, options);
            return PriceQuoteOracleTrades(trades, StagedPricingMarket(input, calibrated));
        } catch (const std::exception& exception) {
            THROW("Staged quote-risk oracle recalibration failed for quote " + Dal::String::FromInt(quoteIndex) + ": " +
                  Dal::String::FromDouble(bump) + ": " + Dal::String_(exception.what()));
        }
    }

    void AssertJointQuoteRiskOracle(int quoteCount, Dal::CurveJacobianMode_ mode) {
        const int curveQuoteCount = quoteCount == 16 ? 3 : quoteCount / 5;
        const int basisQuoteCount = quoteCount - 4 * curveQuoteCount;
        const auto input = MakeJointInputWithWidths(mode, curveQuoteCount, basisQuoteCount);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        const Dal::Vector_<Dal::RateTradeDefinition_> trades = {
            XccyRiskTrade(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), input.spec_.pair_, "joint-oracle")};
        const auto aggregate = Dal::AggregateRatePortfolioQuoteRisk(trades, input.market_, {provenance});
        ASSERT_TRUE(aggregate.provenanceFailures_.empty());
        ASSERT_EQ(static_cast<int>(aggregate.buckets_.size()), quoteCount);
        const auto baseline = PriceQuoteOracleTrades(trades, input.market_);
        const double tolerance = QuoteOracleTolerance(quoteCount);
        for (int i = 0; i < quoteCount; ++i) {
            const auto oracle =
                FullRecalibrationQuoteRisk(baseline, [&](double bump) { return RecalibrateAndPriceJointQuoteBump(input, trades, i, bump); });
            AssertQuoteRiskBucket(aggregate.buckets_[i], oracle, tolerance, "JOINT_XCCY", mode, quoteCount, i);
        }
    }

    void AssertStagedQuoteRiskOracle(int quoteCount, Dal::CurveJacobianMode_ mode) {
        const auto input = MakeStagedInputWithQuoteCount(mode, quoteCount);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        const Dal::Date_ maturity = input.spec_.instruments_.back()->TimeSpan().second;
        const Dal::Vector_<Dal::RateTradeDefinition_> trades = {XccyRiskTrade(input.spec_.today_, maturity, input.spec_.basisPair_, "staged-oracle")};
        const auto aggregate = Dal::AggregateRatePortfolioQuoteRisk(trades, input.market_, {provenance});
        ASSERT_TRUE(aggregate.provenanceFailures_.empty());
        ASSERT_EQ(static_cast<int>(aggregate.buckets_.size()), quoteCount);
        const auto baseline = PriceQuoteOracleTrades(trades, input.market_);
        const double tolerance = QuoteOracleTolerance(quoteCount);
        for (int i = 0; i < quoteCount; ++i) {
            const auto oracle =
                FullRecalibrationQuoteRisk(baseline, [&](double bump) { return RecalibrateAndPriceStagedQuoteBump(input, trades, i, bump); });
            AssertQuoteRiskBucket(aggregate.buckets_[i], oracle, tolerance, "STAGED_XCCY_BASIS", mode, quoteCount, i);
        }
    }

    void RebindJointMarket(JointProvenanceInput_* input) {
        auto xccy =
            std::make_shared<Dal::CrossCurrencyMarket_>(input->result_.domesticCurveBlock_, input->result_.foreignCurveBlock_, input->spec_.fxSpot_,
                                                        input->spec_.valuationTime_, input->spec_.collateralCurrency_, input->result_.fixings_);
        xccy->SetBasisCurve(input->result_.basisCurve_);
        input->market_.xccyMarket_ = xccy;
    }
} // namespace

TEST(QuoteRiskProvenanceTest, TestCoreHeaderFactorySignatures) {
    Dal::RateQuoteRiskProvenanceConfig_ config;
    config.calibrationId_ = "header-isolation";
    ASSERT_EQ(config.calibrationId_, "header-isolation");
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeSingleInput(mode);
        const Dal::RateQuoteRiskProvenance_ provenance =
            Dal::BuildSingleCurveQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);

        ASSERT_TRUE(provenance.Available());
        ASSERT_TRUE(provenance.Reason().empty());
        ASSERT_EQ(provenance.Kind(), "SINGLE_CURVE");
        ASSERT_EQ(provenance.CalibrationId(), input.config_.calibrationId_);
        ASSERT_EQ(provenance.ComponentKeyByParameterBlock(), input.config_.componentKeyByParameterBlock_);
        ASSERT_EQ(provenance.Axis().scheme_, Dal::RateQuoteRiskAxisFingerprintScheme());
        ASSERT_EQ(provenance.State().scheme_, Dal::RateQuoteRiskStateFingerprintScheme());
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().parameters_.size(), 2);
        ASSERT_EQ(provenance.Axis().quotes_.size(), 2);
        ASSERT_EQ(provenance.State().components_.size(), 1);
        ASSERT_EQ(provenance.EffectiveInverse().Rows(), 2);
        ASSERT_EQ(provenance.EffectiveInverse().Cols(), 2);
        ASSERT_DOUBLE_EQ(provenance.Tolerance(), input.spec_.tolerance_);
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC) {
            analyticAxis = provenance.Axis().fingerprint_;
            ASSERT_EQ(analyticAxis, "sha256:da526c4aad2e15b7adbf95f5e2ebd33a20779d27b810edde48baf2eb0754f1ad");
        } else {
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
        }
    }
}

TEST(QuoteRiskAggregationTest, TestSingleCurveProducesUnitBearingBuckets) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    const auto trade = SingleDepositTrade(input);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_EQ(result.policy_, "UnconvertedByActualPvCcy");
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size());
    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_TRUE(result.meta_.front().eligible_);
    ASSERT_TRUE(result.provenanceFailures_.empty());
    ASSERT_EQ(result.pvByActualPvCcy_.size(), 1U);
    ASSERT_TRUE(result.pvByActualPvCcy_.count("USD"));
    for (int i = 0; i < static_cast<int>(result.buckets_.size()); ++i) {
        const auto& bucket = result.buckets_[i];
        ASSERT_EQ(bucket.calibrationId_, provenance.CalibrationId());
        ASSERT_EQ(bucket.axisFingerprint_, provenance.Axis().fingerprint_);
        ASSERT_EQ(bucket.quoteName_, provenance.Axis().quotes_[i].displayName_);
        ASSERT_EQ(bucket.residualBlock_, provenance.Axis().quotes_[i].blockKey_);
        ASSERT_EQ(bucket.quoteOrdinal_, provenance.Axis().quotes_[i].blockOrdinal_);
        ASSERT_EQ(bucket.actualPvCcy_, Dal::Ccy_("USD"));
        ASSERT_TRUE(std::isfinite(bucket.dPvDDecimalQuote_));
        ASSERT_NEAR(bucket.dv01_, bucket.dPvDDecimalQuote_ * 1.0e-4, 1.0e-12);
    }
}

TEST(QuoteRiskAggregationTest, TestStaleStateFailsBeforeAnyNodeRiskWork) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    input.market_.valuationTime_ = Dal::DateTime_(input.spec_.today_, 10, 0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount.store(0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.store(0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({SingleDepositTrade(input)}, input.market_, {provenance});

    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_TRUE(result.meta_.empty());
    ASSERT_EQ(result.provenanceFailures_.size(), 1U);
    ASSERT_EQ(result.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
    ASSERT_EQ(result.provenanceFailures_.front().componentKey_, "discount");
    ASSERT_EQ(result.provenanceFailures_.front().expectedStateFingerprint_, provenance.State().components_.front().fingerprint_);
    ASSERT_NE(result.provenanceFailures_.front().actualStateFingerprint_, result.provenanceFailures_.front().expectedStateFingerprint_);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPassivePriceCount.load(), 0);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(), 0);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
}

TEST(QuoteRiskAggregationTest, TestDuplicateCalibrationIdsFailClosed) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    try {
        static_cast<void>(Dal::AggregateRatePortfolioQuoteRisk({SingleDepositTrade(input)}, input.market_, {provenance, provenance}));
        FAIL() << "Expected duplicate calibration ids to fail";
    } catch (const Dal::Exception_& exception) {
        ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_DUPLICATE_CALIBRATION_ID"), std::string::npos) << exception.what();
    }
}

TEST(QuoteRiskAggregationTest, TestUnavailableProvenanceIsReportedWithoutSweeping) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    input.options_.computeEffJacobianInverse_ = false;
    RecalibrateAndBindSingle(&input);
    const auto provenance = BuildSingle(input);
    ASSERT_FALSE(provenance.Available());
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({SingleDepositTrade(input)}, input.market_, {provenance});

    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_TRUE(result.meta_.empty());
    ASSERT_EQ(result.provenanceFailures_.size(), 1U);
    ASSERT_EQ(result.provenanceFailures_.front().reason_, "QUOTE_RISK_INVERSE_NOT_REQUESTED");
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
}

TEST(QuoteRiskAggregationTest, TestEmptyInputsRemainSuccessful) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    const auto emptyTrades = Dal::AggregateRatePortfolioQuoteRisk({}, input.market_, {provenance});
    ASSERT_TRUE(emptyTrades.buckets_.empty());
    ASSERT_TRUE(emptyTrades.meta_.empty());
    ASSERT_TRUE(emptyTrades.pvByActualPvCcy_.empty());
    ASSERT_TRUE(emptyTrades.provenanceFailures_.empty());

    const auto emptyProvenances = Dal::AggregateRatePortfolioQuoteRisk({SingleDepositTrade(input)}, input.market_, {});
    ASSERT_TRUE(emptyProvenances.buckets_.empty());
    ASSERT_TRUE(emptyProvenances.meta_.empty());
    ASSERT_TRUE(emptyProvenances.provenanceFailures_.empty());
    ASSERT_EQ(emptyProvenances.pvByActualPvCcy_.size(), 1U);
    ASSERT_TRUE(emptyProvenances.pvByActualPvCcy_.count("USD"));
}

TEST(QuoteRiskAggregationTest, TestNonDependencyUsesStructuralZeroWithoutSweeping) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    input.market_.curveComponents_["unrelated"] = input.market_.curveComponents_.at("discount");
    auto trade = SingleDepositTrade(input);
    std::get<Dal::DepositTradeTerms_>(trade.terms_).discountComponentKey_ = "unrelated";
    Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.store(0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_TRUE(result.meta_.front().eligible_);
    ASSERT_TRUE(result.meta_.front().structuralZero_);
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size());
    for (const auto& bucket : result.buckets_) {
        ASSERT_DOUBLE_EQ(bucket.dPvDDecimalQuote_, 0.0);
        ASSERT_DOUBLE_EQ(bucket.dv01_, 0.0);
    }
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(), 0);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
}

TEST(QuoteRiskAggregationTest, TestFailedNonDependencyRemainsStructuralZeroWithoutSweeping) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    input.market_.curveComponents_["unrelated"] = input.market_.curveComponents_.at("discount");
    auto trade = SingleDepositTrade(input);
    auto& terms = std::get<Dal::DepositTradeTerms_>(trade.terms_);
    terms.discountComponentKey_ = "unrelated";
    terms.notional_ = 0.0;
    Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.store(0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_TRUE(result.meta_.front().eligible_);
    ASSERT_TRUE(result.meta_.front().structuralZero_);
    ASSERT_TRUE(result.meta_.front().reason_.empty());
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size());
    for (const auto& bucket : result.buckets_) {
        ASSERT_DOUBLE_EQ(bucket.dPvDDecimalQuote_, 0.0);
        ASSERT_DOUBLE_EQ(bucket.dv01_, 0.0);
    }
    ASSERT_TRUE(result.pvByActualPvCcy_.empty());
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(), 0);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
}

TEST(QuoteRiskAggregationTest, TestFailedDependencyPlanDoesNotClaimStructuralZero) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    input.market_.curveComponents_["unrelated"] = input.market_.curveComponents_.at("discount");
    auto trade = SingleDepositTrade(input);
    std::get<Dal::DepositTradeTerms_>(trade.terms_).discountComponentKey_ = "unrelated";
    trade.maturityDate_ = trade.startDate_;

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_FALSE(result.meta_.front().eligible_);
    ASSERT_FALSE(result.meta_.front().structuralZero_);
    ASSERT_EQ(result.meta_.front().reason_, "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE");
    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_TRUE(result.pvByActualPvCcy_.empty());
}

TEST(QuoteRiskAggregationTest, TestTradeFailureDoesNotAffectSiblingTrade) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    const auto good = SingleDepositTrade(input, "good-deposit");
    auto bad = SingleDepositTrade(input, "bad-deposit");
    std::get<Dal::DepositTradeTerms_>(bad.terms_).notional_ = 0.0;

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({bad, good}, input.market_, {provenance});

    ASSERT_EQ(result.meta_.size(), 2U);
    ASSERT_FALSE(result.meta_[0].eligible_);
    ASSERT_EQ(result.meta_[0].reason_, "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE");
    ASSERT_EQ(result.meta_[0].failingComponentKey_, "discount");
    ASSERT_EQ(result.meta_[0].originalNodeRiskReason_, "TRADE_VALIDATION_FAILED");
    ASSERT_TRUE(result.meta_[1].eligible_);
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size());
    ASSERT_EQ(result.pvByActualPvCcy_.size(), 1U);
}

TEST(QuoteRiskAggregationTest, TestActualPvCurrenciesRemainSeparateAndQuoteKeysIgnoreDuplicateNames) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    ASSERT_EQ(provenance.Axis().quotes_[0].displayName_, provenance.Axis().quotes_[1].displayName_);
    auto usd = SingleDepositTrade(input, "usd-deposit");
    auto eur = SingleDepositTrade(input, "eur-deposit");
    eur.currencyOrPair_ = Dal::Ccy_("EUR");

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({usd, eur}, input.market_, {provenance});

    ASSERT_EQ(result.pvByActualPvCcy_.size(), 2U);
    ASSERT_TRUE(result.pvByActualPvCcy_.count("USD"));
    ASSERT_TRUE(result.pvByActualPvCcy_.count("EUR"));
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size() * 2U);
    std::set<Dal::String_> keys;
    std::set<Dal::String_> currencies;
    for (const auto& bucket : result.buckets_) {
        keys.insert(bucket.quoteKey_);
        currencies.insert(bucket.actualPvCcy_.String());
    }
    ASSERT_EQ(keys.size(), provenance.Axis().quotes_.size());
    ASSERT_EQ(currencies, (std::set<Dal::String_>{"EUR", "USD"}));
}

TEST(QuoteRiskAggregationTest, TestNegativeAndOffsettingTradesRemainValid) {
    const auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = BuildSingle(input);
    auto lend = SingleDepositTrade(input, "lend");
    auto borrow = SingleDepositTrade(input, "borrow");
    std::get<Dal::DepositTradeTerms_>(borrow.terms_).lend_ = false;

    const auto lendOnly = Dal::AggregateRatePortfolioQuoteRisk({lend}, input.market_, {provenance});
    const auto borrowOnly = Dal::AggregateRatePortfolioQuoteRisk({borrow}, input.market_, {provenance});
    ASSERT_LT(lendOnly.pvByActualPvCcy_.at("USD") * borrowOnly.pvByActualPvCcy_.at("USD"), 0.0);
    ASSERT_NEAR(lendOnly.pvByActualPvCcy_.at("USD"), -borrowOnly.pvByActualPvCcy_.at("USD"), 1.0e-10);

    const auto offsetting = Dal::AggregateRatePortfolioQuoteRisk({lend, borrow}, input.market_, {provenance});
    ASSERT_NEAR(offsetting.pvByActualPvCcy_.at("USD"), 0.0, 1.0e-10);
    for (const auto& bucket : offsetting.buckets_) {
        ASSERT_NEAR(bucket.dPvDDecimalQuote_, 0.0, 1.0e-8);
        ASSERT_NEAR(bucket.dv01_, 0.0, 1.0e-12);
    }
}

TEST(QuoteRiskAggregationTest, TestStaleProvenanceDoesNotBlockFreshSibling) {
    auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto stale = BuildSingle(input);
    input.market_.valuationTime_ = Dal::DateTime_(input.spec_.today_, 10, 0);
    input.config_.calibrationId_ = "fresh-calibration";
    const auto fresh = BuildSingle(input);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({SingleDepositTrade(input)}, input.market_, {stale, fresh});

    ASSERT_EQ(result.provenanceFailures_.size(), 1U);
    ASSERT_EQ(result.provenanceFailures_.front().calibrationId_, stale.CalibrationId());
    ASSERT_EQ(result.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
    ASSERT_EQ(result.buckets_.size(), fresh.Axis().quotes_.size());
    for (const auto& bucket : result.buckets_)
        ASSERT_EQ(bucket.calibrationId_, fresh.CalibrationId());
}

TEST(QuoteRiskAggregationTest, TestJointAndStagedProvenancesAreAccepted) {
    const auto joint = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto jointProvenance = Dal::BuildJointXccyQuoteRiskProvenance(joint.spec_, joint.result_, joint.options_, joint.market_, joint.config_);
    const auto jointResult = Dal::AggregateRatePortfolioQuoteRisk({}, joint.market_, {jointProvenance});
    ASSERT_TRUE(jointResult.buckets_.empty());
    ASSERT_TRUE(jointResult.provenanceFailures_.empty());

    const auto staged = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto stagedProvenance =
        Dal::BuildStagedXccyBasisQuoteRiskProvenance(staged.spec_, *staged.result_, staged.options_, staged.market_, staged.config_);
    const auto stagedResult = Dal::AggregateRatePortfolioQuoteRisk({}, staged.market_, {stagedProvenance});
    ASSERT_TRUE(stagedResult.buckets_.empty());
    ASSERT_TRUE(stagedResult.provenanceFailures_.empty());
}

TEST(QuoteRiskAggregationTest, TestJointEquivalentCloneBindingsFailClosedBeforeStructuralZero) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    for (auto& [component, curve] : input.market_.curveComponents_) {
        const auto clone = IndependentCurveClone(*curve);
        ASSERT_NE(clone.get(), curve.get()) << component;
        curve = clone;
    }
    const auto trade = XccyRiskTrade(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), input.spec_.pair_, "joint-clone");

    AssertClonedXccyBindingFailsClosed(
        [&]() { return Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_); },
        input.market_, trade, "joint XCCY");
}

TEST(QuoteRiskAggregationTest, TestStagedEquivalentCloneBindingFailsClosedBeforeStructuralZero) {
    auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const Dal::String_ component = input.config_.componentKeyByParameterBlock_.begin()->second;
    const auto original = input.market_.curveComponents_.at(component);
    input.market_.curveComponents_[component] = IndependentCurveClone(*original);
    ASSERT_NE(input.market_.curveComponents_.at(component).get(), original.get());
    const auto trade = XccyRiskTrade(input.spec_.today_, input.spec_.knotDates_.back(), input.spec_.basisPair_, "staged-clone");

    AssertClonedXccyBindingFailsClosed(
        [&]() { return Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_); },
        input.market_, trade, "staged XCCY basis");
}

TEST(QuoteRiskAggregationTest, TestJointAndStagedProduceXccyBucketsForAnalyticAndBumpedInverses) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto joint = MakeJointInput(mode);
        const auto jointProvenance = Dal::BuildJointXccyQuoteRiskProvenance(joint.spec_, joint.result_, joint.options_, joint.market_, joint.config_);
        const auto jointTrade =
            XccyRiskTrade(joint.spec_.valuationTime_.Date(), joint.spec_.basis_.knotDates_.back(), joint.spec_.pair_, "joint-xccy");
        const auto jointResult = Dal::AggregateRatePortfolioQuoteRisk({jointTrade}, joint.market_, {jointProvenance});
        ASSERT_TRUE(jointResult.provenanceFailures_.empty());
        ASSERT_EQ(jointResult.meta_.size(), 1U);
        ASSERT_TRUE(jointResult.meta_.front().eligible_);
        ASSERT_FALSE(jointResult.meta_.front().structuralZero_);
        ASSERT_EQ(jointResult.buckets_.size(), jointProvenance.Axis().quotes_.size());
        for (const auto& bucket : jointResult.buckets_)
            ASSERT_EQ(bucket.actualPvCcy_, joint.spec_.pair_.domestic_);

        auto staged = MakeStagedInput(mode);
        const auto stagedProvenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(staged.spec_, *staged.result_, staged.options_, staged.market_, staged.config_);
        const auto stagedTrade = XccyRiskTrade(staged.spec_.today_, staged.spec_.knotDates_.back(), staged.spec_.basisPair_, "staged-xccy-basis");
        const auto stagedResult = Dal::AggregateRatePortfolioQuoteRisk({stagedTrade}, staged.market_, {stagedProvenance});
        ASSERT_TRUE(stagedResult.provenanceFailures_.empty());
        ASSERT_EQ(stagedResult.meta_.size(), 1U);
        ASSERT_TRUE(stagedResult.meta_.front().eligible_);
        ASSERT_FALSE(stagedResult.meta_.front().structuralZero_);
        ASSERT_EQ(stagedResult.buckets_.size(), stagedProvenance.Axis().quotes_.size());
        for (const auto& bucket : stagedResult.buckets_) {
            ASSERT_EQ(bucket.residualBlock_, stagedProvenance.Axis().residualRanges_.front().blockKey_);
            ASSERT_EQ(bucket.actualPvCcy_, staged.spec_.basisPair_.domestic_);
        }
    }
}

TEST(QuoteRiskAggregationTest, TestJointConsumedBlockFailureDiscardsSuccessfulSiblingGradients) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto trade = XccyRiskTrade(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), input.spec_.pair_, "joint-failure");
    const auto& failingRange = provenance.Axis().parameterRanges_[2];
    const Dal::String_ failingComponent = provenance.ComponentKeyByParameterBlock().at(failingRange.blockKey_);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0, std::memory_order_relaxed);

    Dal::RatePortfolioQuoteRisk_ result;
    {
        const ScopedQuoteRiskSweepFailure_ failure(failingComponent);
        result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});
    }

    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_FALSE(result.meta_.front().eligible_);
    ASSERT_EQ(result.meta_.front().reason_, "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE");
    ASSERT_EQ(result.meta_.front().failingComponentKey_, failingComponent);
    ASSERT_EQ(result.meta_.front().originalNodeRiskReason_, "QUOTE_RISK_TEST_FORCED_SWEEP_FAILURE");
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(std::memory_order_relaxed), 2);
}

TEST(QuoteRiskAggregationTest, TestJointStructuralZeroSkipsEveryBlockSweep) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto& firstRange = input.result_.parameterRanges_.front();
    const Dal::String_ firstComponent = input.config_.componentKeyByParameterBlock_.at(firstRange.name_);
    input.market_.curveComponents_["unrelated-discount"] = input.market_.curveComponents_.at(firstComponent);
    auto trade = JointDepositRiskTrade(input, 0, input.spec_.pair_.domestic_, "joint-structural-zero");
    std::get<Dal::DepositTradeTerms_>(trade.terms_).discountComponentKey_ = "unrelated-discount";
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0, std::memory_order_relaxed);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_TRUE(result.provenanceFailures_.empty());
    ASSERT_EQ(result.meta_.size(), 1U);
    ASSERT_TRUE(result.meta_.front().eligible_);
    ASSERT_TRUE(result.meta_.front().structuralZero_);
    ASSERT_EQ(result.buckets_.size(), provenance.Axis().quotes_.size());
    for (const auto& bucket : result.buckets_) {
        ASSERT_DOUBLE_EQ(bucket.dPvDDecimalQuote_, 0.0);
        ASSERT_DOUBLE_EQ(bucket.dv01_, 0.0);
    }
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(std::memory_order_relaxed), 0);
}

TEST(QuoteRiskAggregationTest, TestStagedDependencyDriftFailsBeforeAnySweep) {
    auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
    const auto shifted = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, 0.001);
    input.market_.xccyMarket_ = shifted.market_.xccyMarket_;
    const auto trade = XccyRiskTrade(input.spec_.today_, input.spec_.knotDates_.back(), input.spec_.basisPair_, "stale-staged-dependency");
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0, std::memory_order_relaxed);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_EQ(result.provenanceFailures_.size(), 1U);
    ASSERT_EQ(result.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(std::memory_order_relaxed), 0);
}

TEST(QuoteRiskAggregationTest, TestJointActualPvCurrenciesRemainSeparate) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto usdTrade = XccyRiskTrade(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), input.spec_.pair_, "usd-xccy");
    const auto eurTrade = JointDepositRiskTrade(input, 2, input.spec_.pair_.foreign_, "eur-deposit");

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({usdTrade, eurTrade}, input.market_, {provenance});

    ASSERT_TRUE(result.provenanceFailures_.empty());
    ASSERT_EQ(result.pvByActualPvCcy_.size(), 2U);
    ASSERT_EQ(result.buckets_.size(), 2U * provenance.Axis().quotes_.size());
    std::set<Dal::String_> currencies;
    for (const auto& bucket : result.buckets_)
        currencies.insert(bucket.actualPvCcy_.String());
    ASSERT_EQ(currencies, (std::set<Dal::String_>{input.spec_.pair_.domestic_.String(), input.spec_.pair_.foreign_.String()}));
}

TEST(QuoteRiskAggregationTest, TestFullRecalibrationOracleAtRequiredWidthsAndModes) {
    for (const int quoteCount : {5, 10, 16})
        for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
            for (const auto parameterization :
                 {Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
                  Dal::CurveParameterization_::Value_::LOG_DISCOUNT, Dal::CurveParameterization_::Value_::ZERO_RATE}) {
                SCOPED_TRACE(Dal::String_("N=") + Dal::String::FromInt(quoteCount) + " mode=" + Dal::CurveJacobianMode_(mode).String() +
                             " parameterization=" + Dal::CurveParameterization_(parameterization).String());
                try {
                    AssertQuoteRiskOracle(parameterization, quoteCount, mode);
                } catch (const std::exception& exception) {
                    FAIL() << "N=" << quoteCount << " mode=" << Dal::CurveJacobianMode_(mode).String()
                           << " parameterization=" << Dal::CurveParameterization_(parameterization).String() << ": " << exception.what();
                }
            }
}

TEST(QuoteRiskAggregationTest, TestXccyFullRecalibrationOracleAtRequiredWidthsAndModes) {
    for (const int quoteCount : {5, 10, 16})
        for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
            SCOPED_TRACE(Dal::String_("N=") + Dal::String::FromInt(quoteCount) + " mode=" + Dal::CurveJacobianMode_(mode).String());
            AssertJointQuoteRiskOracle(quoteCount, mode);
            AssertStagedQuoteRiskOracle(quoteCount, mode);
        }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveUnavailableReasonsRemainStructured) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.options_.computeEffJacobianInverse_ = false;
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.spec_.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
        input.result_.diagnostics_.usedApproximateFit_ = true;
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::BUMPED);
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE");
    }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveMalformedInputsFailClosed) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.config_.calibrationId_.clear();
        ExpectSingleFailure(&input, "QUOTE_RISK_CALIBRATION_ID_EMPTY");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.config_.componentKeyByParameterBlock_.clear();
        ExpectSingleFailure(&input, "QUOTE_RISK_PARAMETER_BLOCK_BINDINGS_INCOMPLETE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.diagnostics_.effJacobianInverse_(0, 0) = std::numeric_limits<double>::quiet_NaN();
        ExpectSingleFailure(&input, "QUOTE_RISK_EFFECTIVE_INVERSE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.spec_.tolerance_ = std::numeric_limits<double>::infinity();
        ExpectSingleFailure(&input, "QUOTE_RISK_TOLERANCE_INVALID");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.diagnostics_.marketRates_[0] += 1.0e-4;
        ExpectSingleFailure(&input, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
    }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveAxisIsStableWhileStateTracksMutableInputs) {
    auto baselineInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = BuildSingle(baselineInput);

    auto asOfInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    asOfInput.market_.valuationTime_ = Dal::DateTime_(asOfInput.spec_.today_, 10, 0);
    const auto asOf = BuildSingle(asOfInput);

    auto fixingInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    Dal::MarketFixingSnapshot_::values_t fixingValues;
    fixingValues["USD-TEST"][Dal::DateTime_(fixingInput.spec_.today_, 8, 0)] = 0.0123;
    fixingInput.market_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(fixingValues));
    const auto fixing = BuildSingle(fixingInput);

    auto inverseInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    inverseInput.result_.diagnostics_.effJacobianInverse_(0, 0) += 1.0e-12;
    const auto inverse = BuildSingle(inverseInput);

    auto toleranceInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    toleranceInput.spec_.tolerance_ *= 10.0;
    const auto tolerance = BuildSingle(toleranceInput);

    for (const auto* changed : {&asOf, &fixing, &inverse, &tolerance}) {
        ASSERT_EQ(changed->Axis().fingerprint_, baseline.Axis().fingerprint_);
        ASSERT_NE(changed->State().fingerprint_, baseline.State().fingerprint_);
    }
}

TEST(QuoteRiskProvenanceTest, TestCalibrationIdDoesNotChangeCalibrationStateFingerprint) {
    auto firstInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto first = BuildSingle(firstInput);
    auto secondInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    secondInput.config_.calibrationId_ = "same-state-different-id";
    const auto second = BuildSingle(secondInput);

    ASSERT_NE(first.CalibrationId(), second.CalibrationId());
    ASSERT_EQ(first.Axis().fingerprint_, second.Axis().fingerprint_);
    ASSERT_EQ(first.State().components_.front().fingerprint_, second.State().components_.front().fingerprint_);
    ASSERT_EQ(first.State().fingerprint_, second.State().fingerprint_);

    auto jointInput = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto firstJoint =
        Dal::BuildJointXccyQuoteRiskProvenance(jointInput.spec_, jointInput.result_, jointInput.options_, jointInput.market_, jointInput.config_);
    jointInput.config_.calibrationId_ = "same-joint-state-different-id";
    const auto secondJoint =
        Dal::BuildJointXccyQuoteRiskProvenance(jointInput.spec_, jointInput.result_, jointInput.options_, jointInput.market_, jointInput.config_);
    ASSERT_EQ(firstJoint.State().fingerprint_, secondJoint.State().fingerprint_);

    auto stagedInput = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto firstStaged = Dal::BuildStagedXccyBasisQuoteRiskProvenance(stagedInput.spec_, *stagedInput.result_, stagedInput.options_,
                                                                          stagedInput.market_, stagedInput.config_);
    stagedInput.config_.calibrationId_ = "same-staged-state-different-id";
    const auto secondStaged = Dal::BuildStagedXccyBasisQuoteRiskProvenance(stagedInput.spec_, *stagedInput.result_, stagedInput.options_,
                                                                           stagedInput.market_, stagedInput.config_);
    ASSERT_EQ(firstStaged.State().fingerprint_, secondStaged.State().fingerprint_);
}

TEST(QuoteRiskProvenanceTest, TestJointXccyAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeJointInput(mode);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        ASSERT_TRUE(provenance.Available());
        ASSERT_EQ(provenance.Kind(), "JOINT_XCCY");
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), input.result_.parameterRanges_.size());
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), input.result_.residualRanges_.size());
        ASSERT_EQ(provenance.Axis().parameters_.size(), static_cast<std::size_t>(input.result_.effJacobianInverse_.Rows()));
        ASSERT_EQ(provenance.Axis().quotes_.size(), static_cast<std::size_t>(input.result_.effJacobianInverse_.Cols()));
        ASSERT_EQ(provenance.State().components_.size(), input.result_.parameterRanges_.size());
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC)
            analyticAxis = provenance.Axis().fingerprint_;
        else
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedXccyBasisAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeStagedInput(mode);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        ASSERT_TRUE(provenance.Available());
        ASSERT_EQ(provenance.Kind(), "STAGED_XCCY_BASIS");
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().parameters_.size(), input.spec_.knotDates_.size());
        ASSERT_EQ(provenance.Axis().quotes_.size(), input.spec_.instruments_.size());
        ASSERT_EQ(provenance.State().components_.size(), 1);
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC)
            analyticAxis = provenance.Axis().fingerprint_;
        else
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedInverseMetadataMismatchFailsClosed) {
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_->diagnostics_.effJacobianInverseAvailability_ = "not_requested";
        ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                     Dal::Exception_);
    }
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_->diagnostics_.effJacobianInverseScaling_ = "unscaled";
        ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                     Dal::Exception_);
    }
}

TEST(QuoteRiskProvenanceTest, TestJointMissingLayeredBaseTopologyFailsClosed) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    input.spec_.domestic_.curves_[1].baseLayeredOverDiscount_ = true;
    ASSERT_THROW(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_), Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestStagedUnexpectedBaseTopologyFailsClosed) {
    auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto original = input.result_->basisCurves_.at(input.spec_.basisPair_);
    const auto* typed = dynamic_cast<const Dal::Tape::DiscountPWC_<double>*>(original.get());
    ASSERT_TRUE(typed);
    const auto base = AliasCurve(input.spec_.domesticCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)));
    const Dal::Handle_<Dal::DiscountCurve_> replacement(
        Dal::NewDiscountPWC(original->Name(), original->ccy_.String(), Dal::PiecewiseConstant_(typed->KnotDates(), typed->FRight()), base));
    input.result_->basisCurves_[input.spec_.basisPair_] = replacement;
    input.result_->market_.SetBasisCurve(replacement);
    input.market_.curveComponents_["staged-basis-component"] = replacement;
    input.market_.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(input.result_->market_);
    ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                 Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestJointExtraSlotTopologyFailsClosed) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    auto discounts = input.result_.domesticCurveBlock_->DiscountCurves();
    auto forwards = input.result_.domesticCurveBlock_->ForwardCurves();
    forwards[Dal::PeriodLength_("6M")] =
        JointPwc("unexpected_6m", input.spec_.domestic_.ccy_, input.spec_.domestic_.curves_[0].knotDates_, {0.01, 0.01});
    input.result_.domesticCurveBlock_ =
        Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_(input.result_.domesticCurveBlock_->Name(), input.spec_.domestic_.ccy_.String(), discounts,
                                                            forwards, input.result_.domesticCurveBlock_->LiborBasis()));
    RebindJointMarket(&input);
    ASSERT_THROW(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_), Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestJointRangesBindingsAndNonFiniteStateFailClosed) {
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.parameterRanges_[1].offset_ += 1;
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected gapped parameter ranges to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("complete partition"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        auto binding = input.config_.componentKeyByParameterBlock_.begin();
        const Dal::String_ duplicate = binding->second;
        ++binding;
        binding->second = duplicate;
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected duplicate component binding to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_PARAMETER_BLOCK_BINDING_DUPLICATE"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.effJacobianInverse_(0, 0) = std::numeric_limits<double>::infinity();
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected non-finite joint inverse to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_EFFECTIVE_INVERSE"), std::string::npos) << exception.what();
        }
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedDependenciesChangeStateWithoutChangingAxis) {
    auto baselineInput = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = Dal::BuildStagedXccyBasisQuoteRiskProvenance(baselineInput.spec_, *baselineInput.result_, baselineInput.options_,
                                                                       baselineInput.market_, baselineInput.config_);
    auto changedInput = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, 0.001);
    const auto changed = Dal::BuildStagedXccyBasisQuoteRiskProvenance(changedInput.spec_, *changedInput.result_, changedInput.options_,
                                                                      changedInput.market_, changedInput.config_);
    ASSERT_EQ(changed.Axis().fingerprint_, baseline.Axis().fingerprint_);
    ASSERT_NE(changed.State().fingerprint_, baseline.State().fingerprint_);
    ASSERT_NE(changed.State().components_.front().fingerprint_, baseline.State().components_.front().fingerprint_);
}

TEST(QuoteRiskProvenanceTest, TestXccyFactoriesReturnUnavailableWhenInverseWasNotRequested) {
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, false);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::BUMPED, 0.0, false);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
}

TEST(QuoteRiskProvenanceTest, TestSolvedCurveAndBaseChangesAffectStateNotAxis) {
    auto baselineInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = BuildSingle(baselineInput);

    auto curveInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const Dal::Handle_<Dal::DiscountCurve_> changedKnown(Dal::NewDiscountPWC(
        "single_quote_risk_changed_known", "USD", Dal::PiecewiseConstant_(curveInput.spec_.knotDates_, Dal::Vector_<>{0.021, 0.026})));
    const Dal::CurveBlock_ changedKnownBlock(changedKnown, curveInput.spec_.liborBasis_);
    curveInput.spec_.instruments_.clear();
    for (const auto& maturity : curveInput.spec_.knotDates_) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(
            new Dal::Deposit_(curveInput.spec_.today_, curveInput.spec_.today_, maturity, 0.0, SingleIndex()));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(changedKnownBlock);
        curveInput.spec_.instruments_.push_back(
            Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(curveInput.spec_.today_, curveInput.spec_.today_, maturity, quote, SingleIndex())));
    }
    ASSERT_NO_THROW(RecalibrateAndBindSingle(&curveInput));
    const auto curveChanged = BuildSingle(curveInput);

    auto baseInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    baseInput.spec_.baseCurve_ = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC("single_quote_risk_base", "USD", Dal::PiecewiseConstant_(baseInput.spec_.knotDates_, Dal::Vector_<>{0.001, 0.001})));
    const Dal::Handle_<Dal::DiscountCurve_> knownWithBase(
        Dal::NewDiscountPWC("single_quote_risk_known_with_base", "USD",
                            Dal::PiecewiseConstant_(baseInput.spec_.knotDates_, Dal::Vector_<>{0.02, 0.025}), baseInput.spec_.baseCurve_));
    const Dal::CurveBlock_ knownBaseBlock(knownWithBase, baseInput.spec_.liborBasis_);
    baseInput.spec_.instruments_.clear();
    for (const auto& maturity : baseInput.spec_.knotDates_) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(
            new Dal::Deposit_(baseInput.spec_.today_, baseInput.spec_.today_, maturity, 0.0, SingleIndex()));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(knownBaseBlock);
        baseInput.spec_.instruments_.push_back(
            Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(baseInput.spec_.today_, baseInput.spec_.today_, maturity, quote, SingleIndex())));
    }
    baseInput.spec_.initialGuess_ = 0.02;
    ASSERT_NO_THROW(RecalibrateAndBindSingle(&baseInput));
    const auto baseChanged = BuildSingle(baseInput);

    for (const auto* changed : {&curveChanged, &baseChanged}) {
        ASSERT_EQ(changed->Axis().fingerprint_, baseline.Axis().fingerprint_);
        ASSERT_NE(changed->State().fingerprint_, baseline.State().fingerprint_);
        ASSERT_NE(changed->State().components_.front().fingerprint_, baseline.State().components_.front().fingerprint_);
    }
}

TEST(QuoteRiskProvenanceTest, TestNonFiniteFixingAndCyclicCurveGraphFailBeforeHashing) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        Dal::MarketFixingSnapshot_::values_t values;
        values["USD-NONFINITE"][Dal::DateTime_(input.spec_.today_, 8, 0)] = std::numeric_limits<double>::quiet_NaN();
        try {
            static_cast<void>(Dal::MarketFixingSnapshot_(values));
            FAIL() << "Expected non-finite fixing state to fail before provenance hashing";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("requires finite values"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        auto first = std::make_shared<CyclicDiscountCurve_>("cycle-first");
        auto second = std::make_shared<CyclicDiscountCurve_>("cycle-second");
        const Dal::Handle_<Dal::DiscountCurve_> firstHandle(std::static_pointer_cast<const Dal::DiscountCurve_>(first));
        const Dal::Handle_<Dal::DiscountCurve_> secondHandle(std::static_pointer_cast<const Dal::DiscountCurve_>(second));
        first->SetBase(secondHandle);
        second->SetBase(firstHandle);
        input.spec_.forwardCurves_[Dal::PeriodLength_("3M")] = firstHandle;
        ExpectSingleFailure(&input, "QUOTE_RISK_CYCLIC_BASE_GRAPH");
        first->ClearBase();
        second->ClearBase();
    }
}
