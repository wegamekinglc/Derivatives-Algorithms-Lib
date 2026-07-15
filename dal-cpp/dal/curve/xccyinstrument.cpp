//
// Created by GitHub Copilot on 2026/6/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
#include <dal/auto/MG_XccyNotionalMode_enum.inc>

    namespace {
        class CrossCurrencySwapKernelRate_ : public CrossCurrencySwap_::Rate_ {
            XccyCashflowPlan_ plan_;

        public:
            explicit CrossCurrencySwapKernelRate_(const XccyCashflowPlan_& plan) : plan_(plan) {}

            double operator()(const CrossCurrencyMarket_& market) const override {
                const auto& convention = plan_.config_.convention_;
                Tape::JointCurveBlock_<double> domestic;
                Tape::JointCurveBlock_<double> foreign;
                domestic.discountCurves.emplace(convention.domesticIndex_.collateral_,
                                                &market.DomesticDiscountCurve(convention.domesticIndex_.collateral_));
                foreign.discountCurves.emplace(convention.foreignIndex_.collateral_,
                                               &market.ForeignDiscountCurve(convention.foreignIndex_.collateral_));
                if (convention.domesticIndex_.useProjectionCurve_) {
                    domestic.forwardCurves.emplace(
                        convention.domesticIndex_.forecastTenor_,
                        &market.DomesticForwardCurve(convention.domesticIndex_.forecastTenor_, convention.domesticIndex_.collateral_));
                }
                if (convention.foreignIndex_.useProjectionCurve_) {
                    foreign.forwardCurves.emplace(
                        convention.foreignIndex_.forecastTenor_,
                        &market.ForeignForwardCurve(convention.foreignIndex_.forecastTenor_, convention.foreignIndex_.collateral_));
                }

                XccyMarketView_<double> view;
                view.valuationTime_ = market.ValuationTime();
                view.pair_ = CurrencyPair_(market.DomesticCcy(), market.ForeignCcy());
                view.collateralCurrency_ = market.CollateralCurrency();
                view.fxSpot_ = market.FxSpot();
                view.domestic_ = &domestic;
                view.foreign_ = &foreign;
                view.basis_ = market.BasisCurve();
                if (market.Fixings())
                    return PriceXccyParSpread<double>(plan_, view, *market.Fixings());

                static const MarketFixingSnapshot_ emptyFixings;
                if (plan_.config_.notionalMode_ == XccyNotionalMode_::Value_::FIXED && plan_.config_.domesticRateFixing_.indexName_.empty() &&
                    plan_.config_.foreignRateFixing_.indexName_.empty())
                    return PriceXccyParSpread<double>(plan_, view, emptyFixings);

                const auto requests = RequiredHistoricalFixings(plan_, view.valuationTime_);
                const auto fixings = SnapshotGlobalFixings(requests);
                return PriceXccyParSpread<double>(plan_, view, *fixings);
            }
        };

        CrossCurrencySwapConfig_
        FixedConfig(const CurrencyPair_& pair, double domesticNotional, double foreignNotional, const CrossCurrencyConvention_& convention) {
            CrossCurrencySwapConfig_ result;
            result.pair_ = pair;
            result.domesticNotional_ = domesticNotional;
            result.foreignNotional_ = foreignNotional;
            result.convention_ = convention;
            result.notionalMode_ = XccyNotionalMode_::Value_::FIXED;
            return result;
        }

        void ValidateResetConfig(const CrossCurrencySwapConfig_& config) {
            if (config.notionalMode_ == XccyNotionalMode_::Value_::FIXED)
                return;
            REQUIRE(config.fxReset_.fixingLag_ >= 0, "Resettable cross-currency notionals require an explicit non-negative FX fixing lag");
            REQUIRE(config.fxReset_.fixingHour_ >= 0 && config.fxReset_.fixingHour_ < 24 && config.fxReset_.fixingMinute_ >= 0 &&
                        config.fxReset_.fixingMinute_ < 60,
                    "Resettable cross-currency notionals require an explicit valid FX fixing time");
        }

        void
        ValidateConfig(const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const CrossCurrencySwapConfig_& config) {
            REQUIRE(tradeDate.IsValid() && start.IsValid() && maturity.IsValid(),
                    "CrossCurrencySwap_ requires valid trade, start, and maturity dates");
            REQUIRE(maturity > start, "CrossCurrencySwap_ requires maturity after start");
            REQUIRE(std::isfinite(marketRate), "CrossCurrencySwap_ requires a finite market rate");
            REQUIRE(std::isfinite(config.domesticNotional_) && config.domesticNotional_ > 0.0 && std::isfinite(config.foreignNotional_) &&
                        config.foreignNotional_ > 0.0,
                    "CrossCurrencySwap_ requires positive finite notionals");
            REQUIRE(!(config.pair_.domestic_ == config.pair_.foreign_), "CrossCurrencySwap_ requires distinct domestic and foreign currencies");
            REQUIRE(config.notionalMode_ == XccyNotionalMode_::Value_::FIXED || config.notionalMode_ == XccyNotionalMode_::Value_::RESETTABLE ||
                        config.notionalMode_ == XccyNotionalMode_::Value_::MARK_TO_MARKET,
                    "CrossCurrencySwap_ requires a valid notional mode");
            ValidateResetConfig(config);
        }
    } // namespace

    CurrencyPair_::CurrencyPair_() : domestic_(Ccy_::Value_::USD), foreign_(Ccy_::Value_::EUR) {}

    CurrencyPair_::CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign) : domestic_(domestic), foreign_(foreign) {
        REQUIRE(!(domestic_ == foreign_), "CurrencyPair_ requires two distinct currencies");
    }

    CurrencyPair_ CurrencyPair_::Reversed() const { return CurrencyPair_(foreign_, domestic_); }

    bool CurrencyPair_::operator<(const CurrencyPair_& rhs) const {
        if (domestic_ < rhs.domestic_ || rhs.domestic_ < domestic_)
            return domestic_ < rhs.domestic_;
        return foreign_ < rhs.foreign_;
    }

    bool CurrencyPair_::operator==(const CurrencyPair_& rhs) const { return domestic_ == rhs.domestic_ && foreign_ == rhs.foreign_; }

    CrossCurrencySwap_::CrossCurrencySwap_(const Date_& tradeDate,
                                           const Date_& start,
                                           const Date_& maturity,
                                           double marketRate,
                                           const CurrencyPair_& pair,
                                           double domesticNotional,
                                           double foreignNotional,
                                           const CrossCurrencyConvention_& convention)
        : CrossCurrencySwap_(tradeDate, start, maturity, marketRate, FixedConfig(pair, domesticNotional, foreignNotional, convention)) {}

    CrossCurrencySwap_::CrossCurrencySwap_(
        const Date_& tradeDate, const Date_& start, const Date_& maturity, double marketRate, const CrossCurrencySwapConfig_& config)
        : tradeDate_(tradeDate), start_(start), maturity_(maturity), marketRate_(marketRate), config_(config) {
        ValidateConfig(tradeDate_, start_, maturity_, marketRate_, config_);
    }

    String_ CrossCurrencySwap_::Name() const { return "CrossCurrencySwap"; }

    pair<Date_, Date_> CrossCurrencySwap_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<CrossCurrencySwap_::Rate_> CrossCurrencySwap_::Precompute() const {
        const auto plan = BuildXccyCashflowPlan(start_, maturity_, config_);
        return Handle_<Rate_>(new CrossCurrencySwapKernelRate_(plan));
    }
} // namespace Dal
