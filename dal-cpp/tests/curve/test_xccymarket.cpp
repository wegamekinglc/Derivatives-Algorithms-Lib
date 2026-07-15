//
// Created by GitHub Copilot on 2026/6/6.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>

#include <cmath>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/datetime.hpp>
#include <map>

using namespace Dal;

namespace {
    Handle_<DiscountCurve_> MakeFlatCurve(const String_& name, const String_& ccy, const Date_& today, double rate) {
        const Vector_<Date_> knots = {Date::AddMonths(today, 12), Date::AddMonths(today, 24)};
        const Vector_<> vals(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, vals, vals)));
    }

    Handle_<CurveBlock_> MakeBlock(const String_& name, const String_& ccy, const Date_& today, double rate) {
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
        retval.today_ = today;
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
        return CrossCurrencySwap_(today, today, Date::AddMonths(today, 12), quotedSpread, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 110.0, 100.0,
                                  MakeConvention());
    }
} // namespace

TEST(XccyMarketTest, TestFxForwardParityUsesDiscountCurves) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const Date_ maturity = Date::AddMonths(today, 12);
    const auto market = MakeMarket(today);

    const double expected = 1.10 * market.ForeignDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity) /
                            market.DomesticDiscountCurve(CollateralType_(CollateralType_::Value_::OIS))(today, maturity);
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

TEST(XccyMarketTest, TestSetBasisCurveRejectsForeignCurrency) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    auto market = MakeMarket(today);

    // The basis discount factor is applied to the domestic leg, so a basis
    // curve in any other currency would silently misprice swaps and FX
    // forwards and must be rejected.
    ASSERT_THROW(market.SetBasisCurve(MakeFlatCurve("basis", "EUR", today, 0.002)), Dal::Exception_);
    market.SetBasisCurve(MakeFlatCurve("basis", "USD", today, 0.002));
    ASSERT_LT(market.BasisDiscountFactor(today, Date::AddMonths(today, 12)), 1.0);
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

TEST(XccyMarketTest, TestCurrencyPairOrderingAndMapKey) {
    const CurrencyPair_ usdEur(Ccy_("USD"), Ccy_("EUR"));
    const CurrencyPair_ eurUsd(Ccy_("EUR"), Ccy_("USD"));
    const CurrencyPair_ usdGbp(Ccy_("USD"), Ccy_("GBP"));

    // Strict-weak ordering: exactly one of a<b, b<a holds for distinct pairs.
    ASSERT_NE(usdEur < eurUsd, eurUsd < usdEur);
    ASSERT_NE(usdEur < usdGbp, usdGbp < usdEur);

    // Equal pairs are not ordered either way.
    ASSERT_FALSE(usdEur < CurrencyPair_(Ccy_("USD"), Ccy_("EUR")));
    ASSERT_FALSE(CurrencyPair_(Ccy_("USD"), Ccy_("EUR")) < usdEur);

    // Usable as a std::map key with distinct entries.
    std::map<CurrencyPair_, int> byPair;
    byPair[usdEur] = 1;
    byPair[eurUsd] = 2;
    byPair[usdGbp] = 3;
    ASSERT_EQ(byPair.size(), 3u);
    ASSERT_EQ(byPair[usdEur], 1);
    ASSERT_EQ(byPair[eurUsd], 2);
    ASSERT_EQ(byPair[usdGbp], 3);
}

TEST(XccyMarketTest, TestSwapPricingRejectsMismatchedPair) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today); // USD/EUR orientation

    CrossCurrencyConvention_ convention = MakeConvention();

    const CrossCurrencySwap_ mismatchedSwap(today, today, Date::AddMonths(today, 12), 0.0, CurrencyPair_(Ccy_("USD"), Ccy_("GBP")), 110.0, 100.0,
                                            convention);
    const auto rate = mismatchedSwap.Precompute();
    ASSERT_THROW(static_cast<void>((*rate)(market)), Dal::Exception_);
}

TEST(XccyMarketTest, TestSwapPricingInProgressUsesHistoricalFixings) {
    const Date_ tradeDate(2024, 1, 15);
    const Date_ start = tradeDate;
    const Date_ maturity = Date::AddMonths(start, 12);
    const Date_ evaluationDate = Date::AddMonths(start, 3);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(evaluationDate);
    const auto market = MakeMarket(evaluationDate);

    CrossCurrencySwapConfig_ config;
    config.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    config.domesticNotional_ = 110.0;
    config.foreignNotional_ = 100.0;
    config.convention_ = MakeConvention();
    config.domesticRateFixing_ = {"USD-XCCY-IN-PROGRESS", 11, 0};
    config.foreignRateFixing_ = {"EUR-XCCY-IN-PROGRESS", 11, 0};

    const DateTime_ fixingTime(start, 11, 0);
    FixHistory_ domesticHistory;
    domesticHistory.vals_ = {{fixingTime, 0.04}};
    XGLOBAL::StoreFixings(config.domesticRateFixing_.indexName_, domesticHistory, false);
    FixHistory_ foreignHistory;
    foreignHistory.vals_ = {{fixingTime, 0.03}};
    XGLOBAL::StoreFixings(config.foreignRateFixing_.indexName_, foreignHistory, false);

    const CrossCurrencySwap_ swap(tradeDate, start, maturity, 0.0, config);
    ASSERT_TRUE(std::isfinite((*swap.Precompute())(market)));
}

TEST(XccyMarketTest, TestParSpreadAnchoredToEvaluationDateNotTradeDate) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);

    const Date_ start = today;
    const Date_ maturity = Date::AddMonths(today, 12);

    CrossCurrencyConvention_ convention = MakeConvention();

    auto makeSwap = [&](const Date_& tradeDate) {
        return CrossCurrencySwap_(tradeDate, start, maturity, 0.0, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 110.0, 100.0, convention);
    };

    // Pricing must value all cashflows as of the market evaluation date, so the
    // model par spread is independent of the historical trade date.
    const double spreadTradedToday = (*makeSwap(today).Precompute())(market);
    const double spreadTradedEarlier = (*makeSwap(today.AddDays(-45)).Precompute())(market);

    ASSERT_NEAR(spreadTradedToday, spreadTradedEarlier, 1e-10);
}

TEST(XccyMarketTest, TestForwardStartingParFloatersHaveZeroParSpread) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);

    // Single curve per currency (forecast == discount) and zero basis make each
    // leg a par floating-rate note: floating coupons plus the final notional
    // exchange minus the initial notional exchange must value to zero. This
    // holds for a forward-starting swap only when the initial exchange is
    // discounted to its start date the same way the final exchange is, so the
    // model par spread must be zero even though the evaluation date precedes
    // the accrual start.
    const Date_ start = Date::AddMonths(today, 6);
    const Date_ maturity = Date::AddMonths(today, 18);
    const CrossCurrencySwap_ forwardStarting(today, start, maturity, 0.0, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 110.0, 100.0, MakeConvention());

    ASSERT_GT(start, today);
    ASSERT_NEAR((*forwardStarting.Precompute())(market), 0.0, 1e-10);
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

TEST(XccyMarketTest, TestResetConfigRequiresExplicitFixingIdentity) {
    CrossCurrencySwapConfig_ config;
    config.notionalMode_ = XccyNotionalMode_::Value_::RESETTABLE;
    ASSERT_THROW(static_cast<void>(CrossCurrencySwap_(Date_(2024, 1, 2), Date_(2024, 1, 4), Date_(2025, 1, 4), 0.0, config)), Dal::Exception_);
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
    Handle_<CrossCurrencySwap_> swap(new CrossCurrencySwap_(today, today, maturity, 0.0, pair, 110.0, 100.0, convention));
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
    instruments.push_back(Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity, 0.0050, pair, 110.0, 100.0, convention)));

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
    auto makeSwap = [&](const Date_& maturity) { return CrossCurrencySwap_(today, today, maturity, 0.0, pair, 110.0, 100.0, convention); };

    const auto swap1Y = makeSwap(maturity1Y);
    const auto swap2Y = makeSwap(maturity2Y);
    const auto swap3Y = makeSwap(maturity3Y);

    const double quote1Y = (*swap1Y.Precompute())(trueMarket);
    const double quote2Y = (*swap2Y.Precompute())(trueMarket);
    const double quote3Y = (*swap3Y.Precompute())(trueMarket);

    // Calibrate using the observed quotes
    CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
    spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity1Y, quote1Y, pair, 110.0, 100.0, convention)),
                         Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity2Y, quote2Y, pair, 110.0, 100.0, convention)),
                         Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(today, today, maturity3Y, quote3Y, pair, 110.0, 100.0, convention))};
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
        spec.instruments_ = {Handle_<CrossCurrencySwap_>()};
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
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {today};
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {Date::AddMonths(today, 24), Date::AddMonths(today, 12)};
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
    {
        CrossCurrencyCalibrationSpec_ spec = MakeCalibrationSpec(today, pair);
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeSwap(today, 0.0020)))};
        spec.knotDates_ = {Date::AddMonths(today, 12), Date::AddMonths(today, 12)};
        ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
    }
}

TEST(XccyMarketTest, TestMtmSwapPricingUsesResetKernel) {
    const Date_ today(2024, 1, 15);
    const auto evalDate = XGLOBAL::SetEvaluationDateInScope(today);
    const auto market = MakeMarket(today);

    CrossCurrencySwapConfig_ config;
    config.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    config.domesticNotional_ = 110.0;
    config.foreignNotional_ = 100.0;
    config.convention_ = MakeConvention();
    config.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
    config.convention_.foreignLeg_.paymentFrequency_ = PeriodLength_("3M");
    config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
    config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
    config.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    config.fxReset_.fixingLag_ = 0;
    config.fxReset_.fixingHour_ = 11;
    config.fxReset_.fixingMinute_ = 0;
    config.domesticRateFixing_ = {"USD-XCCY-MTM", 11, 0};
    config.foreignRateFixing_ = {"EUR-XCCY-MTM", 11, 0};

    const CrossCurrencySwap_ swap(today, today, Date::AddMonths(today, 12), 0.0, config);
    ASSERT_TRUE(std::isfinite((*swap.Precompute())(market)));
}
TEST(XccyMarketTest, TestResetAwareCalibrationUsesExplicitValuationAndFixingSnapshot) {
    const Date_ valuationDate(2025, 1, 16);
    const DateTime_ valuationTime(valuationDate, 9, 0);
    const Ccy_ collateral("USD");
    const auto domesticBlock = MakeBlock("usd_explicit", "USD", valuationDate, 0.02);
    const auto foreignBlock = MakeBlock("eur_explicit", "EUR", valuationDate, 0.01);

    CrossCurrencySwapConfig_ config;
    config.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
    config.domesticNotional_ = 110.0;
    config.foreignNotional_ = 100.0;
    config.convention_ = MakeConvention();
    config.convention_.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
    config.convention_.foreignLeg_.paymentFrequency_ = PeriodLength_("3M");
    config.convention_.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
    config.convention_.foreignIndex_.forecastTenor_ = PeriodLength_("3M");
    config.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
    config.fxReset_.fixingLag_ = 0;
    config.fxReset_.fixingHour_ = 11;
    config.fxReset_.fixingMinute_ = 0;
    config.domesticRateFixing_ = {"USD-XCCY-CAL", 11, 0};
    config.foreignRateFixing_ = {"EUR-XCCY-CAL", 11, 0};

    const Date_ startedDate(2024, 10, 15);
    const DateTime_ historicalFixing(Date::AddMonths(startedDate, 3), 11, 0);
    MarketFixingSnapshot_::values_t values;
    values[config.domesticRateFixing_.indexName_][historicalFixing] = 0.04;
    values[config.foreignRateFixing_.indexName_][historicalFixing] = 0.03;
    values[FxIndexName(config.pair_)][historicalFixing] = 1.20;
    const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(values));

    CrossCurrencyMarket_ quoteMarket(domesticBlock, foreignBlock, 1.10, valuationTime, collateral, fixings);
    quoteMarket.SetBasisCurve(MakeFlatCurve("known_explicit_basis", "USD", valuationDate, 0.002));

    const Date_ startedMaturity = Date::AddMonths(startedDate, 12);
    const Date_ futureStart = Date::AddMonths(valuationDate, 1);
    const Date_ futureMaturity = Date::AddMonths(futureStart, 12);
    const CrossCurrencySwap_ startedPrototype(startedDate, startedDate, startedMaturity, 0.0, config);
    const CrossCurrencySwap_ futurePrototype(valuationDate, futureStart, futureMaturity, 0.0, config);
    const double startedQuote = (*startedPrototype.Precompute())(quoteMarket);
    const double futureQuote = (*futurePrototype.Precompute())(quoteMarket);

    CrossCurrencyCalibrationSpec_ spec;
    spec.today_ = valuationDate;
    spec.valuationTime_ = valuationTime;
    spec.collateralCurrency_ = collateral;
    spec.fixings_ = fixings;
    spec.basisPair_ = config.pair_;
    spec.domesticCurveBlock_ = domesticBlock;
    spec.foreignCurveBlock_ = foreignBlock;
    spec.fxSpot_ = 1.10;
    spec.instruments_ = {
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(startedDate, startedDate, startedMaturity, startedQuote, config)),
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(valuationDate, futureStart, futureMaturity, futureQuote, config)),
    };
    spec.knotDates_ = {startedMaturity, futureMaturity};
    spec.initialGuess_ = 0.0;
    spec.tolerance_ = 1.0e-10;

    CrossCurrencyCalibrationOptions_ options;
    options.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
    const auto result = CalibrateCrossCurrencyMarket(spec, options);
    ASSERT_LE(result.diagnostics_.maxAbsResidual_, 1.0e-8);
    ASSERT_FALSE(result.diagnostics_.jacobian_.Empty());
}

TEST(XccyMarketTest, TestExplicitContextRejectsNonDomesticCollateral) {
    const Date_ today(2025, 1, 16);
    const DateTime_ valuationTime(today, 9, 0);
    const auto domestic = MakeBlock("usd_collateral", "USD", today, 0.02);
    const auto foreign = MakeBlock("eur_collateral", "EUR", today, 0.01);
    ASSERT_THROW(static_cast<void>(CrossCurrencyMarket_(domestic, foreign, 1.10, valuationTime, Ccy_("EUR"))), Dal::Exception_);

    auto spec = MakeCalibrationSpec(today, CurrencyPair_(Ccy_("USD"), Ccy_("EUR")));
    spec.valuationTime_ = valuationTime;
    spec.collateralCurrency_ = Ccy_("EUR");
    ASSERT_THROW(static_cast<void>(CalibrateCrossCurrencyMarket(spec)), Dal::Exception_);
}
