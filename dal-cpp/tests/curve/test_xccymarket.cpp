//
// Created by GitHub Copilot on 2026/6/6.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/xccymarket.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>

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
        auto domesticBlock = MakeBlock("usd", "USD", today, 0.02);
        auto foreignBlock = MakeBlock("eur", "EUR", today, 0.01);
        CrossCurrencyMarket_ retval(domesticBlock, foreignBlock, 1.10);
        if (basisRate != 0.0) {
            retval.SetBasisCurve(MakeFlatCurve("basis", "USD", today, basisRate));
        }
        return retval;
    }

    CrossCurrencyCalibrationSpec_ MakeCalibrationSpec(const Date_& today, const CurrencyPair_& pair) {
        CrossCurrencyCalibrationSpec_ retval;
        retval.basisPair_ = pair;
        retval.domesticCurveBlock_ = MakeBlock("usd", "USD", today, 0.02);
        retval.foreignCurveBlock_ = MakeBlock("eur", "EUR", today, 0.01);
        retval.fxSpot_ = 1.10;
        return retval;
    }

    CrossCurrencyConvention_ MakeConvention() {
        CrossCurrencyConvention_ retval;
        retval.initialNotionalExchange_ = true;
        retval.finalNotionalExchange_ = true;
        retval.spreadOnForeignLeg_ = true;
        retval.domesticIndex_ = MakeIndex();
        retval.domesticLeg_ = MakeLeg();
        retval.foreignIndex_ = MakeIndex();
        retval.foreignLeg_ = MakeLeg();
        return retval;
    }

    CrossCurrencySwap_ MakeSwap(const Date_& today, double quotedSpread = 0.0) {
        return CrossCurrencySwap_(today,
                                  today,
                                  Date::AddMonths(today, 12),
                                  quotedSpread,
                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                  110.0,
                                  100.0,
                                  MakeConvention());
    }
} // namespace

TEST(XccyMarketTest, TestFxForwardParityUsesDiscountCurves) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const Date_ maturity = Date::AddMonths(today, 12);
    const auto market = MakeMarket(today);

    const double expected = 1.10
                            * market.ForeignDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity)
                            / market.DomesticDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity);
    ASSERT_NEAR(market.FxForward(maturity), expected, 1e-10);
    ASSERT_NEAR(market.FxForward(today, maturity, CollateralType_(CollateralType_::Value_::OIS)), expected, 1e-10);
}

TEST(XccyMarketTest, TestCurveRoutingUsesDomesticAndForeignCurrencies) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);
    const Date_ maturity = Date::AddMonths(today, 12);

    const double domesticDf = market.DomesticDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity);
    const double foreignDf = market.ForeignDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity);

    ASSERT_NEAR(domesticDf, (*MakeFlatCurve("checkUsd", "USD", today, 0.02))(today, maturity), 1e-10);
    ASSERT_NEAR(foreignDf, (*MakeFlatCurve("checkEur", "EUR", today, 0.01))(today, maturity), 1e-10);
}

TEST(XccyMarketTest, TestConstructorAcceptsBlocksAndSpot) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    auto domesticBlock = MakeBlock("usd", "USD", today, 0.03);
    auto foreignBlock = MakeBlock("eur", "EUR", today, 0.02);

    CrossCurrencyMarket_ market(domesticBlock, foreignBlock, 1.15);

    ASSERT_EQ(market.DomesticCcy(), Ccy_("USD"));
    ASSERT_EQ(market.ForeignCcy(), Ccy_("EUR"));
    ASSERT_NEAR(market.FxSpot(), 1.15, 1e-10);
}

TEST(XccyMarketTest, TestConstructorRejectsNullDomesticBlock) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    Handle_<CurveBlock_> nullBlock;
    auto foreignBlock = MakeBlock("eur", "EUR", today, 0.02);

    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(nullBlock, foreignBlock, 1.10)), Dal::Exception_);
}

TEST(XccyMarketTest, TestConstructorRejectsNullForeignBlock) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    auto domesticBlock = MakeBlock("usd", "USD", today, 0.03);
    Handle_<CurveBlock_> nullBlock;

    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(domesticBlock, nullBlock, 1.10)), Dal::Exception_);
}

TEST(XccyMarketTest, TestConstructorRejectsNonPositiveFxSpot) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    auto domesticBlock = MakeBlock("usd", "USD", today, 0.03);
    auto foreignBlock = MakeBlock("eur", "EUR", today, 0.02);

    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(domesticBlock, foreignBlock, 0.0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(domesticBlock, foreignBlock, -1.0)), Dal::Exception_);
}

TEST(XccyMarketTest, TestConstructorRejectsSameCurrencies) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    auto block1 = MakeBlock("usd1", "USD", today, 0.03);
    auto block2 = MakeBlock("usd2", "USD", today, 0.02);

    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(block1, block2, 1.10)), Dal::Exception_);
}

TEST(XccyMarketTest, TestCrossCurrencySwapParSpreadIndependentOfQuotedRate) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);

    const double parSpreadZeroQuote = (*MakeSwap(today, 0.0).Precompute())(market);
    const double parSpreadLargeQuote = (*MakeSwap(today, 0.0250).Precompute())(market);
    const double parSpreadNegativeQuote = (*MakeSwap(today, -0.0100).Precompute())(market);

    // The model-implied par spread is a property of the curves and conventions; it must
    // not depend on the swap's quoted MarketRate.
    ASSERT_NEAR(parSpreadLargeQuote, parSpreadZeroQuote, 1e-10);
    ASSERT_NEAR(parSpreadNegativeQuote, parSpreadZeroQuote, 1e-10);
}

TEST(XccyMarketTest, TestSwapPricingRejectsMismatchedPair) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today); // USD/EUR orientation

    CrossCurrencyConvention_ convention = MakeConvention();

    const CrossCurrencySwap_ mismatchedSwap(today,
                                            today,
                                            Date::AddMonths(today, 12),
                                            0.0,
                                            CurrencyPair_(Ccy_("USD"), Ccy_("GBP")),
                                            110.0,
                                            100.0,
                                            convention);
    const auto rate = mismatchedSwap.Precompute();
    ASSERT_THROW(static_cast<void>((*rate)(market)), Dal::Exception_);
}

TEST(XccyMarketTest, TestParSpreadAnchoredToEvaluationDateNotTradeDate) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);

    const Date_ start = today;
    const Date_ maturity = Date::AddMonths(today, 12);

    CrossCurrencyConvention_ convention = MakeConvention();

    auto makeSwap = [&](const Date_& tradeDate) {
        return CrossCurrencySwap_(tradeDate,
                                  start,
                                  maturity,
                                  0.0,
                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                  110.0,
                                  100.0,
                                  convention);
    };

    // Pricing must value all cashflows as of the market evaluation date, so the
    // model par spread is independent of the historical trade date.
    const double spreadTradedToday = (*makeSwap(today).Precompute())(market);
    const double spreadTradedEarlier = (*makeSwap(today.AddDays(-45)).Precompute())(market);

    ASSERT_NEAR(spreadTradedToday, spreadTradedEarlier, 1e-10);
}

TEST(XccyMarketTest, TestCrossCurrencyCalibrationRepricesInputQuote) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto trueMarket = MakeMarket(today, 0.0020);
    const auto prototype = MakeSwap(today);
    const double marketQuote = (*prototype.Precompute())(trueMarket);

    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")));
    spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, marketQuote)))};
    spec.knotDates_ = {Date::AddMonths(today, 12)};

    const CrossCurrencyCalibrationResult_ result = CalibrateCrossCurrencyMarket(spec);
    const double calibratedQuote = (*prototype.Precompute())(result.market_);

    ASSERT_NEAR(calibratedQuote, marketQuote, 1e-8);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
    ASSERT_EQ(spec.knotDates_, result.fxForwardCurve_.dates_);
    ASSERT_NEAR(result.fxForwardCurve_.forwards_.front(), result.market_.FxForward(spec.knotDates_.front()), 1e-10);
}

TEST(XccyMarketTest, TestResettableConventionThrows) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    CrossCurrencyConvention_ convention;
    convention.resettableNotional_ = true;

    const CrossCurrencySwap_ swap(today,
                                  today,
                                  Date::AddMonths(today, 12),
                                  0.0,
                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                  110.0,
                                  100.0,
                                  convention);

    ASSERT_THROW(static_cast<void>(swap.Precompute()), Dal::Exception_);
}

TEST(XccyMarketTest, TestCalibrationWithSingleKnotAndZeroMarketRate) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    CrossCurrencyConvention_ convention;
    convention.initialNotionalExchange_ = true;
    convention.finalNotionalExchange_ = true;
    convention.spreadOnForeignLeg_ = true;

    RateIndexConvention_ indexConv;
    indexConv.useProjectionCurve_ = true;
    indexConv.forecastTenor_ = PeriodLength_("12M");
    indexConv.dayBasis_ = DayBasis_("ACT_365F");
    indexConv.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    RateLegConvention_ legConv;
    legConv.paymentFrequency_ = PeriodLength_("12M");
    legConv.dayBasis_ = DayBasis_("ACT_365F");

    convention.domesticIndex_ = indexConv;
    convention.domesticLeg_ = legConv;
    convention.foreignIndex_ = indexConv;
    convention.foreignLeg_ = legConv;

    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

    Vector_<Handle_<CrossCurrencySwap_>> instruments;

    const Date_ maturity = Date::AddMonths(today, 12);
    Handle_<CrossCurrencySwap_> swap(new CrossCurrencySwap_(today,
                                                            today,
                                                            maturity,
                                                            0.0,
                                                            pair,
                                                            110.0,
                                                            100.0,
                                                            convention));
    instruments.push_back(swap);

    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
    spec.instruments_ = instruments;
    spec.knotDates_ = {maturity};
    spec.maxEvaluations_ = 100;
    spec.tolerance_ = 1e-10;

    const auto result = CalibrateCrossCurrencyMarket(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
}

// The underdetermined solver handles both exact-determined and underdetermined systems.
// Tests below verify single-instrument (flat), multi-instrument (term structure), and
// approximate-solve modes.

TEST(XccyMarketTest, TestCalibrationConvergesForLargerSpread) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    CrossCurrencyConvention_ convention;
    convention.initialNotionalExchange_ = true;
    convention.finalNotionalExchange_ = true;
    convention.spreadOnForeignLeg_ = true;

    RateIndexConvention_ indexConv;
    indexConv.useProjectionCurve_ = true;
    indexConv.forecastTenor_ = PeriodLength_("12M");
    indexConv.dayBasis_ = DayBasis_("ACT_365F");
    indexConv.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    RateLegConvention_ legConv;
    legConv.paymentFrequency_ = PeriodLength_("12M");
    legConv.dayBasis_ = DayBasis_("ACT_365F");

    convention.domesticIndex_ = indexConv;
    convention.domesticLeg_ = legConv;
    convention.foreignIndex_ = indexConv;
    convention.foreignLeg_ = legConv;

    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
    const Date_ maturity = Date::AddMonths(today, 12);

    Vector_<Handle_<CrossCurrencySwap_>> instruments;
    instruments.push_back(Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today,
                                                                             today,
                                                                             maturity,
                                                                             0.0050,
                                                                             pair,
                                                                             110.0,
                                                                             100.0,
                                                                             convention)));

    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
    spec.instruments_ = instruments;
    spec.knotDates_ = {maturity};
    spec.maxEvaluations_ = 200;
    spec.tolerance_ = 1e-10;

    const auto result = CalibrateCrossCurrencyMarket(spec);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, spec.tolerance_);
}

TEST(XccyMarketTest, TestCalibrationWithMultipleInstrumentsTermStructure) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);

    CrossCurrencyConvention_ convention;
    convention.initialNotionalExchange_ = true;
    convention.finalNotionalExchange_ = true;
    convention.spreadOnForeignLeg_ = true;

    RateIndexConvention_ indexConv;
    indexConv.useProjectionCurve_ = true;
    indexConv.forecastTenor_ = PeriodLength_("12M");
    indexConv.dayBasis_ = DayBasis_("ACT_365F");
    indexConv.collateral_ = CollateralType_(CollateralType_::Value_::OIS);

    RateLegConvention_ legConv;
    legConv.paymentFrequency_ = PeriodLength_("12M");
    legConv.dayBasis_ = DayBasis_("ACT_365F");

    convention.domesticIndex_ = indexConv;
    convention.domesticLeg_ = legConv;
    convention.foreignIndex_ = indexConv;
    convention.foreignLeg_ = legConv;

    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

    // Build a true market with a term structure of basis spreads
    const Date_ maturity1Y = Date::AddMonths(today, 12);
    const Date_ maturity2Y = Date::AddMonths(today, 24);
    const Date_ maturity3Y = Date::AddMonths(today, 36);

    // True basis curve: 10bp at 1Y, 15bp at 2Y, 20bp at 3Y
    const Vector_<Date_> trueKnots = {maturity1Y, maturity2Y, maturity3Y};
    const Vector_<> trueRates = {0.0010, 0.0015, 0.0020};

    auto trueDomesticBlock = MakeBlock("usd", "USD", today, 0.02);
    auto trueForeignBlock = MakeBlock("eur", "EUR", today, 0.01);
    CrossCurrencyMarket_ trueMarket(trueDomesticBlock, trueForeignBlock, 1.10);
    trueMarket.SetBasisCurve(Handle_<DiscountCurve_>(NewDiscountPWC("true_basis", "USD", PiecewiseConstant_(trueKnots, trueRates))));

    // Create instruments and read their par spreads from the true market
    auto makeSwap = [&](const Date_& maturity) {
        return CrossCurrencySwap_(today, today, maturity, 0.0, pair, 110.0, 100.0, convention);
    };

    const auto swap1Y = makeSwap(maturity1Y);
    const auto swap2Y = makeSwap(maturity2Y);
    const auto swap3Y = makeSwap(maturity3Y);

    const double quote1Y = (*swap1Y.Precompute())(trueMarket);
    const double quote2Y = (*swap2Y.Precompute())(trueMarket);
    const double quote3Y = (*swap3Y.Precompute())(trueMarket);

    // Calibrate using the observed quotes
    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
    spec.instruments_ = {
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity1Y, quote1Y, pair, 110.0, 100.0, convention)),
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity2Y, quote2Y, pair, 110.0, 100.0, convention)),
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity3Y, quote3Y, pair, 110.0, 100.0, convention))
    };
    spec.knotDates_ = trueKnots;
    spec.smoothingWeight_ = 1.0;
    spec.tolerance_ = 1e-10;

    const auto result = CalibrateCrossCurrencyMarket(spec);

    // Check repricing
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
    ASSERT_LT(result.diagnostics_.rmsResidual_, 1e-8);
    ASSERT_EQ(result.diagnostics_.instrumentNames_.size(), 3);

    // Verify the calibrated market reprices all instruments
    for (int i = 0; i < 3; ++i) {
        const double modelRate = result.diagnostics_.modelRates_[i];
        const double marketRate = result.diagnostics_.marketRates_[i];
        ASSERT_NEAR(modelRate, marketRate, 1e-8);
    }
}

TEST(XccyMarketTest, TestCalibrationWithApproximateSolveMode) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
    spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
    spec.knotDates_ = {Date::AddMonths(today, 12)};
    spec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.fitTolerance_ = 1e-6;
    spec.tolerance_ = 1e-8;

    const auto result = CalibrateCrossCurrencyMarket(spec);
    ASSERT_TRUE(result.diagnostics_.usedApproximateFit_);
    ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-4);
}

TEST(XccyMarketTest, TestCalibrationRejectsInvalidSpec) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {};
        spec.knotDates_ = {Date::AddMonths(today, 12)};
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {};
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {Date::AddMonths(today, 12)};
        spec.smoothingWeight_ = 0.0;
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {Date::AddMonths(today, 12)};
        spec.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("GBP"));
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {Date::AddMonths(today, 12)};
        spec.basisPair_ = CurrencyPair_(Ccy_("GBP"), Ccy_("EUR"));
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
}
