//
// Created by GitHub Copilot on 2026/6/6.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccymarket.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> MakeFlatCurve(const String_& name,
                                          const String_& ccy,
                                          const Date_& today,
                                          double rate) {
        const Vector_<Date_> knots = {Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        const Vector_<> vals(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, vals, vals)));
    }

    Handle_<CurveBlock_> MakeBlock(const String_& name,
                                   const String_& ccy,
                                   const Date_& today,
                                   double rate) {
        return Handle_<CurveBlock_>(new CurveBlock_(MakeFlatCurve(name, ccy, today, rate)));
    }

    RateIndexConvention_ MakeIndex() {
        RateIndexConvention_ retval;
        retval.useProjectionCurve_ = true;
        retval.forecastTenor_ = PeriodLength_("12M");
        retval.dayBasis_ = DayBasis_("ACT_365F");
        retval.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return retval;
    }

    RateLegConvention_ MakeLeg() {
        RateLegConvention_ retval;
        retval.paymentFrequency_ = PeriodLength_("12M");
        retval.dayBasis_ = DayBasis_("ACT_365F");
        return retval;
    }

    CrossCurrencyMarket_ MakeMarket(const Date_& today, double basisRate = 0.0) {
        CrossCurrencyMarket_ retval(today);
        retval.SetCurveBlock(Ccy_("USD"), MakeBlock("usd", "USD", today, 0.02));
        retval.SetCurveBlock(Ccy_("EUR"), MakeBlock("eur", "EUR", today, 0.01));
        retval.SetFxSpot(CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 1.10);
        if (basisRate != 0.0)
            retval.SetBasisCurve(CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), MakeFlatCurve("basis", "USD", today, basisRate));
        return retval;
    }

    CrossCurrencySwap_ MakeSwap(const Date_& today, double quotedSpread = 0.0) {
        CrossCurrencyConvention_ convention;
        convention.initialNotionalExchange_ = true;
        convention.finalNotionalExchange_ = true;
        convention.spreadOnForeignLeg_ = true;
        return CrossCurrencySwap_(today,
                                  today,
                                  Date::AddMonths(today, 12),
                                  quotedSpread,
                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                  110.0,
                                  100.0,
                                  MakeIndex(),
                                  MakeLeg(),
                                  MakeIndex(),
                                  MakeLeg(),
                                  convention);
    }
} // namespace

TEST(XccyMarketTest, TestFxForwardParityUsesDiscountCurves) {
    const Date_ today(2024, 1, 15);
    const Date_ maturity = Date::AddMonths(today, 12);
    const auto market = MakeMarket(today);
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

    const double expected = 1.10
                            * market.ForeignDiscountCurve(pair, CollateralType_(CollateralType_::Value_::OIS))(today, maturity)
                            / market.DomesticDiscountCurve(pair, CollateralType_(CollateralType_::Value_::OIS))(today, maturity);
    ASSERT_NEAR(market.FxForward(pair, maturity), expected, 1e-10);
}

TEST(XccyMarketTest, TestCurveRoutingUsesDomesticAndForeignCurrencies) {
    const Date_ today(2024, 1, 15);
    const auto market = MakeMarket(today);
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
    const Date_ maturity = Date::AddMonths(today, 12);

    const double domesticDf = market.DomesticDiscountCurve(pair, CollateralType_(CollateralType_::Value_::OIS))(today, maturity);
    const double foreignDf = market.ForeignDiscountCurve(pair, CollateralType_(CollateralType_::Value_::OIS))(today, maturity);

    ASSERT_NEAR(domesticDf, (*MakeFlatCurve("checkUsd", "USD", today, 0.02))(today, maturity), 1e-10);
    ASSERT_NEAR(foreignDf, (*MakeFlatCurve("checkEur", "EUR", today, 0.01))(today, maturity), 1e-10);
}

TEST(XccyMarketTest, TestCrossCurrencySwapParSpreadOnFlatCurves) {
    const Date_ today(2024, 1, 15);
    const auto market = MakeMarket(today);
    const auto swap = MakeSwap(today);
    const auto rate = swap.Precompute();

    const double parSpread = (*rate)(market);
    const auto quotedSwap = MakeSwap(today, parSpread);
    const auto quotedRate = quotedSwap.Precompute();

    ASSERT_NEAR((*quotedRate)(market), parSpread, 1e-10);
}

TEST(XccyMarketTest, TestCrossCurrencyCalibrationRepricesInputQuote) {
    const Date_ today(2024, 1, 15);
    const auto trueMarket = MakeMarket(today, 0.0020);
    const auto prototype = MakeSwap(today);
    const double marketQuote = (*prototype.Precompute())(trueMarket);

    CrossCurrencyCalibrationSpec_ spec;
    spec.market_ = MakeMarket(today);
    spec.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, marketQuote)))};
    spec.knotDates_ = {Date::AddMonths(today, 12)};

    const CrossCurrencyCalibrationResult_ result = CalibrateCrossCurrencyMarket(spec);
    const double calibratedQuote = (*prototype.Precompute())(result.market_);

    ASSERT_NEAR(calibratedQuote, marketQuote, 1e-8);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
}

TEST(XccyMarketTest, TestResettableConventionThrows) {
    const Date_ today(2024, 1, 15);
    CrossCurrencyConvention_ convention;
    convention.resettableNotional_ = true;

    const CrossCurrencySwap_ swap(today,
                                  today,
                                  Date::AddMonths(today, 12),
                                  0.0,
                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                  110.0,
                                  100.0,
                                  MakeIndex(),
                                  MakeLeg(),
                                  MakeIndex(),
                                  MakeLeg(),
                                  convention);

    ASSERT_THROW(swap.Precompute(), Dal::Exception_);
}
