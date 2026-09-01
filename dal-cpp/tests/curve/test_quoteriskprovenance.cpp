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

    template <class BumpReprice_>
    QuoteOracleRisk_ QuoteRiskOracleFromBumps(const QuoteOracleObservation_& baseline, int quoteIndex, BumpReprice_&& bumpReprice) {
        constexpr double h = 1.0e-6;
        constexpr double b = 1.0e-4;
        const auto plusH = bumpReprice(quoteIndex, h);
        const auto minusH = bumpReprice(quoteIndex, -h);
        const auto plusB = bumpReprice(quoteIndex, b);
        const auto minusB = bumpReprice(quoteIndex, -b);
        const double priceScale = std::max({1.0, baseline.grossAbsPv_, plusH.grossAbsPv_, minusH.grossAbsPv_, plusB.grossAbsPv_, minusB.grossAbsPv_});
        return {priceScale, (plusH.netPv_ - minusH.netPv_) / (2.0 * h), (plusB.netPv_ - minusB.netPv_) / 2.0};
    }

    QuoteOracleRisk_ QuoteRiskOracle(const SingleProvenanceInput_& input,
                                     const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                     const QuoteOracleObservation_& baseline,
                                     int quoteIndex) {
        return QuoteRiskOracleFromBumps(baseline, quoteIndex,
                                        [&](int index, double bump) { return RecalibrateAndPriceQuoteBump(input, trades, index, bump); });
    }

    void
    AssertQuoteRiskBucket(const Dal::RateQuoteRiskBucket_& bucket, const QuoteOracleRisk_& oracle, double tolerance, const Dal::String_& context) {
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
            << context << " api=" << bucket.dPvDDecimalQuote_ << " oracle=" << oracle.derivative_ << " absError=" << derivativeError
            << " relError=" << derivativeRelative;
        const double dv01Error = std::abs(bucket.dv01_ - oracle.dv01_);
        const double dv01Relative = RelativeError(dv01Error, std::max(std::abs(bucket.dv01_), std::abs(oracle.dv01_)));
        ASSERT_TRUE(dv01Error <= tolerance * b * oracle.priceScale_ || dv01Relative <= tolerance)
            << context << " api=" << bucket.dv01_ << " oracle=" << oracle.dv01_ << " absError=" << dv01Error << " relError=" << dv01Relative;
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
        for (int i = 0; i < quoteCount; ++i) {
            const Dal::String_ context = Dal::String_("parameterization=") + parameterization.String() + " mode=" + mode.String() +
                                         " N=" + Dal::String::FromInt(quoteCount) + " quote=" + Dal::String::FromInt(i);
            AssertQuoteRiskBucket(aggregate.buckets_[i], QuoteRiskOracle(input, trades, baseline, i), tolerance, context);
        }
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

    JointProvenanceInput_ MakeJointInput(Dal::CurveJacobianMode_ mode, bool computeInverse = true) {
        JointProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::Vector_<Dal::Date_> maturities = {Dal::Date::AddMonths(today, 12), Dal::Date::AddMonths(today, 24)};
        const Dal::Vector_<Dal::Date_> knots = {Dal::Date::AddMonths(today, 6), Dal::Date::AddMonths(today, 18)};
        const auto domestic = JointBlock("usd_true_provenance", Dal::Ccy_("USD"), knots, {0.015, 0.018}, {0.024, 0.027});
        const auto foreign = JointBlock("eur_true_provenance", Dal::Ccy_("EUR"), knots, {0.010, 0.013}, {0.019, 0.022});
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("basis_true_provenance", pair.domestic_, knots, {0.0010, 0.0020}));

        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.pair_ = pair;
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.domestic_ = JointCurrencySpec(today, pair.domestic_, knots, maturities, domestic);
        input.spec_.foreign_ = JointCurrencySpec(today, pair.foreign_, knots, maturities, foreign);
        input.spec_.basis_.curveName_ = "usd_eur_basis_provenance";
        input.spec_.basis_.knotDates_ = knots;
        input.spec_.fixings_ = fixings;
        input.spec_.initialGuess_ = 0.005;
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        const auto config = JointXccyConfig(pair);
        for (const auto& maturity : maturities) {
            const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
            input.spec_.basis_.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        input.options_.jacobianMode_ = mode;
        input.options_.computeEffJacobianInverse_ = computeInverse;
        input.result_ = Dal::CalibrateJointXccyMarket(input.spec_, input.options_);

        Dal::Vector_<Dal::Handle_<Dal::DiscountCurve_>> curves = {
            AliasCurve(input.result_.domesticCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.domesticCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.foreignCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.foreignCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            input.result_.basisCurve_,
        };
        REQUIRE(curves.size() == input.result_.parameterRanges_.size(), "Unexpected joint XCCY parameter range count");
        input.config_.calibrationId_ = "joint-xccy-calibration";
        for (int i = 0; i < static_cast<int>(input.result_.parameterRanges_.size()); ++i) {
            const Dal::String_ component = "joint-component-" + Dal::String::FromInt(i);
            input.config_.componentKeyByParameterBlock_[input.result_.parameterRanges_[i].name_] = component;
            input.market_.curveComponents_[component] = curves[i];
        }
        input.market_.valuationTime_ = input.spec_.valuationTime_;
        input.market_.resultCurrency_ = pair.domestic_;
        input.market_.fixings_ = input.result_.fixings_;
        auto xccy =
            std::make_shared<Dal::CrossCurrencyMarket_>(input.result_.domesticCurveBlock_, input.result_.foreignCurveBlock_, input.spec_.fxSpot_,
                                                        input.spec_.valuationTime_, input.spec_.collateralCurrency_, input.result_.fixings_);
        xccy->SetBasisCurve(input.result_.basisCurve_);
        input.market_.xccyMarket_ = xccy;
        return input;
    }

    struct StagedProvenanceInput_ {
        Dal::CrossCurrencyCalibrationSpec_ spec_;
        Dal::CrossCurrencyCalibrationOptions_ options_;
        std::unique_ptr<Dal::CrossCurrencyCalibrationResult_> result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    StagedProvenanceInput_ MakeStagedInput(Dal::CurveJacobianMode_ mode, double dependencyShift = 0.0, bool computeInverse = true) {
        StagedProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::Vector_<Dal::Date_> knots = {Dal::Date::AddMonths(today, 12), Dal::Date::AddMonths(today, 24)};
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        const auto domestic = JointBlock("usd_staged_provenance", pair.domestic_, knots, {0.015 + dependencyShift, 0.018 + dependencyShift},
                                         {0.024 + dependencyShift, 0.027 + dependencyShift});
        const auto foreign = JointBlock("eur_staged_provenance", pair.foreign_, knots, {0.010 - dependencyShift, 0.013 - dependencyShift},
                                        {0.019 - dependencyShift, 0.022 - dependencyShift});
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const auto config = JointXccyConfig(pair);
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("known_staged_provenance", pair.domestic_, knots, {0.0010, 0.0020}));

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
        for (const auto& maturity : knots) {
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

    void RebindJointMarket(JointProvenanceInput_* input) {
        auto xccy =
            std::make_shared<Dal::CrossCurrencyMarket_>(input->result_.domesticCurveBlock_, input->result_.foreignCurveBlock_, input->spec_.fxSpot_,
                                                        input->spec_.valuationTime_, input->spec_.collateralCurrency_, input->result_.fixings_);
        xccy->SetBasisCurve(input->result_.basisCurve_);
        input->market_.xccyMarket_ = xccy;
    }

    Dal::RateTradeDefinition_ XccyTrade(const Dal::CurrencyPair_& pair,
                                        const Dal::Date_& today,
                                        const Dal::Date_& maturity,
                                        const Dal::String_& instrumentId = "quote-risk-xccy") {
        Dal::XccyTradeTerms_ terms;
        terms.positionCount_ = 1.0;
        terms.contractSpread_ = 0.001;
        terms.spreadOnForeignLeg_ = true;
        terms.receiveNonSpreadPaySpread_ = true;
        terms.config_ = JointXccyConfig(pair);
        return {instrumentId, Dal::RateInstrumentType_::Value_::XCCY, today, today, maturity, pair.domestic_, terms};
    }

    Dal::RateTradeDefinition_ QuoteRiskDeposit(const Dal::Date_& today,
                                               const Dal::Date_& maturity,
                                               const Dal::String_& componentKey,
                                               const Dal::Ccy_& actualPvCcy,
                                               const Dal::String_& instrumentId) {
        Dal::DepositTradeTerms_ terms;
        terms.notional_ = 1'000'000.0;
        terms.contractRate_ = 0.02;
        terms.lend_ = true;
        terms.index_ = JointIndex(false);
        terms.discountComponentKey_ = componentKey;
        return {instrumentId, Dal::RateInstrumentType_::Value_::DEPOSIT, today, today, maturity, actualPvCcy, terms};
    }

    class ScopedQuoteRiskForcedFailure_ {
    public:
        explicit ScopedQuoteRiskForcedFailure_(const Dal::String_& component) {
            Dal::RateCashflowPricingInternal::g_quoteRiskForcedFailureComponent.store(&component);
        }
        ~ScopedQuoteRiskForcedFailure_() { Dal::RateCashflowPricingInternal::g_quoteRiskForcedFailureComponent.store(nullptr); }

        ScopedQuoteRiskForcedFailure_(const ScopedQuoteRiskForcedFailure_&) = delete;
        ScopedQuoteRiskForcedFailure_& operator=(const ScopedQuoteRiskForcedFailure_&) = delete;
    };

    Dal::Vector_<> XccyOracleBasisParameters(int count) {
        Dal::Vector_<> result(count);
        for (int i = 0; i < count; ++i)
            result[i] = 0.0005 + 0.00005 * i;
        return result;
    }

    Dal::Vector_<Dal::Date_> XccyOracleMaturities(const Dal::Date_& today, int count) {
        Dal::Vector_<Dal::Date_> result;
        for (int i = 1; i <= count; ++i)
            result.push_back(Dal::Date::AddMonths(today, 12 * i));
        return result;
    }

    Dal::Vector_<Dal::Date_> XccyOracleKnots(const Dal::Date_& today, int count) {
        Dal::Vector_<Dal::Date_> result;
        for (int i = 0; i < count; ++i)
            result.push_back(Dal::Date::AddMonths(today, 6 + 12 * i));
        return result;
    }

    Dal::Handle_<Dal::CrossCurrencySwap_> XccyOracleInstrument(const Dal::Date_& today,
                                                               const Dal::Date_& maturity,
                                                               const Dal::CrossCurrencySwapConfig_& config,
                                                               const Dal::CrossCurrencyMarket_& market) {
        const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
        return Dal::Handle_<Dal::CrossCurrencySwap_>(new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(market), config));
    }

    Dal::Handle_<Dal::YCInstrument_> BumpOracleDeposit(const Dal::Handle_<Dal::YCInstrument_>& instrument, double bump) {
        const auto* deposit = dynamic_cast<const Dal::Deposit_*>(instrument.get());
        REQUIRE(deposit, "XCCY quote oracle expected a deposit");
        const auto span = deposit->TimeSpan();
        return Dal::Handle_<Dal::YCInstrument_>(
            new Dal::Deposit_(deposit->TradeDate(), span.first, span.second, deposit->MarketRate() + bump, deposit->FloatConvention()));
    }

    Dal::Handle_<Dal::CrossCurrencySwap_> BumpOracleXccy(const Dal::Handle_<Dal::CrossCurrencySwap_>& instrument, double bump) {
        REQUIRE(instrument, "XCCY quote oracle instrument is empty");
        const auto span = instrument->TimeSpan();
        return Dal::Handle_<Dal::CrossCurrencySwap_>(
            new Dal::CrossCurrencySwap_(instrument->TradeDate(), span.first, span.second, instrument->MarketRate() + bump, instrument->Config()));
    }

    struct JointXccyQuoteOracleInput_ {
        Dal::JointXccyCalibrationSpec_ spec_;
        Dal::JointXccyCalibrationOptions_ options_;
        Dal::JointXccyCalibrationResult_ result_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
        Dal::RatePricingMarket_ market_;
        Dal::RateTradeDefinition_ trade_;
    };

    Dal::RatePricingMarket_ JointXccyOracleMarket(const Dal::JointXccyCalibrationSpec_& spec,
                                                  const Dal::JointXccyCalibrationResult_& result,
                                                  const Dal::RateQuoteRiskProvenanceConfig_& config) {
        REQUIRE(result.parameterRanges_.size() == 5, "XCCY quote oracle expected domestic, foreign, and basis parameter blocks");
        const auto collateral = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        const Dal::Vector_<Dal::Handle_<Dal::DiscountCurve_>> curves = {
            AliasCurve(result.domesticCurveBlock_->Discount(collateral)),
            AliasCurve(result.domesticCurveBlock_->Forward(Dal::PeriodLength_("3M"), collateral)),
            AliasCurve(result.foreignCurveBlock_->Discount(collateral)),
            AliasCurve(result.foreignCurveBlock_->Forward(Dal::PeriodLength_("3M"), collateral)), result.basisCurve_};
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.pair_.domestic_;
        market.fixings_ = result.fixings_;
        for (int i = 0; i < static_cast<int>(result.parameterRanges_.size()); ++i)
            market.curveComponents_[config.componentKeyByParameterBlock_.at(result.parameterRanges_[i].name_)] = curves[i];
        auto xccy = std::make_shared<Dal::CrossCurrencyMarket_>(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_,
                                                                spec.valuationTime_, spec.collateralCurrency_, result.fixings_);
        xccy->SetBasisCurve(result.basisCurve_);
        market.xccyMarket_ = xccy;
        return market;
    }

    JointXccyQuoteOracleInput_ MakeJointXccyQuoteOracleInput(int quoteCount, Dal::CurveJacobianMode_ mode) {
        REQUIRE(quoteCount == 5 || quoteCount == 10 || quoteCount == 16, "Joint XCCY quote oracle requires a frozen ladder width");
        JointXccyQuoteOracleInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        const int currencyQuoteCount = quoteCount <= 5 ? 1 : quoteCount <= 10 ? 2 : 3;
        const int basisQuoteCount = quoteCount - 4 * currencyQuoteCount;
        const auto currencyKnots = XccyOracleKnots(today, currencyQuoteCount);
        const auto currencyMaturities = XccyOracleMaturities(today, currencyQuoteCount);
        const auto basisKnots = XccyOracleKnots(today, basisQuoteCount);
        const auto basisMaturities = XccyOracleMaturities(today, basisQuoteCount);
        const Dal::Date_ horizon = std::max(currencyMaturities.back(), basisMaturities.back());
        Dal::Vector_<> domesticDiscount(currencyQuoteCount), domesticForward(currencyQuoteCount), foreignDiscount(currencyQuoteCount),
            foreignForward(currencyQuoteCount);
        for (int i = 0; i < currencyQuoteCount; ++i) {
            domesticDiscount[i] = 0.015 + 0.0002 * i;
            domesticForward[i] = 0.024 + 0.0002 * i;
            foreignDiscount[i] = 0.010 + 0.0002 * i;
            foreignForward[i] = 0.019 + 0.0002 * i;
        }
        const auto domestic = JointBlock("usd_joint_oracle", pair.domestic_, currencyKnots, domesticDiscount, domesticForward);
        const auto foreign = JointBlock("eur_joint_oracle", pair.foreign_, currencyKnots, foreignDiscount, foreignForward);
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const auto config = JointXccyConfig(pair);
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        const auto basisParameters = XccyOracleBasisParameters(static_cast<int>(basisKnots.size()));
        quoteMarket.SetBasisCurve(JointPwc("joint_oracle_basis_truth", pair.domestic_, basisKnots, basisParameters));

        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.pair_ = pair;
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.fixings_ = fixings;
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        input.spec_.initialGuess_ = 0.005;
        input.spec_.domestic_ = JointCurrencySpec(today, pair.domestic_, currencyKnots, currencyMaturities, domestic);
        input.spec_.foreign_ = JointCurrencySpec(today, pair.foreign_, currencyKnots, currencyMaturities, foreign);
        input.spec_.domestic_.curves_[0].initialGuessPerNode_ = domesticDiscount;
        input.spec_.domestic_.curves_[1].initialGuessPerNode_ = domesticForward;
        input.spec_.foreign_.curves_[0].initialGuessPerNode_ = foreignDiscount;
        input.spec_.foreign_.curves_[1].initialGuessPerNode_ = foreignForward;
        input.spec_.basis_.curveName_ = "joint_oracle_basis";
        input.spec_.basis_.knotDates_ = basisKnots;
        input.spec_.basis_.initialGuessPerNode_ = basisParameters;
        for (const auto& maturity : basisMaturities)
            input.spec_.basis_.instruments_.push_back(XccyOracleInstrument(today, maturity, config, quoteMarket));
        input.options_.jacobianMode_ = mode;
        try {
            input.result_ = Dal::CalibrateJointXccyMarket(input.spec_, input.options_);
        } catch (const std::exception& exception) {
            THROW("Joint XCCY quote oracle baseline calibration failed: " + Dal::String_(exception.what()));
        }

        input.config_.calibrationId_ = "joint-xccy-oracle-" + Dal::String::FromInt(quoteCount);
        for (int i = 0; i < static_cast<int>(input.result_.parameterRanges_.size()); ++i)
            input.config_.componentKeyByParameterBlock_[input.result_.parameterRanges_[i].name_] =
                "joint-oracle-component-" + Dal::String::FromInt(i);
        input.market_ = JointXccyOracleMarket(input.spec_, input.result_, input.config_);
        input.trade_ = XccyTrade(pair, today, horizon, "joint-xccy-oracle-trade");
        std::get<Dal::XccyTradeTerms_>(input.trade_.terms_).config_ = config;
        std::get<Dal::XccyTradeTerms_>(input.trade_.terms_).contractSpread_ = 0.25;
        return input;
    }

    QuoteOracleObservation_ RecalibrateAndPriceJointXccyQuoteBump(const JointXccyQuoteOracleInput_& input, int quoteIndex, double bump) {
        Dal::JointXccyCalibrationSpec_ bumped = input.spec_;
        int ordinal = quoteIndex;
        const auto bumpCurrency = [&](Dal::JointCurrencyCurveSpec_* currency) {
            for (auto& declaration : currency->curves_) {
                if (ordinal < static_cast<int>(declaration.instruments_.size())) {
                    declaration.instruments_[ordinal] = BumpOracleDeposit(declaration.instruments_[ordinal], bump);
                    return true;
                }
                ordinal -= static_cast<int>(declaration.instruments_.size());
            }
            return false;
        };
        if (!bumpCurrency(&bumped.domestic_) && !bumpCurrency(&bumped.foreign_))
            bumped.basis_.instruments_[ordinal] = BumpOracleXccy(bumped.basis_.instruments_[ordinal], bump);
        Dal::Vector_<double*> initialGuesses;
        const auto appendGuesses = [&](Dal::JointCurrencyCurveSpec_* currency) {
            for (auto& declaration : currency->curves_)
                for (double& guess : declaration.initialGuessPerNode_)
                    initialGuesses.push_back(&guess);
        };
        appendGuesses(&bumped.domestic_);
        appendGuesses(&bumped.foreign_);
        for (double& guess : bumped.basis_.initialGuessPerNode_)
            initialGuesses.push_back(&guess);
        REQUIRE(static_cast<int>(initialGuesses.size()) == input.result_.effJacobianInverse_.Rows(),
                "Joint XCCY quote oracle initial-guess axis mismatch");
        // Seed the local branch only; the bumped quote is still fully recalibrated and checked
        // against the solver's residual gate before it is used as an oracle price.
        for (int parameter = 0; parameter < static_cast<int>(initialGuesses.size()); ++parameter)
            *initialGuesses[parameter] += input.result_.effJacobianInverse_(parameter, quoteIndex) * bump / input.spec_.tolerance_;
        *initialGuesses.front() += 1.0e-3;
        Dal::JointXccyCalibrationResult_ calibrated;
        try {
            calibrated = Dal::CalibrateJointXccyMarket(bumped, input.options_);
        } catch (const std::exception& exception) {
            THROW("Joint XCCY bumped calibration failed for quote " + Dal::String::FromInt(quoteIndex) + " and bump " +
                  Dal::String::FromDouble(bump) + ": " + Dal::String_(exception.what()));
        }
        REQUIRE(calibrated.jointMaxAbsResidual_ <= input.spec_.fitTolerance_, "Joint XCCY quote oracle calibration failed its fit gate");
        return PriceQuoteOracleTrades({input.trade_}, JointXccyOracleMarket(bumped, calibrated, input.config_));
    }

    struct StagedXccyQuoteOracleInput_ {
        Dal::CrossCurrencyCalibrationSpec_ spec_;
        Dal::CrossCurrencyCalibrationOptions_ options_;
        std::unique_ptr<Dal::CrossCurrencyCalibrationResult_> result_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
        Dal::RatePricingMarket_ market_;
        Dal::RateTradeDefinition_ trade_;
    };

    Dal::RatePricingMarket_ StagedXccyOracleMarket(const Dal::CrossCurrencyCalibrationSpec_& spec,
                                                   const Dal::CrossCurrencyCalibrationResult_& result,
                                                   const Dal::RateQuoteRiskProvenanceConfig_& config) {
        const auto basis = result.basisCurves_.at(spec.basisPair_);
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.basisPair_.domestic_;
        market.fixings_ = result.market_.Fixings();
        market.curveComponents_[config.componentKeyByParameterBlock_.begin()->second] = basis;
        market.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(result.market_);
        return market;
    }

    StagedXccyQuoteOracleInput_ MakeStagedXccyQuoteOracleInput(int quoteCount, Dal::CurveJacobianMode_ mode) {
        StagedXccyQuoteOracleInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        Dal::Vector_<Dal::Date_> basisKnots;
        Dal::Vector_<Dal::Date_> instrumentMaturities;
        for (int i = 1; i <= quoteCount; ++i) {
            basisKnots.push_back(Dal::Date::AddMonths(today, 24 * i));
            instrumentMaturities.push_back(Dal::Date::AddMonths(today, 24 * i + 12));
        }
        const Dal::Date_ horizon = Dal::Date::AddMonths(today, 3);
        Dal::Vector_<> domesticDiscount(quoteCount), domesticForward(quoteCount), foreignDiscount(quoteCount), foreignForward(quoteCount);
        for (int i = 0; i < quoteCount; ++i) {
            domesticDiscount[i] = 0.015 + 0.0001 * i;
            domesticForward[i] = 0.024 + 0.0001 * i;
            foreignDiscount[i] = 0.010 + 0.0001 * i;
            foreignForward[i] = 0.019 + 0.0001 * i;
        }
        const auto domestic = JointBlock("usd_staged_oracle", pair.domestic_, basisKnots, domesticDiscount, domesticForward);
        const auto foreign = JointBlock("eur_staged_oracle", pair.foreign_, basisKnots, foreignDiscount, foreignForward);
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const auto config = JointXccyConfig(pair);
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        const Dal::Vector_<> basisParameters(quoteCount, 0.0020);
        quoteMarket.SetBasisCurve(JointPwc("staged_oracle_basis_truth", pair.domestic_, basisKnots, basisParameters));

        input.spec_.today_ = today;
        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fixings_ = fixings;
        input.spec_.basisPair_ = pair;
        input.spec_.domesticCurveBlock_ = domestic;
        input.spec_.foreignCurveBlock_ = foreign;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.knotDates_ = basisKnots;
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        input.spec_.initialGuess_ = basisParameters.front();
        input.spec_.maxEvaluations_ = 500;
        input.spec_.maxRestarts_ = 40;
        for (const auto& maturity : instrumentMaturities)
            input.spec_.instruments_.push_back(XccyOracleInstrument(today, maturity, config, quoteMarket));
        input.options_.jacobianMode_ = mode;
        try {
            input.result_ = std::make_unique<Dal::CrossCurrencyCalibrationResult_>(Dal::CalibrateCrossCurrencyMarket(input.spec_, input.options_));
        } catch (const std::exception& exception) {
            THROW("Staged XCCY quote oracle baseline calibration failed: " + Dal::String_(exception.what()));
        }
        const Dal::String_ parameterBlock = Dal::String_("basis:xccy_basis_") + pair.domestic_.String();
        input.config_.calibrationId_ = "staged-xccy-oracle-" + Dal::String::FromInt(quoteCount);
        input.config_.componentKeyByParameterBlock_[parameterBlock] = "staged-oracle-basis";
        input.market_ = StagedXccyOracleMarket(input.spec_, *input.result_, input.config_);
        input.trade_ = XccyTrade(pair, today, horizon, "staged-xccy-oracle-trade");
        std::get<Dal::XccyTradeTerms_>(input.trade_.terms_).config_ = config;
        return input;
    }

    QuoteOracleObservation_ RecalibrateAndPriceStagedXccyQuoteBump(const StagedXccyQuoteOracleInput_& input, int quoteIndex, double bump) {
        std::unique_ptr<Dal::CrossCurrencyCalibrationResult_> calibrated;
        Dal::CrossCurrencyCalibrationSpec_ bumped;
        auto oracleOptions = input.options_;
        oracleOptions.jacobianMode_ = Dal::CurveJacobianMode_::Value_::BUMPED;
        const int steps = std::abs(bump) >= 1.0e-4 ? 10 : 1;
        for (int step = 1; step <= steps; ++step) {
            const auto* seed = calibrated ? calibrated.get() : input.result_.get();
            bumped = input.spec_;
            bumped.instruments_[quoteIndex] = BumpOracleXccy(input.spec_.instruments_[quoteIndex], bump * step / steps);
            const auto basis = seed->basisCurves_.at(input.spec_.basisPair_);
            const auto* solved = dynamic_cast<const Dal::Tape::DiscountPWC_<double>*>(basis.get());
            REQUIRE(solved, "Staged XCCY quote oracle expected a PWC basis curve");
            bumped.initialGuessPerNode_ = solved->FRight();
            // Keep the exact solver on the local solution branch; every continuation point still
            // recalibrates the bumped original quote and must independently pass the residual gate.
            for (int parameter = 0; parameter < static_cast<int>(bumped.initialGuessPerNode_.size()); ++parameter) {
                bumped.initialGuessPerNode_[parameter] +=
                    seed->diagnostics_.effJacobianInverse_(parameter, quoteIndex) * (bump / steps) / input.spec_.tolerance_;
                bumped.initialGuessPerNode_[parameter] += 1.0e-5;
            }
            try {
                calibrated = std::make_unique<Dal::CrossCurrencyCalibrationResult_>(Dal::CalibrateCrossCurrencyMarket(bumped, oracleOptions));
            } catch (const std::exception& exception) {
                THROW("Staged XCCY bumped calibration failed for quote " + Dal::String::FromInt(quoteIndex) + " and bump " +
                      Dal::String::FromDouble(bump) + " at continuation step " + Dal::String::FromInt(step) + ": " + Dal::String_(exception.what()));
            }
        }
        REQUIRE(calibrated->diagnostics_.maxAbsResidual_ <= input.spec_.fitTolerance_, "Staged XCCY quote oracle calibration failed its fit gate");
        return PriceQuoteOracleTrades({input.trade_}, StagedXccyOracleMarket(bumped, *calibrated, input.config_));
    }

    void AssertJointXccyQuoteRiskOracle(int quoteCount, Dal::CurveJacobianMode_ mode) {
        const auto input = MakeJointXccyQuoteOracleInput(quoteCount, mode);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        const auto aggregate = Dal::AggregateRatePortfolioQuoteRisk({input.trade_}, input.market_, {provenance});
        ASSERT_EQ(static_cast<int>(aggregate.buckets_.size()), quoteCount);
        const auto baseline = PriceQuoteOracleTrades({input.trade_}, input.market_);
        for (int i = 0; i < quoteCount; ++i) {
            const auto oracle = QuoteRiskOracleFromBumps(
                baseline, i, [&](int index, double bump) { return RecalibrateAndPriceJointXccyQuoteBump(input, index, bump); });
            const Dal::String_ context = Dal::String_("domain=JOINT_XCCY mode=") + mode.String() + " N=" + Dal::String::FromInt(quoteCount) +
                                         " quote=" + Dal::String::FromInt(i);
            AssertQuoteRiskBucket(aggregate.buckets_[i], oracle, QuoteOracleTolerance(quoteCount), context);
        }
    }

    void AssertStagedXccyQuoteRiskOracle(int quoteCount, Dal::CurveJacobianMode_ mode) {
        const auto input = MakeStagedXccyQuoteOracleInput(quoteCount, mode);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        const auto aggregate = Dal::AggregateRatePortfolioQuoteRisk({input.trade_}, input.market_, {provenance});
        ASSERT_EQ(static_cast<int>(aggregate.buckets_.size()), quoteCount);
        const auto baseline = PriceQuoteOracleTrades({input.trade_}, input.market_);
        for (int i = 0; i < quoteCount; ++i) {
            const auto oracle = QuoteRiskOracleFromBumps(
                baseline, i, [&](int index, double bump) { return RecalibrateAndPriceStagedXccyQuoteBump(input, index, bump); });
            const Dal::String_ context = Dal::String_("domain=STAGED_XCCY_BASIS mode=") + mode.String() + " N=" + Dal::String::FromInt(quoteCount) +
                                         " quote=" + Dal::String::FromInt(i);
            AssertQuoteRiskBucket(aggregate.buckets_[i], oracle, QuoteOracleTolerance(quoteCount), context);
        }
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

TEST(QuoteRiskAggregationTest, TestJointAndStagedAggregateKindsAreSupported) {
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

TEST(QuoteRiskAggregationTest, TestJointAndStagedXccyProduceTheirPublishedQuoteAxes) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        const auto joint = MakeJointInput(mode);
        const auto jointProvenance = Dal::BuildJointXccyQuoteRiskProvenance(joint.spec_, joint.result_, joint.options_, joint.market_, joint.config_);
        const auto jointTrade = XccyTrade(joint.spec_.pair_, joint.spec_.valuationTime_.Date(), joint.spec_.basis_.knotDates_.back());
        const auto jointResult = Dal::AggregateRatePortfolioQuoteRisk({jointTrade}, joint.market_, {jointProvenance});
        ASSERT_TRUE(jointResult.provenanceFailures_.empty());
        ASSERT_EQ(jointResult.buckets_.size(), jointProvenance.Axis().quotes_.size());
        ASSERT_EQ(jointResult.meta_.size(), 1U);
        ASSERT_TRUE(jointResult.meta_.front().eligible_);
        ASSERT_FALSE(jointResult.meta_.front().structuralZero_);

        const auto staged = MakeStagedInput(mode);
        const auto stagedProvenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(staged.spec_, *staged.result_, staged.options_, staged.market_, staged.config_);
        const auto stagedTrade = XccyTrade(staged.spec_.basisPair_, staged.spec_.valuationTime_.Date(), staged.spec_.knotDates_.back());
        const auto stagedResult = Dal::AggregateRatePortfolioQuoteRisk({stagedTrade}, staged.market_, {stagedProvenance});
        ASSERT_TRUE(stagedResult.provenanceFailures_.empty());
        ASSERT_EQ(stagedResult.buckets_.size(), stagedProvenance.Axis().quotes_.size());
        ASSERT_EQ(stagedResult.meta_.size(), 1U);
        ASSERT_TRUE(stagedResult.meta_.front().eligible_);
        ASSERT_FALSE(stagedResult.meta_.front().structuralZero_);
        for (const auto& bucket : stagedResult.buckets_)
            ASSERT_EQ(bucket.residualBlock_, stagedProvenance.Axis().residualRanges_.front().blockKey_);
    }
}

TEST(QuoteRiskAggregationTest, TestJointXccyConsumedBlockFailureDiscardsSiblingGradients) {
    const auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto trade = XccyTrade(input.spec_.pair_, input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back());

    for (int block = 0; block < static_cast<int>(provenance.Axis().parameterRanges_.size()); ++block) {
        const Dal::String_ component = provenance.ComponentKeyByParameterBlock().at(provenance.Axis().parameterRanges_[block].blockKey_);
        Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);
        const ScopedQuoteRiskForcedFailure_ forcedFailure(component);
        const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, input.market_, {provenance});

        ASSERT_TRUE(result.buckets_.empty());
        ASSERT_EQ(result.meta_.size(), 1U);
        ASSERT_FALSE(result.meta_.front().eligible_);
        ASSERT_EQ(result.meta_.front().reason_, "QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE");
        ASSERT_EQ(result.meta_.front().failingComponentKey_, component);
        ASSERT_EQ(result.meta_.front().originalNodeRiskReason_, "AAD_EVALUATION_FAILED");
        ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), block);
    }
}

TEST(QuoteRiskAggregationTest, TestJointXccyUnusedBlocksAreStructuralZerosWithoutSweeps) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto& firstRange = provenance.Axis().parameterRanges_.front();
    const auto& firstComponent = provenance.ComponentKeyByParameterBlock().at(firstRange.blockKey_);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto oneBlock = QuoteRiskDeposit(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), firstComponent, Dal::Ccy_("EUR"),
                                           "one-joint-block");
    const auto oneBlockResult = Dal::AggregateRatePortfolioQuoteRisk({oneBlock}, input.market_, {provenance});

    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 1);
    ASSERT_EQ(oneBlockResult.buckets_.size(), provenance.Axis().quotes_.size());
    ASSERT_EQ(oneBlockResult.meta_.size(), 1U);
    ASSERT_TRUE(oneBlockResult.meta_.front().eligible_);
    ASSERT_FALSE(oneBlockResult.meta_.front().structuralZero_);

    input.market_.curveComponents_["unrelated"] = input.market_.curveComponents_.at(firstComponent);
    const auto unrelated =
        QuoteRiskDeposit(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), "unrelated", Dal::Ccy_("EUR"), "no-joint-block");
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);
    const auto noBlockResult = Dal::AggregateRatePortfolioQuoteRisk({unrelated}, input.market_, {provenance});

    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
    ASSERT_EQ(noBlockResult.buckets_.size(), provenance.Axis().quotes_.size());
    ASSERT_EQ(noBlockResult.meta_.size(), 1U);
    ASSERT_TRUE(noBlockResult.meta_.front().eligible_);
    ASSERT_TRUE(noBlockResult.meta_.front().structuralZero_);
    for (const auto& bucket : noBlockResult.buckets_) {
        ASSERT_DOUBLE_EQ(bucket.dPvDDecimalQuote_, 0.0);
        ASSERT_DOUBLE_EQ(bucket.dv01_, 0.0);
    }
}

TEST(QuoteRiskAggregationTest, TestStagedXccyDomesticAndForeignStateMismatchPrecedesSweeps) {
    const auto baseline = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance =
        Dal::BuildStagedXccyBasisQuoteRiskProvenance(baseline.spec_, *baseline.result_, baseline.options_, baseline.market_, baseline.config_);
    auto changed = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, 0.001);
    const auto basis = baseline.result_->basisCurves_.at(baseline.spec_.basisPair_);
    const auto component = provenance.State().components_.front().componentKey_;
    changed.market_.curveComponents_[component] = basis;
    auto changedXccy = std::make_shared<Dal::CrossCurrencyMarket_>(changed.spec_.domesticCurveBlock_, changed.spec_.foreignCurveBlock_,
                                                                   baseline.spec_.fxSpot_, baseline.spec_.valuationTime_,
                                                                   baseline.spec_.collateralCurrency_, baseline.result_->market_.Fixings());
    changedXccy->SetBasisCurve(basis);
    changed.market_.xccyMarket_ = changedXccy;
    const auto trade = XccyTrade(baseline.spec_.basisPair_, baseline.spec_.valuationTime_.Date(), baseline.spec_.knotDates_.back());
    Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.store(0);
    Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.store(0);

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({trade}, changed.market_, {provenance});

    ASSERT_TRUE(result.buckets_.empty());
    ASSERT_TRUE(result.meta_.empty());
    ASSERT_EQ(result.provenanceFailures_.size(), 1U);
    ASSERT_EQ(result.provenanceFailures_.front().reason_, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH");
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load(), 0);
    ASSERT_EQ(Dal::RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load(), 0);
}

TEST(QuoteRiskAggregationTest, TestJointXccyKeepsActualPvCurrenciesSeparate) {
    const auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    const auto usd = XccyTrade(input.spec_.pair_, input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(), "usd-xccy");
    const auto& firstRange = provenance.Axis().parameterRanges_.front();
    const auto eur = QuoteRiskDeposit(input.spec_.valuationTime_.Date(), input.spec_.basis_.knotDates_.back(),
                                      provenance.ComponentKeyByParameterBlock().at(firstRange.blockKey_), Dal::Ccy_("EUR"), "eur-deposit");

    const auto result = Dal::AggregateRatePortfolioQuoteRisk({usd, eur}, input.market_, {provenance});

    ASSERT_EQ(result.pvByActualPvCcy_.size(), 2U);
    ASSERT_TRUE(result.pvByActualPvCcy_.count("USD"));
    ASSERT_TRUE(result.pvByActualPvCcy_.count("EUR"));
    ASSERT_EQ(result.buckets_.size(), 2U * provenance.Axis().quotes_.size());
    std::set<Dal::String_> currencies;
    for (const auto& bucket : result.buckets_)
        currencies.insert(bucket.actualPvCcy_.String());
    ASSERT_EQ(currencies, (std::set<Dal::String_>{"EUR", "USD"}));
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
            {
                SCOPED_TRACE("domain=JOINT_XCCY");
                try {
                    ASSERT_NO_FATAL_FAILURE(AssertJointXccyQuoteRiskOracle(quoteCount, mode));
                } catch (const std::exception& exception) {
                    FAIL() << "JOINT_XCCY oracle failed: " << exception.what();
                }
            }
            {
                SCOPED_TRACE("domain=STAGED_XCCY_BASIS");
                try {
                    ASSERT_NO_FATAL_FAILURE(AssertStagedXccyQuoteRiskOracle(quoteCount, mode));
                } catch (const std::exception& exception) {
                    FAIL() << "STAGED_XCCY_BASIS oracle failed: " << exception.what();
                }
            }
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
