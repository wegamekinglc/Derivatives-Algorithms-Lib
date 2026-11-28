#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/math/aad/secondorder.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/platform/initall.hpp>

#include "../../floatformat.hpp"

namespace {
    using namespace Dal;

    constexpr double STRIKE = 100.0;
    constexpr double MATURITY = 1.7;
    constexpr double RATE = 0.03;
    constexpr double DIVIDEND = 0.01;

    struct Greeks_ {
        double price_, delta_, vega_, gamma_, vanna_, volga_;
    };

    Greeks_ AnalyticGreeks(double spot, double vol) {
        const double rootTime = std::sqrt(MATURITY);
        const double discountedSpot = spot * std::exp(-DIVIDEND * MATURITY);
        const double d1 = (std::log(spot / STRIKE) + (RATE - DIVIDEND + 0.5 * vol * vol) * MATURITY) / (vol * rootTime);
        const double d2 = d1 - vol * rootTime;
        const double delta = std::exp(-DIVIDEND * MATURITY) * Dal::NCDF(d1);
        const double vega = discountedSpot * Dal::NPDF(d1) * rootTime;
        return {discountedSpot * Dal::NCDF(d1) - STRIKE * std::exp(-RATE * MATURITY) * Dal::NCDF(d2),
                delta,
                vega,
                vega / (spot * spot * vol * MATURITY),
                -vega * d2 / (spot * vol * rootTime),
                vega * d1 * d2 / vol};
    }

    void PrintComparison(const char* label, double aad, double analytic) {
        REQUIRE(std::abs(aad - analytic) < 1e-9, std::string("Second-order Black-Scholes result differs for ") + label);
        std::cout << std::left << std::setw(24) << label << std::right << std::setw(16) << ExampleFloat(aad, 8) << std::setw(16)
                  << ExampleFloat(analytic, 8) << '\n';
    }
} // namespace

int main() {
    using namespace Dal;
    RegisterAll_::Init(1);

    const Vector_<> point{103.0, 0.24}; // spot, decimal volatility
    Matrix_<> directions(3, 2, 0.0);
    directions(0, 0) = 1.0;
    directions(1, 1) = 1.0;
    directions(2, 0) = -2.0;
    directions(2, 1) = 0.5;

    const auto price = [](const Vector_<AAD::SecondOrderNumber_>& x) -> AAD::SecondOrderNumber_ {
        const auto& spot = x[0];
        const auto& vol = x[1];
        const AAD::SecondOrderNumber_ totalVol = vol * std::sqrt(MATURITY);
        const AAD::SecondOrderNumber_ d1 = (log(spot / STRIKE) + (RATE - DIVIDEND + 0.5 * vol * vol) * MATURITY) / totalVol;
        const AAD::SecondOrderNumber_ d2 = d1 - totalVol;
        return spot * std::exp(-DIVIDEND * MATURITY) * NCDF(d1) - STRIKE * std::exp(-RATE * MATURITY) * NCDF(d2);
    };
    const auto risk = AAD::EvaluateSecondOrder(price, point, directions);
    const auto analytic = AnalyticGreeks(point[0], point[1]);
    const auto& hvp = risk.HessianProducts();

    std::cout << "\nBlack-Scholes call: native second-order AAD\n"
              << std::left << std::setw(24) << "Quantity" << std::right << std::setw(16) << "AAD" << std::setw(16) << "Analytic" << '\n';
    PrintComparison("PV", risk.Value(), analytic.price_);
    PrintComparison("Delta", risk.Gradient()[0], analytic.delta_);
    PrintComparison("Vega", risk.Gradient()[1], analytic.vega_);
    PrintComparison("Gamma", hvp(0, 0), analytic.gamma_);
    PrintComparison("Vanna", hvp(0, 1), analytic.vanna_);
    PrintComparison("Vanna (vol basis)", hvp(1, 0), analytic.vanna_);
    PrintComparison("Volga", hvp(1, 1), analytic.volga_);

    const auto& slope = risk.DirectionalDerivatives();
    PrintComparison("g^T (-2, 0.5)", slope[2], -2.0 * analytic.delta_ + 0.5 * analytic.vega_);
    PrintComparison("H (-2, 0.5), spot", hvp(2, 0), -2.0 * analytic.gamma_ + 0.5 * analytic.vanna_);
    PrintComparison("H (-2, 0.5), vol", hvp(2, 1), -2.0 * analytic.vanna_ + 0.5 * analytic.volga_);
    std::cout << "\nMethod: " << risk.Execution().method_ << ", recordings: " << risk.Execution().recordings_
              << ", reverse sweeps: " << risk.Execution().reverseSweeps_ << '\n';
}
