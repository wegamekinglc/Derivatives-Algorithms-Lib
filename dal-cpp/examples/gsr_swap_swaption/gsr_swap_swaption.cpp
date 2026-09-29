//
// Created by Codex on 2026/9/29.
//

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/platform/consts.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/dateincrement.hpp>

#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 9, 28);
    const Date_ EXPIRY(2027, 9, 28);
    const Date_ FIRST_FLOAT_COUPON(2027, 12, 28);
    const Date_ FIRST_COUPON(2028, 3, 28);
    const Date_ THIRD_FLOAT_COUPON(2028, 6, 28);
    const Date_ MATURITY(2028, 9, 28);
    constexpr double INPUT_RATE = 0.025;
    constexpr double STRIKE = 0.03;
    constexpr int PATHS = 1 << 16;

    double Price(const Handle_<Script::ScriptProductData_>& product, const Handle_<ModelData_>& model) {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = TODAY;
        Script::MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        return ValueByMonteCarlo(product, model, PATHS, valuation, simulation).at("PV");
    }

    void PrintScript(const char* name, const Handle_<Script::ScriptProductData_>& product) {
        std::string tree = DebugScriptProductTree(product, true, 100).c_str();
        while (!tree.empty() && tree.back() == '\n')
            tree.pop_back();
        std::cout << "### " << name << "\n\n```text\n" << tree << "\n```\n\n";
    }

    Handle_<Script::ScriptProductData_> StandardPayerSwap() {
        const std::array<Date_, 5> dates{EXPIRY, FIRST_FLOAT_COUPON, FIRST_COUPON, THIRD_FLOAT_COUPON, MATURITY};
        Vector_<Cell_> eventDates{Cell_("STRIKE")};
        Vector_<String_> scripts{String_(std::to_string(STRIKE))};
        for (size_t i = 1; i < dates.size(); ++i) {
            const String_ start = Date::ToString(dates[i - 1]);
            const String_ fixing = Date::ToString(Date::NBusDays(2, Holidays::None())->BackFrom(dates[i - 1]));
            const String_ floating =
                "(" + String_(std::to_string(dates[i] - dates[i - 1])) + ".0 / 360.0) * FIX(IR[USD,LIBOR_3M_CME," + start + "]," + fixing + ")";
            const String_ fixed = i % 2 == 0 ? " - 0.5 * STRIKE" : "";
            eventDates.push_back(Cell_(dates[i]));
            scripts.push_back("pay PAYS " + floating + fixed);
        }
        return NewScriptProduct("payer_swap", eventDates, scripts);
    }

    double StaticSwapPrice(const Handle_<DiscountCurve_>& discountCurve) {
        FixedFloatTradeTerms_ terms;
        terms.notional_ = 1.0;
        terms.contractRate_ = STRIKE;
        terms.payFixed_ = true;
        terms.fixedLeg_.paymentFrequency_ = PeriodLength_("6M");
        terms.fixedLeg_.dayBasis_ = DayBasis_("30_360");
        terms.floatLeg_.paymentFrequency_ = PeriodLength_("3M");
        terms.floatLeg_.dayBasis_ = DayBasis_("ACT_360");
        terms.floatIndex_.forecastTenor_ = PeriodLength_("3M");
        terms.floatIndex_.fixingLag_ = 2;
        terms.floatIndex_.dayBasis_ = DayBasis_("ACT_360");
        terms.fixingIdentity_ = {"USD-LIBOR-3M", 11, 0};
        terms.forecastComponentKey_ = "curve";
        terms.discountComponentKey_ = "curve";
        const RateTradeDefinition_ trade{"forward_payer_swap", RateInstrumentType_("IRS"), TODAY, EXPIRY, MATURITY,
                                         Ccy_("USD"),          IrsTradeTerms_{terms}};
        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(TODAY, 9, 0);
        market.resultCurrency_ = Ccy_("USD");
        market.curveComponents_["curve"] = discountCurve;
        const auto result = PriceRateTrade(trade, market);
        REQUIRE(result.succeeded_, "Static IRS pricing failed: " + result.error_);
        return result.pv_;
    }
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(TODAY);

    const std::array<Date_, 4> nodes{TODAY, EXPIRY, FIRST_COUPON, MATURITY};
    Vector_<Date_> nodeDates(nodes.begin(), nodes.end());
    Vector_<> logDiscountFactors;
    for (const auto& date : nodes)
        logDiscountFactors.push_back(-INPUT_RATE * (date - TODAY) / DAYS_PER_YEAR);

    const auto discountCurve = DiscountLogDFNew("input_usd_ois", "USD", nodeDates, logDiscountFactors);
    const auto yieldCurve = CurveBlockNew(discountCurve);
    const auto curve = NewGSRCurveDataFromYieldCurve("gsr_curve", *yieldCurve, TODAY, nodeDates, {});
    const auto vol = NewGSRVolData("gsr_vol", {TODAY}, {0.02}, {TODAY}, {1.0});
    const auto model = NewGSRModelData("gsr", curve, vol);

    std::cout << "# GSR swap and swaption example\n\n"
              << "The GSR curve snapshots the input USD OIS yield curve; g and H are supplied, not calibrated.\n\n"
              << std::left << std::setw(15) << "Curve node" << std::right << std::setw(17) << "Input DF" << std::setw(17) << "GSR P(0,T)" << '\n'
              << std::string(49, '-') << '\n';
    std::cout << std::fixed << std::setprecision(9);
    for (size_t i = 1; i < nodes.size(); ++i) {
        const String_ maturity = Date::ToString(nodes[i]);
        const auto bond = NewScriptProduct("bond", {Cell_(TODAY)}, {"pay PAYS FIX(IR[USD,DF," + maturity + "])"});
        const double inputDf = (*discountCurve)(TODAY, nodes[i]);
        const double modelDf = Price(bond, model);
        REQUIRE(std::abs(modelDf - inputDf) < 1e-10, "GSR did not fit the input discount curve at a node");
        std::cout << std::left << std::setw(15) << maturity.c_str() << std::right << std::setw(17) << inputDf << std::setw(17) << modelDf << '\n';
    }

    std::cout << std::string(49, '-') << "\n\n"
              << std::left << std::setw(15) << "g knot" << std::right << std::setw(12) << "g" << "  " << std::left << std::setw(15) << "H knot"
              << std::right << std::setw(12) << "H" << '\n'
              << std::string(56, '-') << '\n'
              << std::left << std::setw(15) << Date::ToString(TODAY).c_str() << std::right << std::setw(12) << "0.020000" << "  " << std::left
              << std::setw(15) << Date::ToString(TODAY).c_str() << std::right << std::setw(12) << "1.000000" << '\n'
              << std::string(56, '-') << "\n\n";

    // The delivered swap pays four 3M ACT/360 Libor coupons and two 6M 30/360 fixed coupons.
    const String_ annuity = "0.5 * FIX(IR[USD,DF,2028-03-28]) + 0.5 * FIX(IR[USD,DF,2028-09-28])";
    const String_ swapValue = "(FIX(IR[USD,SWAP,1Y,2027-09-28]) - STRIKE) * (" + annuity + ")";
    const auto swap = StandardPayerSwap();
    const auto swaption = NewScriptProduct("payer_swaption", {Cell_("STRIKE"), Cell_(EXPIRY)}, {"0.03", "pay PAYS MAX(" + swapValue + ", 0)"});

    PrintScript("Standard forward payer swap", swap);
    PrintScript("Cash-settled European payer swaption", swaption);

    const double swapPv = Price(swap, model);
    const double swaptionPv = Price(swaption, model);
    REQUIRE(std::isfinite(swapPv) && swapPv < 0.0 && swaptionPv > 0.0, "Unexpected GSR swap or swaption price");
    const double staticSwapPv = StaticSwapPrice(discountCurve);
    const double curveIdentity = (*discountCurve)(TODAY, EXPIRY) - (*discountCurve)(TODAY, MATURITY) -
                                 STRIKE * 0.5 * ((*discountCurve)(TODAY, FIRST_COUPON) + (*discountCurve)(TODAY, MATURITY));
    REQUIRE(std::abs(staticSwapPv - curveIdentity) < 1e-10, "Static IRS PV differs from the yield-curve identity");
    REQUIRE(std::abs(swapPv - staticSwapPv) < 2e-4, "Standard swap GSR PV differs from its input-curve PV");
    std::cout << "Forward start " << Date::ToString(EXPIRY).c_str() << "; unit notional; fixed rate " << std::setprecision(2) << STRIKE
              << std::setprecision(9) << ". The swap pays coupons on their scheduled dates; the swaption settles in cash at expiry.\n\n"
              << std::left << std::setw(41) << "Product" << std::right << std::setw(23) << "GSR Monte Carlo PV" << std::setw(23)
              << "Static YieldCurve PV" << std::setw(18) << "Difference" << '\n'
              << std::string(105, '-') << '\n'
              << std::left << std::setw(41) << "Standard forward payer swap" << std::right << std::setw(23) << swapPv << std::setw(23) << staticSwapPv
              << std::setw(18) << swapPv - staticSwapPv << '\n'
              << std::left << std::setw(41) << "Cash-settled European payer swaption" << std::right << std::setw(23) << swaptionPv << std::setw(23)
              << "N/A" << std::setw(18) << "N/A" << '\n'
              << std::string(105, '-') << '\n';
}
