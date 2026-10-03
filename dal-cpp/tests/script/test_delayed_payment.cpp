//
// Created by wegamekinglc on 2026/10/3.
//
// Delayed payments (var PAYS expr ON date): parsing, preparation, discounting
// semantics across tree/compiled/fuzzy/LSMC evaluators and models.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/operators.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/model/hybriddata.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    constexpr double RATE = 0.05;

    double YearFraction(const Date_& from, const Date_& to) { return static_cast<double>(to - from) / DAYS_PER_YEAR; }

    Handle_<ModelData_> FlatBsModel(double spot = 100.0, double vol = 0.0, double rate = RATE, double div = 0.0) {
        return Handle_<ModelData_>(new BSModelData_("", spot, vol, rate, div));
    }

    ScriptValuationSettings_ ValuationOn(const Date_& date) {
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = date;
        return valuation;
    }

    double MeanValue(const ScriptProductData_& product, const Handle_<ModelData_>& model, size_t nPaths, const Date_& evalDate) {
        return MCSimulation<double>(product, model, nPaths, ValuationOn(evalDate)).aggregated_ / static_cast<double>(nPaths);
    }

    Handle_<ModelData_> GsrModel(double gVol) {
        const Date_ today(2023, 1, 28);
        const Date_ oneYear(2024, 1, 28);
        const Date_ twoYears(2025, 1, 28);
        const Handle_<GSRCurveData_> curve(
            new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
        const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {gVol}, {today}, {1.0 / 0.03}));
        return Handle_<ModelData_>(new GSRModelData_("gsr", curve, vol));
    }
} // namespace

TEST(ScriptDelayedPaymentTest, TestParserAcceptsOnDateLiteral) {
    const Date_ evalDate(2023, 1, 28);
    const ScriptProductData_ product("", {Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON 2023-08-28"});
    auto model = AAD::BlackScholes_<double>(100.0, 0.0, RATE, 0.0);
    const auto prepared = PrepareScript(product, &model, ValuationOn(evalDate), {});
    ASSERT_EQ(prepared.Plan().DefLine().size(), 1u);
    ASSERT_EQ(prepared.Plan().DefLine()[0].discountMats_.size(), 1u);
    ASSERT_NEAR(prepared.Plan().DefLine()[0].discountMats_[0], YearFraction(evalDate, Date_(2023, 8, 28)), 1e-10);
}

TEST(ScriptDelayedPaymentTest, TestParserRejectsMalformedOnClause) {
    {
        const ScriptProductData_ product("", {Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON"});
        ASSERT_THROW(product.Product(), Dal::Exception_);
    }
    {
        const ScriptProductData_ product("", {Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON 28-08-2023"});
        ASSERT_THROW(product.Product(), Dal::Exception_);
    }
    {
        const ScriptProductData_ product("", {Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON 2023-08-28 + 1"});
        ASSERT_THROW(product.Product(), Dal::Exception_);
    }
    {
        //  ON is a reserved word: assignments to a variable named `on` stop parsing
        const ScriptProductData_ product("", {Cell_(Date_(2023, 2, 28))}, {"on = 1"});
        ASSERT_THROW(product.Product(), Dal::Exception_);
    }
}

TEST(ScriptDelayedPaymentTest, TestPaymentDateMustNotPrecedeItsEvent) {
    const Date_ evalDate(2023, 1, 28);
    const ScriptProductData_ product("", {Cell_(Date_(2023, 5, 28))}, {"pay PAYS 100 ON 2023-05-27"});
    auto model = AAD::BlackScholes_<double>(100.0, 0.0, RATE, 0.0);
    try {
        static_cast<void>(PrepareScript(product, &model, ValuationOn(evalDate), {}));
        FAIL() << "expected InvalidPaymentDate";
    } catch (const Dal::Exception_& error) {
        ASSERT_NE(String_(error.what()).find("InvalidPaymentDate"), String_::npos);
    }
}

TEST(ScriptDelayedPaymentTest, TestSameDateOnBehavesLikePlainPays) {
    const Date_ eventDate(2023, 2, 28);
    const ScriptProductData_ plain("", {Cell_(eventDate)}, {"pay PAYS 100"});
    const ScriptProductData_ delayed("", {Cell_(eventDate)}, {"pay PAYS 100 ON 2023-02-28"});
    ASSERT_DOUBLE_EQ(MeanValue(plain, FlatBsModel(), 256, eventDate), MeanValue(delayed, FlatBsModel(), 256, eventDate));
}

TEST(ScriptDelayedPaymentTest, TestBsFlatRateDiscountsFromPaymentDate) {
    const Date_ evalDate(2023, 1, 28);
    const ScriptProductData_ product("",
                                     {Cell_(Date_(2023, 2, 28)), Cell_(Date_(2023, 5, 28))},
                                     {"pay PAYS 100 ON 2023-08-28", "pay PAYS 50 ON 2024-02-28"});
    const double expected = 100.0 * std::exp(-RATE * YearFraction(evalDate, Date_(2023, 8, 28))) +
                            50.0 * std::exp(-RATE * YearFraction(evalDate, Date_(2024, 2, 28)));
    //  vol = 0 keeps every path deterministic, so the estimate is exact
    ASSERT_NEAR(MeanValue(product, FlatBsModel(), 64, evalDate), expected, 1e-12);
}

TEST(ScriptDelayedPaymentTest, TestValueIsIndependentOfTheEventDate) {
    const ScriptProductData_ early("", {Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON 2023-11-28"});
    const ScriptProductData_ late("", {Cell_(Date_(2023, 8, 28))}, {"pay PAYS 100 ON 2023-11-28"});
    ASSERT_DOUBLE_EQ(MeanValue(early, FlatBsModel(), 64, Date_(2023, 1, 28)), MeanValue(late, FlatBsModel(), 64, Date_(2023, 1, 28)));
}

TEST(ScriptDelayedPaymentTest, TestSlotsDedupeWithinAndAcrossEvents) {
    const Date_ evalDate(2023, 1, 28);
    //  the first event carries two payments on one shared date (dedup to a single slot)
    //  plus a third on another date (a second slot on the same sample); the second
    //  event adds its own maturity on its own sample
    const ScriptProductData_ product("",
                                     {Cell_(Date_(2023, 2, 28)), Cell_(Date_(2023, 5, 28))},
                                     {"a PAYS 1 ON 2023-11-28 b PAYS 2 ON 2023-11-28 d PAYS 4 ON 2024-02-28", "c PAYS 3 ON 2024-05-28"});
    auto model = AAD::BlackScholes_<double>(100.0, 0.0, RATE, 0.0);
    const auto prepared = PrepareScript(product, &model, ValuationOn(evalDate), {});
    const auto& defLine = prepared.Plan().DefLine();
    ASSERT_EQ(defLine.size(), 2u);
    ASSERT_EQ(defLine[0].discountMats_.size(), 2u);
    ASSERT_EQ(defLine[1].discountMats_.size(), 1u);
    ASSERT_NEAR(defLine[0].discountMats_[0], YearFraction(evalDate, Date_(2023, 11, 28)), 1e-10);
    ASSERT_NEAR(defLine[0].discountMats_[1], YearFraction(evalDate, Date_(2024, 2, 28)), 1e-10);
    ASSERT_NEAR(defLine[1].discountMats_[0], YearFraction(evalDate, Date_(2024, 5, 28)), 1e-10);
}

TEST(ScriptDelayedPaymentTest, TestPastDelayedPaymentIsConsumed) {
    const Date_ evalDate(2023, 6, 28);
    //  the settled event (with its past payment) must not require history or a discount slot
    const ScriptProductData_ product("",
                                     {Cell_(Date_(2023, 2, 28)), Cell_(Date_(2023, 8, 28))},
                                     {"x = 10 pay PAYS x ON 2023-03-31", "y PAYS 3"});
    const double expected = 3.0 * std::exp(-RATE * YearFraction(evalDate, Date_(2023, 8, 28)));
    ASSERT_NEAR(MeanValue(product, FlatBsModel(), 64, evalDate), expected, 1e-12);
}

TEST(ScriptDelayedPaymentTest, TestUnsettledDelayedPaymentFromPastEventRejected) {
    const Date_ evalDate(2023, 3, 15);
    //  the event is history but its payment is still outstanding: settlement follows the
    //  payment date, so the amount must not be dropped as settled cash
    const ScriptProductData_ product("",
                                     {Cell_(Date_(2023, 2, 28)), Cell_(Date_(2023, 8, 28))},
                                     {"x = 10 pay PAYS x ON 2023-03-31", "y PAYS 3"});
    auto model = AAD::BlackScholes_<double>(100.0, 0.0, RATE, 0.0);
    try {
        static_cast<void>(PrepareScript(product, &model, ValuationOn(evalDate), {}));
        FAIL() << "expected UnsettledDelayedPayment";
    } catch (const Dal::Exception_& error) {
        ASSERT_NE(String_(error.what()).find("UnsettledDelayedPayment"), String_::npos);
    }
}

TEST(ScriptDelayedPaymentTest, TestTreeCompiledParityOnCallPayoff) {
    const Date_ evalDate(2023, 1, 28);
    const ScriptProductData_ product("", {Cell_(Date_(2023, 7, 28))}, {"pay PAYS MAX(SPOT() - 100, 0) ON 2023-08-28"});
    const size_t nPaths = 65536;
    const double treeValue = MeanValue(product, FlatBsModel(100.0, 0.2), nPaths, evalDate);
    MonteCarloSettings_ compiled;
    compiled.compiled_ = true;
    const double compiledValue =
        MCSimulation<double>(product, FlatBsModel(100.0, 0.2), nPaths, ValuationOn(evalDate), compiled).aggregated_ / static_cast<double>(nPaths);
    ASSERT_NEAR(treeValue, compiledValue, 1e-12);
    //  analytic: Black call at the event, discounted from the payment date
    const double tEvent = YearFraction(evalDate, Date_(2023, 7, 28));
    const double tPay = YearFraction(evalDate, Date_(2023, 8, 28));
    const double forward = 100.0 * std::exp(RATE * tEvent);
    const double expected = std::exp(-RATE * tPay) * Distribution::BlackOpt(forward, 0.2 * std::sqrt(tEvent), 100.0, OptionType_("CALL"));
    ASSERT_NEAR(treeValue, expected, 3e-3);
}

TEST(ScriptDelayedPaymentTest, TestAadDifferentiatesThroughDiscountFactor) {
    const Date_ evalDate(2023, 1, 28);
    const double tEvent = YearFraction(evalDate, Date_(2023, 7, 28));
    const double tPay = YearFraction(evalDate, Date_(2023, 8, 28));
    const ScriptProductData_ product("", {Cell_(Date_(2023, 7, 28))}, {"pay PAYS MAX(SPOT() - 100, 0) ON 2023-08-28"});
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto results = MCSimulation<AAD::Number_>(product, FlatBsModel(100.0, 0.0), 1024, ValuationOn(evalDate), simulation);
    const double pv = results.aggregated_ / 1024.0;
    //  vol = 0: the payoff is deterministic, so PV and risks are analytic
    const double payoff = std::max(100.0 * std::exp(RATE * tEvent) - 100.0, 0.0);
    const double expectedPv = std::exp(-RATE * tPay) * payoff;
    ASSERT_NEAR(pv, expectedPv, 1e-10);
    //  BS risk order: spot, vol, rate, div
    ASSERT_EQ(results.risks_.size(), 4u);
    const double expectedSpot = std::exp(-RATE * tPay) * std::exp(RATE * tEvent);
    ASSERT_NEAR(results.risks_[0], expectedSpot, 1e-10);
    //  d/dr [e^{-r T} max(S0 e^{r t} - K, 0)] = e^{-r T} S0 t e^{r t} - T * PV
    const double expectedRate = std::exp(-RATE * tPay) * 100.0 * tEvent * std::exp(RATE * tEvent) - tPay * expectedPv;
    ASSERT_NEAR(results.risks_[2], expectedRate, 1e-10);
}

TEST(ScriptDelayedPaymentTest, TestFuzzyAadDelayedDigitalIsStable) {
    const Date_ evalDate(2023, 1, 28);
    const double tEvent = YearFraction(evalDate, Date_(2023, 7, 28));
    const double tPay = YearFraction(evalDate, Date_(2023, 8, 28));
    const ScriptProductData_ product("", {Cell_(Date_(2023, 7, 28))}, {"IF SPOT() > 100 THEN pay PAYS 1 ON 2023-08-28 END"});
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.smooth_ = 0.05;
    const auto results = MCSimulation<AAD::Number_>(product, FlatBsModel(100.0, 0.2), 4096, ValuationOn(evalDate), simulation);
    const double pv = results.aggregated_ / 4096.0;
    const double totalVol = 0.2 * std::sqrt(tEvent);
    //  P(S_event > K) with the forward carrying the rate drift
    const double expected = std::exp(-RATE * tPay) * NCDF((RATE * tEvent - 0.5 * totalVol * totalVol) / totalVol);
    ASSERT_NEAR(pv, expected, 5e-3);
    for (const double risk : results.risks_)
        ASSERT_TRUE(std::isfinite(risk));
}

TEST(ScriptDelayedPaymentTest, TestGsrTowerProperty) {
    const Date_ evalDate(2023, 1, 28);
    const Date_ eventDate(2024, 1, 28);
    const Date_ paymentDate(2024, 7, 28);
    //  E[P(t,T)/N(t)] = P(0,T) for any volatility: the stochastic DF reprices the initial curve
    const ScriptProductData_ product("", {Cell_(eventDate)}, {"pay PAYS 1 ON " + Date::ToString(paymentDate)});
    const double value = MeanValue(product, GsrModel(0.01), 65536, evalDate);
    const double tPay = YearFraction(evalDate, paymentDate);
    //  logDF is piecewise linear between -0.03 (1y) and -0.06 (2y)
    const double expected = std::exp(-0.03 - 0.03 * (tPay - 1.0));
    ASSERT_NEAR(value, expected, 1e-3);
}

TEST(ScriptDelayedPaymentTest, TestLsmcDelayedPaymentScalesByDiscountFactor) {
    const Date_ evalDate(2023, 1, 28);
    const Date_ exerciseDate(2023, 4, 28);
    const Date_ maturity(2023, 7, 28);
    const Date_ latePayment(2023, 9, 28);
    //  A never-true exercise condition keeps every path's decision identical between the two
    //  products, so the LSMC driver's recorded, discount-scaled rows must satisfy the exact
    //  relation delayed = plain * P(maturity -> late payment) on the same paths
    const String_ exerciseBody = "EXERCISE MAX(100 - SPOT(), 0) IF 1 = 2";
    const ScriptProductData_ plain("", {Cell_(exerciseDate), Cell_(maturity)}, {exerciseBody, "pay PAYS MAX(100 - SPOT(), 0)"});
    const ScriptProductData_ delayed(
        "", {Cell_(exerciseDate), Cell_(maturity)}, {exerciseBody, "pay PAYS MAX(100 - SPOT(), 0) ON " + Date::ToString(latePayment)});
    const double plainValue = MeanValue(plain, FlatBsModel(100.0, 0.2), 4096, evalDate);
    const double delayedValue = MeanValue(delayed, FlatBsModel(100.0, 0.2), 4096, evalDate);
    ASSERT_NEAR(delayedValue, plainValue * std::exp(-RATE * YearFraction(maturity, latePayment)), 1e-12);
}

TEST(ScriptDelayedPaymentTest, TestLegacyPathRejectsUnpreparedDelayedPayment) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2023, 1, 28));
    ScriptProduct_ product({Cell_(Date_(2023, 2, 28))}, {"pay PAYS 100 ON 2023-08-28"});
    product.PreProcess(false, true);
    Scenario_<double> path(1);
    path[0].numeraire_ = 1.0;
    {
        auto evaluator = product.BuildEvaluator<double>();
        ASSERT_THROW(product.Evaluate(path, evaluator), Dal::Exception_);
    }
    {
        auto state = product.BuildEvalState<double>();
        ASSERT_THROW(product.Compile().Evaluate(path, state), Dal::Exception_);
    }
}

TEST(ScriptDelayedPaymentTest, TestHybridDeterministicRateDiscountsDelayedPayment) {
    const Date_ evalDate(2023, 1, 28);
    HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Handle_<HybridComponentData_>(new HybridDeterministicRateData_("usd", "USD", RATE)),
                            Handle_<HybridComponentData_>(new HybridBSEquityData_("eq", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.2, 0.01))};
    settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("corr", {"W_AAA"}, Matrix_<>(1, 1, 1.0)));
    const Handle_<ModelData_> model(new HybridModelData_("hybrid", settings));
    const ScriptProductData_ product("", {Cell_(Date_(2023, 7, 28))}, {"pay PAYS MAX(FIX(EQ[AAA]) - 100, 0) ON 2023-08-28"});
    const double tEvent = YearFraction(evalDate, Date_(2023, 7, 28));
    const double tPay = YearFraction(evalDate, Date_(2023, 8, 28));
    const double forward = 100.0 * std::exp((RATE - 0.01) * tEvent);
    const double expected = std::exp(-RATE * tPay) * Distribution::BlackOpt(forward, 0.2 * std::sqrt(tEvent), 100.0, OptionType_("CALL"));
    ASSERT_NEAR(MeanValue(product, model, 65536, evalDate), expected, 3e-3);
}
