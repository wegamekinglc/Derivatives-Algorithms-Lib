//
// Created by Codex on 2026/7/15.
//

#include <dal/platform/platform.hpp>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/platform/initall.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    const CollateralType_ OIS(CollateralType_::Value_::OIS);
    const PeriodLength_ THREE_MONTHS("3M");
    const Date_ today(2025, 1, 16);
    const Vector_<Date_> knots = {
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 18),
        Date::AddMonths(today, 36),
    };

    struct ModeResult_ {
        const char* label_;
        int periodCount_ = 0;
        int resetCount_ = 0;
        int mtmDeltaCount_ = 0;
        double nextDomesticNotional_ = 0.0;
        double parQuote_ = 0.0;
    };

    Handle_<DiscountCurve_> Pwc(const String_& name, const Ccy_& ccy, const Vector_<>& values) {
        return Handle_<DiscountCurve_>(NewDiscountPWC(name, ccy.String(), PiecewiseConstant_(knots, values)));
    }

    Handle_<CurveBlock_> Block(const String_& name, const Ccy_& ccy, const Vector_<>& oisValues, const Vector_<>& threeMonthValues) {
        return Handle_<CurveBlock_>(new CurveBlock_(name, ccy.String(), {{OIS, Pwc(name + "_ois", ccy, oisValues)}},
                                                    {{THREE_MONTHS, Pwc(name + "_3m", ccy, threeMonthValues)}}, DayBasis_("ACT_365F")));
    }

    struct MarketFixture_ {
        Handle_<CurveBlock_> usd_ = Block("xccy_reset_usd", Ccy_("USD"), {0.015, 0.022, 0.030}, {0.025, 0.035, 0.050});
        Handle_<CurveBlock_> eur_ = Block("xccy_reset_eur", Ccy_("EUR"), {0.008, 0.012, 0.018}, {0.018, 0.025, 0.032});
        Handle_<DiscountCurve_> basis_ = Pwc("xccy_reset_usd_basis", Ccy_("USD"), {0.001, 0.003, 0.006});

        CrossCurrencyMarket_ Market(const DateTime_& valuationTime, const Handle_<MarketFixingSnapshot_>& fixings) const {
            CrossCurrencyMarket_ result(usd_, eur_, 1.10, valuationTime, Ccy_("USD"), fixings);
            result.SetBasisCurve(basis_);
            return result;
        }
    };

    RateIndexConvention_ IndexConvention() {
        RateIndexConvention_ result;
        result.useProjectionCurve_ = true;
        result.forecastTenor_ = THREE_MONTHS;
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        result.collateral_ = OIS;
        return result;
    }

    RateLegConvention_ LegConvention() {
        RateLegConvention_ result;
        result.paymentFrequency_ = THREE_MONTHS;
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.paymentConvention_ = BizDayConvention_("Unadjusted");
        result.accrualHolidays_ = Holidays::None();
        result.paymentHolidays_ = Holidays::None();
        return result;
    }

    CrossCurrencySwapConfig_ Config(XccyNotionalMode_ mode) {
        CrossCurrencySwapConfig_ result;
        result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.notionalMode_ = mode;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = IndexConvention();
        result.convention_.foreignIndex_ = IndexConvention();
        result.convention_.domesticLeg_ = LegConvention();
        result.convention_.foreignLeg_ = LegConvention();
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingConvention_ = BizDayConvention_("Unadjusted");
        result.fxReset_.fixingHour_ = 10;
        result.fxReset_.fixingMinute_ = 30;
        result.domesticRateFixing_ = {"USD-XCCY-RESET-3M", 11, 0};
        result.foreignRateFixing_ = {"EUR-XCCY-RESET-3M", 11, 0};
        return result;
    }

    const char* ModeLabel(XccyNotionalMode_ mode) {
        if (mode == XccyNotionalMode_::Value_::FIXED)
            return "FIXED";
        if (mode == XccyNotionalMode_::Value_::RESETTABLE)
            return "RESETTABLE";
        if (mode == XccyNotionalMode_::Value_::MARK_TO_MARKET)
            return "MARK_TO_MARKET";
        THROW("Unknown XCCY notional mode");
    }

    ModeResult_ EvaluateFutureMode(const MarketFixture_& fixture, XccyNotionalMode_ mode) {
        const DateTime_ valuationTime(today, 9, 0);
        const Date_ start = Date::AddMonths(today, 3);
        const Date_ maturity = Date::AddMonths(start, 24);
        const CrossCurrencySwapConfig_ config = Config(mode);
        const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(start, maturity, config);
        const CrossCurrencySwap_ swap(start, start, maturity, 0.0, config);
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
        const CrossCurrencyMarket_ market = fixture.Market(valuationTime, fixings);

        const auto& convention = plan.config_.convention_;
        Tape::JointCurveBlock_<double> domestic;
        Tape::JointCurveBlock_<double> foreign;
        domestic.discountCurves.emplace(convention.domesticIndex_.collateral_, &market.DomesticDiscountCurve(convention.domesticIndex_.collateral_));
        foreign.discountCurves.emplace(convention.foreignIndex_.collateral_, &market.ForeignDiscountCurve(convention.foreignIndex_.collateral_));
        if (convention.domesticIndex_.useProjectionCurve_) {
            domestic.forwardCurves.emplace(
                convention.domesticIndex_.forecastTenor_,
                &market.DomesticForwardCurve(convention.domesticIndex_.forecastTenor_, convention.domesticIndex_.collateral_));
        }
        if (convention.foreignIndex_.useProjectionCurve_) {
            foreign.forwardCurves.emplace(convention.foreignIndex_.forecastTenor_,
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

        const XccyResolvedNotionals_<double> notionals = ResolveXccyNotionals<double>(plan, view, *fixings);
        ModeResult_ result;
        result.label_ = ModeLabel(mode);
        result.periodCount_ = static_cast<int>(plan.domesticPeriods_.size());
        result.resetCount_ = static_cast<int>(plan.resets_.size());
        result.mtmDeltaCount_ = static_cast<int>(notionals.mtmDeltas_.size());
        REQUIRE(notionals.domesticNotionals_.size() > 1, "Future XCCY example requires at least two domestic periods");
        result.nextDomesticNotional_ = notionals.domesticNotionals_[1];
        result.parQuote_ = (*swap.Precompute())(market);
        const double directQuote = PriceXccyParSpread<double>(plan, view, *fixings);
        REQUIRE(std::fabs(result.parQuote_ - directQuote) < 1.0e-14, "Precomputed and direct XCCY par quotes must agree");
        return result;
    }

    void PrintMode(const ModeResult_& result) {
        std::cout << result.label_ << " periods=" << result.periodCount_ << " resets=" << result.resetCount_
                  << " mtm_deltas=" << result.mtmDeltaCount_ << " next_domestic_notional=" << result.nextDomesticNotional_
                  << " par_quote_bp=" << 1.0e4 * result.parQuote_ << '\n';
    }

    Handle_<MarketFixingSnapshot_> SnapshotFor(const XccyCashflowPlan_& plan, const DateTime_& valuationTime) {
        const CrossCurrencySwapConfig_& config = plan.config_;
        const String_ fxIndex = FxIndexName(config.pair_);
        const Vector_<FixingRequest_> requests = RequiredHistoricalFixings(plan, valuationTime);
        REQUIRE(!requests.empty(), "Started MTM example requires historical fixing requests");

        bool hasDomestic = false;
        bool hasForeign = false;
        bool hasFx = false;
        MarketFixingSnapshot_::values_t values;
        for (const auto& request : requests) {
            std::cout << "historical_fixing index=" << request.indexName_ << " time=" << DateTime::ToString(request.fixingTime_) << '\n';
            if (request.indexName_ == config.domesticRateFixing_.indexName_) {
                values["USD-XCCY-RESET-3M"][request.fixingTime_] = 0.040;
                hasDomestic = true;
            } else if (request.indexName_ == config.foreignRateFixing_.indexName_) {
                values["EUR-XCCY-RESET-3M"][request.fixingTime_] = 0.030;
                hasForeign = true;
            } else if (request.indexName_ == fxIndex) {
                values[fxIndex][request.fixingTime_] = 1.20;
                hasFx = true;
            } else {
                THROW("Unknown historical XCCY fixing identity: " + request.indexName_);
            }
        }
        REQUIRE(hasDomestic && hasForeign && hasFx, "Started MTM example requires USD, EUR, and FX historical fixing requests");
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values));
    }

    void RunStartedMtm(const MarketFixture_& fixture) {
        const DateTime_ valuationTime(today, 12, 0);
        const Date_ start = Date::AddMonths(today, -3);
        const Date_ maturity = Date::AddMonths(start, 24);
        const CrossCurrencySwapConfig_ config = Config(XccyNotionalMode_::Value_::MARK_TO_MARKET);
        const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(start, maturity, config);
        const Handle_<MarketFixingSnapshot_> fixings = SnapshotFor(plan, valuationTime);
        const CrossCurrencyMarket_ market = fixture.Market(valuationTime, fixings);
        REQUIRE(market.Fixings().get() == fixings.get(), "Started MTM market must retain its immutable fixing snapshot");
        const CrossCurrencySwap_ swap(start, start, maturity, 0.0, config);
        const double parQuote = (*swap.Precompute())(market);
        REQUIRE(std::isfinite(parQuote), "Started MTM par quote must be finite");
        std::cout << "STARTED_MARK_TO_MARKET par_quote_bp=" << 1.0e4 * parQuote << '\n';
    }

    bool RunExample() {
        std::cout << std::fixed << std::setprecision(8);
        const MarketFixture_ fixture;
        const ModeResult_ fixed = EvaluateFutureMode(fixture, XccyNotionalMode_::Value_::FIXED);
        const ModeResult_ resettable = EvaluateFutureMode(fixture, XccyNotionalMode_::Value_::RESETTABLE);
        const ModeResult_ mtm = EvaluateFutureMode(fixture, XccyNotionalMode_::Value_::MARK_TO_MARKET);
        PrintMode(fixed);
        PrintMode(resettable);
        PrintMode(mtm);

        REQUIRE(fixed.resetCount_ == 0 && fixed.mtmDeltaCount_ == 0, "FIXED mode must have zero resets and zero MTM deltas");
        REQUIRE(resettable.resetCount_ == resettable.periodCount_ - 1 && resettable.mtmDeltaCount_ == 0,
                "RESETTABLE mode must reset after every first period and have zero MTM deltas");
        REQUIRE(mtm.resetCount_ == mtm.periodCount_ - 1 && mtm.mtmDeltaCount_ == mtm.periodCount_ - 1,
                "MARK_TO_MARKET mode must reset and exchange a notional delta after every first period");
        REQUIRE(std::fabs(fixed.parQuote_ - resettable.parQuote_) > 1.0e-12, "FIXED and RESETTABLE par quotes must differ");
        REQUIRE(std::fabs(fixed.parQuote_ - mtm.parQuote_) > 1.0e-12, "FIXED and MARK_TO_MARKET par quotes must differ");
        REQUIRE(std::fabs(resettable.parQuote_ - mtm.parQuote_) > 1.0e-12, "RESETTABLE and MARK_TO_MARKET par quotes must differ");
        REQUIRE(std::isfinite(fixed.parQuote_) && std::isfinite(resettable.parQuote_) && std::isfinite(mtm.parQuote_),
                "Future XCCY par quotes must be finite");
        REQUIRE(std::isfinite(fixed.nextDomesticNotional_) && std::isfinite(resettable.nextDomesticNotional_) &&
                    std::isfinite(mtm.nextDomesticNotional_),
                "Future XCCY next notionals must be finite");

        RunStartedMtm(fixture);
        return true;
    }
} // namespace

int main() {
    try {
        RegisterAll_::Init();
        return RunExample() ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
