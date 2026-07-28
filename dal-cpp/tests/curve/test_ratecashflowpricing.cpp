//
// Created by dal-implementer on 2026/7/28.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycconst.hpp>

namespace {
    Dal::RateIndexConvention_ QuarterlyIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    Dal::RateLegConvention_ AnnualLeg() {
        Dal::RateLegConvention_ result;
        result.paymentFrequency_ = Dal::PeriodLength_("12M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        return result;
    }

    Dal::Handle_<Dal::DiscountCurve_> FlatCurve(const Dal::Date_& maturity, double rate = 0.04, const Dal::String_& ccy = "USD") {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC("flat", ccy, Dal::PiecewiseConstant_({maturity}, {rate})));
    }

    Dal::RatePricingMarket_ Market(const Dal::Date_& today, const Dal::Handle_<Dal::DiscountCurve_>& curve) {
        Dal::RatePricingMarket_ result;
        result.valuationTime_ = Dal::DateTime_(today, 10, 30);
        result.resultCurrency_ = Dal::Ccy_("USD");
        result.curveComponents_["discount"] = curve;
        result.curveComponents_["forecast"] = curve;
        result.curveComponents_["reference"] = curve;
        result.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return result;
    }

    Dal::RateTradeDefinition_ Trade(const Dal::RateInstrumentType_& type,
                                    const Dal::Date_& today,
                                    const Dal::Date_& start,
                                    const Dal::Date_& maturity,
                                    const Dal::RateTradeTerms_& terms) {
        return {"trade-1", type, today, start, maturity, Dal::Ccy_("USD"), terms};
    }
} // namespace

TEST(RateCashflowPricingTest, TestRegistryAndDepositCashflows) {
    const auto families = Dal::RateInstrumentTypeListAll();
    ASSERT_EQ(families.size(), 7);
    ASSERT_EQ(families[0].String(), "DEPOSIT");
    ASSERT_EQ(families[1].String(), "FRA");
    ASSERT_EQ(families[2].String(), "FUTURE");
    ASSERT_EQ(families[3].String(), "OIS");
    ASSERT_EQ(families[4].String(), "IRS");
    ASSERT_EQ(families[5].String(), "BASIS_SWAP");
    ASSERT_EQ(families[6].String(), "XCCY");

    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto definition = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto result = Dal::PriceRateTrade(definition, market);
    const double accrual = terms.index_.dayBasis_(today, maturity, nullptr);
    const double expected = -100.0 + 100.0 * (1.0 + 0.05 * accrual) * (*curve)(today, maturity);

    ASSERT_TRUE(result.succeeded_);
    ASSERT_NEAR(result.pv_, expected, 1.0e-10);
    ASSERT_TRUE(result.requiredHistoricalFixings_.empty());
    ASSERT_EQ(result.dependencyComponentKeys_, Dal::Vector_<Dal::String_>({"discount"}));

    terms.lend_ = false;
    const auto borrow = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    ASSERT_NEAR(Dal::PriceRateTrade(borrow, market).pv_, -expected, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADMatchesCentralNativeParameterBump) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    const double rate = 0.04;
    const auto market = Market(today, FlatCurve(maturity, rate));
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);

    const auto aad = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    const double epsilon = 1.0e-6;
    const double plus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate + epsilon))).pv_;
    const double minus = Dal::PriceRateTrade(trade, Market(today, FlatCurve(maturity, rate - epsilon))).pv_;

    ASSERT_TRUE(aad.eligible_);
    ASSERT_EQ(aad.gradient_.size(), 1);
    ASSERT_NEAR(aad.pv_, Dal::PriceRateTrade(trade, market).pv_, 1.0e-12);
    ASSERT_NEAR(aad.gradient_[0], (plus - minus) / (2.0 * epsilon), 1.0e-6);
}

TEST(RateCashflowPricingTest, TestDepositNodeAADRewindsPerTradeAndIsThreadLocal) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2027, 1, 15);
    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("DEPOSIT"), today, today, maturity, terms);
    const auto evaluate = [=](double rate) {
        return Dal::RateTradeNodeSensitivities(
            trade,
            Market(today, FlatCurve(maturity, rate)),
            "discount");
    };

    const auto first = evaluate(0.04);
    const auto repeated = evaluate(0.04);
    auto left = std::async(std::launch::async, evaluate, 0.03);
    auto right = std::async(std::launch::async, evaluate, 0.06);
    const auto leftResult = left.get();
    const auto rightResult = right.get();

    ASSERT_TRUE(first.eligible_);
    ASSERT_TRUE(repeated.eligible_);
    ASSERT_TRUE(leftResult.eligible_);
    ASSERT_TRUE(rightResult.eligible_);
    ASSERT_EQ(first.gradient_, repeated.gradient_);
    ASSERT_NE(leftResult.gradient_[0], rightResult.gradient_[0]);
}

TEST(RateCashflowPricingTest, TestFraAndFutureFormulas) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ start(2026, 4, 15);
    const Dal::Date_ maturity(2026, 7, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);
    const auto index = QuarterlyIndex();
    const double accrual = index.dayBasis_(start, maturity, nullptr);
    const double forward = (1.0 / (*curve)(start, maturity) - 1.0) / accrual;

    Dal::FraTradeTerms_ fra;
    fra.notional_ = 2'000'000.0;
    fra.contractRate_ = 0.03;
    fra.receiveFloating_ = true;
    fra.settleAtStart_ = true;
    fra.index_ = index;
    fra.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    fra.forecastComponentKey_ = "forecast";
    fra.discountComponentKey_ = "discount";
    const auto fraResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FRA"), today, start, maturity, fra), market);
    const double payoff = fra.notional_ * accrual * (forward - fra.contractRate_) / (1.0 + accrual * forward);
    ASSERT_NEAR(fraResult.pv_, payoff * (*curve)(today, start), 1.0e-8);

    Dal::FutureTradeTerms_ future;
    future.contractCount_ = 12.0;
    future.long_ = true;
    future.referencePrice_ = 95.0;
    future.contractValuePerPricePoint_ = 25.0;
    future.convexityAdjustment_ = 0.0005;
    future.index_ = index;
    future.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    future.forecastComponentKey_ = "forecast";
    const auto futureResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    const double modelPrice = 100.0 * (1.0 - forward + future.convexityAdjustment_);
    ASSERT_NEAR(futureResult.pv_, 12.0 * 25.0 * (modelPrice - 95.0), 1.0e-10);

    future.long_ = false;
    const auto shortResult = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("FUTURE"), today, start, maturity, future), market);
    ASSERT_NEAR(shortResult.pv_, -futureResult.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFloatingFamilySidesAreExactOpposites) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto curve = FlatCurve(maturity);
    const auto market = Market(today, curve);

    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    for (const auto& family : {"OIS", "IRS"}) {
        const auto type = Dal::RateInstrumentType_(family);
        const Dal::RateTradeTerms_ first = family == std::string("OIS") ? Dal::RateTradeTerms_(Dal::OisTradeTerms_{fixedFloat})
                                                                        : Dal::RateTradeTerms_(Dal::IrsTradeTerms_{fixedFloat});
        const double receive = Dal::PriceRateTrade(Trade(type, today, today, maturity, first), market).pv_;
        fixedFloat.payFixed_ = false;
        const Dal::RateTradeTerms_ opposite = family == std::string("OIS") ? Dal::RateTradeTerms_(Dal::OisTradeTerms_{fixedFloat})
                                                                           : Dal::RateTradeTerms_(Dal::IrsTradeTerms_{fixedFloat});
        const double pay = Dal::PriceRateTrade(Trade(type, today, today, maturity, opposite), market).pv_;
        ASSERT_NEAR(receive, -pay, 1.0e-10);
        fixedFloat.payFixed_ = true;
    }

    Dal::BasisTradeTerms_ basis;
    basis.notional_ = 1'000'000.0;
    basis.contractSpread_ = 0.001;
    basis.receiveReferencePaySpread_ = true;
    basis.spreadLeg_ = AnnualLeg();
    basis.referenceLeg_ = AnnualLeg();
    basis.spreadIndex_ = QuarterlyIndex();
    basis.referenceIndex_ = QuarterlyIndex();
    basis.spreadFixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    basis.referenceFixingIdentity_ = {"USD-SOFR", 11, 0};
    basis.spreadForecastComponentKey_ = "forecast";
    basis.referenceForecastComponentKey_ = "reference";
    basis.discountComponentKey_ = "discount";
    const auto first = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, basis), market);
    basis.receiveReferencePaySpread_ = false;
    const auto opposite = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("BASIS_SWAP"), today, today, maturity, basis), market);
    ASSERT_NEAR(first.pv_, -opposite.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestXccySpreadLegSidesAndPositionCount) {
    const Dal::Date_ today(2026, 1, 15);
    const Dal::Date_ maturity(2028, 1, 15);
    const auto usd = FlatCurve(maturity, 0.04, "USD");
    const auto eur = FlatCurve(maturity, 0.04, "EUR");
    const auto collateral = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
    const auto usdBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("usd", "USD", {{collateral, usd}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto eurBlock = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_("eur", "EUR", {{collateral, eur}}, {}, Dal::DayBasis_("ACT_365F")));
    const auto fixings = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
    const auto xccyMarket =
        std::make_shared<Dal::CrossCurrencyMarket_>(usdBlock, eurBlock, 1.2, Dal::DateTime_(today, 10, 30), Dal::Ccy_("USD"), fixings);

    Dal::XccyTradeTerms_ terms;
    terms.positionCount_ = 2.0;
    terms.contractSpread_ = 0.001;
    terms.spreadOnForeignLeg_ = true;
    terms.receiveNonSpreadPaySpread_ = true;
    terms.config_.pair_ = Dal::CurrencyPair_(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
    terms.config_.domesticNotional_ = 120.0;
    terms.config_.foreignNotional_ = 100.0;
    terms.config_.convention_.domesticLeg_ = AnnualLeg();
    terms.config_.convention_.foreignLeg_ = AnnualLeg();
    terms.config_.convention_.domesticIndex_ = QuarterlyIndex();
    terms.config_.convention_.foreignIndex_ = QuarterlyIndex();
    terms.config_.convention_.spreadOnForeignLeg_ = true;
    terms.config_.domesticRateFixing_ = {"USD-SOFR", 11, 0};
    terms.config_.foreignRateFixing_ = {"EUR-ESTR", 11, 0};

    Dal::RatePricingMarket_ market;
    market.valuationTime_ = Dal::DateTime_(today, 10, 30);
    market.resultCurrency_ = Dal::Ccy_("USD");
    market.xccyMarket_ = xccyMarket;
    market.fixings_ = fixings;
    const auto trade = Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms);
    const auto receive = Dal::PriceRateTrade(trade, market);
    ASSERT_TRUE(receive.succeeded_);

    terms.receiveNonSpreadPaySpread_ = false;
    const auto pay = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(pay.succeeded_);
    ASSERT_NEAR(receive.pv_, -pay.pv_, 1.0e-10);

    terms.positionCount_ = 1.0;
    terms.receiveNonSpreadPaySpread_ = true;
    const auto single = Dal::PriceRateTrade(Trade(Dal::RateInstrumentType_("XCCY"), today, today, maturity, terms), market);
    ASSERT_TRUE(single.succeeded_);
    ASSERT_NEAR(receive.pv_, 2.0 * single.pv_, 1.0e-10);
}

TEST(RateCashflowPricingTest, TestFixingAndPaymentBoundaries) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    const auto curve = FlatCurve(maturity);
    Dal::FraTradeTerms_ terms;
    terms.notional_ = 1'000'000.0;
    terms.contractRate_ = 0.03;
    terms.receiveFloating_ = true;
    terms.settleAtStart_ = false;
    terms.index_ = QuarterlyIndex();
    terms.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
    terms.forecastComponentKey_ = "forecast";
    terms.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("FRA"), start, start, maturity, terms);

    auto missingMarket = Market(start, curve);
    missingMarket.valuationTime_ = Dal::DateTime_(start.AddDays(1), 10, 30);
    const auto missing = Dal::PriceRateTrade(trade, missingMarket);
    ASSERT_FALSE(missing.succeeded_);
    ASSERT_EQ(missing.requiredHistoricalFixings_.size(), 1);
    ASSERT_EQ(missing.missingHistoricalFixings_.size(), 1);

    auto atFixing = Market(start, curve);
    atFixing.valuationTime_ = Dal::DateTime_(start, 11, 0);
    const auto forecast = Dal::PriceRateTrade(trade, atFixing);
    ASSERT_TRUE(forecast.succeeded_);
    ASSERT_TRUE(forecast.requiredHistoricalFixings_.empty());

    Dal::MarketFixingSnapshot_::values_t history;
    history["USD-LIBOR-3M"][Dal::DateTime_(start, 11, 0)] = -0.001;
    auto atPayment = Market(start, curve);
    atPayment.valuationTime_ = Dal::DateTime_(maturity, 10, 30);
    atPayment.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(history));
    const auto payment = Dal::PriceRateTrade(trade, atPayment);
    ASSERT_TRUE(payment.succeeded_);
    const double accrual = terms.index_.dayBasis_(start, maturity, nullptr);
    ASSERT_NEAR(payment.pv_, terms.notional_ * accrual * (-0.001 - terms.contractRate_), 1.0e-10);

    auto expired = Market(start, curve);
    expired.valuationTime_ = Dal::DateTime_(maturity.AddDays(1), 10, 30);
    const auto paid = Dal::PriceRateTrade(trade, expired);
    ASSERT_TRUE(paid.succeeded_);
    ASSERT_DOUBLE_EQ(paid.pv_, 0.0);
    ASSERT_TRUE(paid.requiredHistoricalFixings_.empty());
}

TEST(RateCashflowPricingTest, TestOisPlanRecordsEachDailyHistoricalObservation) {
    const Dal::Date_ start(2026, 1, 15);
    const Dal::Date_ maturity(2026, 4, 15);
    Dal::FixedFloatTradeTerms_ fixedFloat;
    fixedFloat.notional_ = 1'000'000.0;
    fixedFloat.contractRate_ = 0.03;
    fixedFloat.payFixed_ = true;
    fixedFloat.fixedLeg_ = AnnualLeg();
    fixedFloat.floatLeg_ = AnnualLeg();
    fixedFloat.floatIndex_ = QuarterlyIndex();
    fixedFloat.fixingIdentity_ = {"USD-SOFR", 11, 0};
    fixedFloat.forecastComponentKey_ = "forecast";
    fixedFloat.discountComponentKey_ = "discount";
    const auto trade = Trade(Dal::RateInstrumentType_("OIS"), start, start, maturity, Dal::OisTradeTerms_{fixedFloat});

    const auto plan = Dal::BuildRateCashflowPlan(trade, Dal::DateTime_(start.AddDays(2), 10, 30));

    ASSERT_EQ(plan.requiredHistoricalFixings_.size(), 2);
    EXPECT_EQ(plan.requiredHistoricalFixings_[0].fixingTime_, Dal::DateTime_(start, 11, 0));
    EXPECT_EQ(plan.requiredHistoricalFixings_[1].fixingTime_, Dal::DateTime_(start.AddDays(1), 11, 0));
}
