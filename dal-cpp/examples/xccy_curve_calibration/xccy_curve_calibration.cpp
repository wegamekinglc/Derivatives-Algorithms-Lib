//
// Created by GitHub Copilot on 2026/6/6.
//

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/timer.hpp>

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
                Date::AddMonths(today, 36),
                Date::AddMonths(today, 60),
                Date::AddMonths(today, 120),
                Date::AddMonths(today, 240),
                Date::AddMonths(today, 360),
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
            auto usdBlock = MakeXccyBlock("usd_ois", "USD", today, 0.02);
            auto eurBlock = MakeXccyBlock("eur_ois", "EUR", today, 0.01);

            CrossCurrencyMarket_ retval(usdBlock, eurBlock, 1.10);
            if (basisRate != 0.0) {
                retval.SetBasisCurve(MakeFlatXccyCurve("usd_eur_basis", "USD", today, basisRate));
            }
            return retval;
        }

        CrossCurrencySwap_ MakeXccySwap(const Date_& today, double marketRate, int maturityMonths) {
            CrossCurrencyConvention_ convention;
            convention.initialNotionalExchange_ = true;
            convention.finalNotionalExchange_ = true;
            convention.spreadOnForeignLeg_ = true;
            convention.domesticIndex_ = MakeXccyIndex();
            convention.domesticLeg_ = MakeXccyLeg();
            convention.foreignIndex_ = MakeXccyIndex();
            convention.foreignLeg_ = MakeXccyLeg();
            return CrossCurrencySwap_(today,
                                      today,
                                      Date::AddMonths(today, maturityMonths),
                                      marketRate,
                                      CurrencyPair_(Ccy_("USD"), Ccy_("EUR")),
                                      110.0,
                                      100.0,
                                      convention);
        }
    } // namespace

    void PrintXccyCurveCalibrationExample(const Date_& today) {
        // Build a quote market with a basis curve to derive market-implied spreads
        const CrossCurrencyMarket_ quoteMarket = MakeXccyMarket(today, 0.0020);
        // Prototype swaps at various maturities to get market spreads
        const Vector_<int> maturities = {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 108, 120};
        Vector_<> marketSpreads;
        marketSpreads.reserve(maturities.size());
        for (int m : maturities) {
            const auto proto = MakeXccySwap(today, 0.0, m);
            marketSpreads.push_back((*proto.Precompute())(quoteMarket));
        }

        CrossCurrencyCalibrationSpec_ spec;
        spec.today_ = today;
        spec.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        spec.domesticCurveBlock_ = MakeXccyBlock("usd_ois", "USD", today, 0.02);
        spec.foreignCurveBlock_ = MakeXccyBlock("eur_ois", "EUR", today, 0.01);
        spec.fxSpot_ = 1.10;
        spec.knotDates_ = {
            Date::AddMonths(today, 6),
            Date::AddMonths(today, 12),
            Date::AddMonths(today, 24),
            Date::AddMonths(today, 60),
            Date::AddMonths(today, 120),
        };
        spec.instruments_ = {};
        // Build descriptive instrument names
        Vector_<String_> xccyNames;
        xccyNames.reserve(maturities.size());
        for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
            spec.instruments_.push_back(
                Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeXccySwap(today, marketSpreads[i], maturities[i]))));
            xccyNames.push_back(String_(std::string("XCCY Swap ") + std::to_string(maturities[i]) + "M"));
        }

        Timer_ timer;
        timer.Reset();
        const auto result = CalibrateCrossCurrencyMarket(spec);
        const double elapsedMs = timer.Elapsed<milliseconds>();

        std::cout << "\n"
                  << std::string(70, '=') << "\n"
                  << "  Cross-currency basis calibration example  ("
                  << spec.instruments_.size() << " instruments)\n"
                  << std::string(70, '=') << "\n\n";
        const Vector_<int> w = {26, 14, 14, 14};
        std::cout << std::left  << std::setw(w[0]) << "Instrument"
                  << std::right << std::setw(w[1]) << "Market(bp)"
                  << std::setw(w[2]) << "Model(bp)"
                  << std::setw(w[3]) << "Error(bp)" << '\n';
        std::cout << std::string(68, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(result.diagnostics_.instrumentNames_.size()); ++i) {
            std::cout << std::left  << std::setw(w[0]) << xccyNames[i].c_str()
                      << std::right << std::setw(w[1]) << result.diagnostics_.marketRates_[i] * 10000.0
                      << std::setw(w[2]) << result.diagnostics_.modelRates_[i] * 10000.0
                      << std::setw(w[3]) << result.diagnostics_.residuals_[i] * 10000.0 << '\n';
        }
        std::cout << std::string(68, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  FX spot: " << spec.fxSpot_
                  << "  |  max residual: "
                  << *std::max_element(result.diagnostics_.residuals_.begin(),
                                       result.diagnostics_.residuals_.end(),
                                       [](double a, double b) { return std::abs(a) < std::abs(b); })
                    * 10000.0 << " bp"
                  << "  |  elapsed: " << int(elapsedMs) << " ms"
                  << "\n\n";
    }
} // namespace Dal

int main() {
    RegisterAll_::Init();

    const Date_ today(2024, 1, 15);
    XGLOBAL::SetEvaluationDate(today);

    PrintXccyCurveCalibrationExample(today);

    return 0;
}

