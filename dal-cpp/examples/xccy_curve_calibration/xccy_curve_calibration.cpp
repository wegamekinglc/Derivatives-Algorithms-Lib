//
// Created by GitHub Copilot on 2026/6/6.
//

#include <iomanip>
#include <iostream>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccymarket.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>

using namespace Dal;

namespace Dal {
    namespace {
        Handle_<DiscountCurve_> MakeFlatXccyCurve(const String_& name,
                                                  const String_& ccy,
                                                  const Date_& today,
                                                  double rate) {
            const Vector_<Date_> knots = {
                Date::AddMonths(today, 12),
                Date::AddMonths(today, 24),
                Date::AddMonths(today, 60),
            };
            const Vector_<> vals(knots.size(), rate);
            return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, vals, vals)));
        }

        Handle_<CurveBlock_> MakeXccyBlock(const String_& name,
                                           const String_& ccy,
                                           const Date_& today,
                                           double rate) {
            return Handle_<CurveBlock_>(new CurveBlock_(MakeFlatXccyCurve(name, ccy, today, rate)));
        }

        RateIndexConvention_ MakeXccyIndex() {
            RateIndexConvention_ retval;
            retval.useProjectionCurve_ = true;
            retval.forecastTenor_ = PeriodLength_("12M");
            retval.dayBasis_ = DayBasis_("ACT_365F");
            retval.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
            return retval;
        }

        RateLegConvention_ MakeXccyLeg() {
            RateLegConvention_ retval;
            retval.paymentFrequency_ = PeriodLength_("12M");
            retval.dayBasis_ = DayBasis_("ACT_365F");
            return retval;
        }

        CrossCurrencyMarket_ MakeXccyMarket(const Date_& today, double basisRate) {
            CrossCurrencyMarket_ retval;
            const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
            retval.SetCurveBlock(Ccy_("USD"), MakeXccyBlock("usd_ois", "USD", today, 0.02));
            retval.SetCurveBlock(Ccy_("EUR"), MakeXccyBlock("eur_ois", "EUR", today, 0.01));
            retval.SetFxSpot(pair, 1.10);
            if (basisRate != 0.0) {
                retval.SetBasisCurve(pair, MakeFlatXccyCurve("usd_eur_basis", "USD", today, basisRate));
            }
            return retval;
        }

        CrossCurrencySwap_ MakeXccySwap(const Date_& today, double marketRate) {
            CrossCurrencyConvention_ convention;
            convention.initialNotionalExchange_ = true;
            convention.finalNotionalExchange_ = true;
            convention.spreadOnForeignLeg_ = true;
            return CrossCurrencySwap_(today,
                                      today,
                                      Date::AddMonths(today, 12),
                                      marketRate,
                                      CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                      110.0,
                                      100.0,
                                      MakeXccyIndex(),
                                      MakeXccyLeg(),
                                      MakeXccyIndex(),
                                      MakeXccyLeg(),
                                      convention);
        }
    } // namespace

    void PrintXccyCurveCalibrationExample(const Date_& today) {
        const CrossCurrencyMarket_ quoteMarket = MakeXccyMarket(today, 0.0020);
        const auto prototype = MakeXccySwap(today, 0.0);
        const double marketSpread = (*prototype.Precompute())(quoteMarket);

        CrossCurrencyCalibrationSpec_ spec;
        spec.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        spec.domesticCurveBlock_ = MakeXccyBlock("usd_ois", "USD", today, 0.02);
        spec.foreignCurveBlock_ = MakeXccyBlock("eur_ois", "EUR", today, 0.01);
        spec.fxSpot_ = 1.10;
        spec.knotDates_ = {Date::AddMonths(today, 12)};
        spec.instruments_ = {Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeXccySwap(today, marketSpread)))};

        const auto result = CalibrateCrossCurrencyMarket(spec);

        std::cout << "\nCross-currency basis calibration example\n";
        std::cout << "----------------------------------------\n";
        std::cout << std::left << std::setw(14) << "Instrument"
                  << std::setw(12) << "Market(bp)"
                  << std::setw(12) << "Model(bp)"
                  << std::setw(12) << "Error(bp)" << '\n';
        for (int i = 0; i < static_cast<int>(result.diagnostics_.instrumentNames_.size()); ++i) {
            std::cout << std::setw(14) << result.diagnostics_.instrumentNames_[i]
                      << std::setw(12) << result.diagnostics_.marketRates_[i] * 10000.0
                      << std::setw(12) << result.diagnostics_.modelRates_[i] * 10000.0
                      << std::setw(12) << result.diagnostics_.residuals_[i] * 10000.0 << '\n';
        }
    }
} // namespace Dal

int main() {
    RegisterAll_::Init();

    const Date_ today(2024, 1, 15);
    XGLOBAL::SetEvaluationDate(today);

    PrintXccyCurveCalibrationExample(today);

    return 0;
}

