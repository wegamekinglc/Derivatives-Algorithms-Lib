#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

#include <dal/model/dupirequotesecondorder.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/initall.hpp>

#include "../../floatformat.hpp"

namespace {
    class FlatBase_ final : public Dal::AAD::IVS_ {
    public:
        FlatBase_() : IVS_(100.0, 0.05, 0.02) {}
        double ImpliedVol(double, double) const override { return 0.20; }
    };
} // namespace

int main() {
    using namespace Dal;
    RegisterAll_::Init(1);

    Matrix_<> spreads(3, 2, 0.0); // rows are strikes, columns are maturities
    spreads(0, 0) = 0.008;
    spreads(0, 1) = 0.012;
    spreads(1, 1) = 0.004;
    spreads(2, 0) = 0.006;
    spreads(2, 1) = 0.009;
    const Vector_<> calibrationSpots{60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0};
    const DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, spreads, calibrationSpots, 10.0, {0.5, 1.0}, 0.5};
    const auto calibration = CalibrateDupireWithRisk(FlatBase_(), inputs);
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(calibration, {105.0, 1.0, "EQ", Ccy_("USD")}, {0.8, 81, 64});
    const auto& axis = plan.InputAxis();
    REQUIRE(axis.size() == 6, "Expected six registered Dupire quote coordinates");

    Matrix_<> singleDirection(1, axis.size(), 0.0);
    singleDirection(0, 3) = 1.0;
    const auto single = EvaluateDupireQuoteSecondOrder(plan, axis, singleDirection);

    Matrix_<> directions(3, axis.size(), 0.0);
    directions(0, 3) = 1.0; // K=105, T=1.2
    directions(1, 2) = 1.0; // K=105, T=0.4
    constexpr std::array<double, 6> MIXED{0.5, -0.25, -0.5, 1.0, 0.25, -0.5};
    std::copy(MIXED.begin(), MIXED.end(), directions.Row(2).begin());
    const auto result = EvaluateDupireQuoteSecondOrder(plan, axis, directions);
    const auto& risk = result.Risk();
    const auto& hvp = risk.HessianProducts();
    const auto* pointwise = single.Risk().Pointwise();
    const auto* shared = risk.Shared();
    REQUIRE(pointwise && shared, "Expected pointwise AAD for one direction and shared AAD for three");

    // Reference values for the registered fixed grid from independent high-precision repricing.
    REQUIRE(std::abs(risk.Value() - 7.2746480967500065) < 2e-7, "Dupire PV differs from the fixed-grid reference");
    REQUIRE(std::abs(hvp(0, 3) + 153.37815956069173) < 1e-5, "Dupire Volga differs from the fixed-grid reference");
    REQUIRE(std::abs(hvp(0, 2) - 49.77890609287156) < 1e-5, "Dupire cross-tenor risk differs from the fixed-grid reference");
    REQUIRE(std::abs(hvp(0, 1) - 4.798956487024745) < 1e-5, "Dupire cross-strike risk differs from the fixed-grid reference");
    REQUIRE(std::abs(pointwise->Value() - risk.Value()) < 2e-7, "Pointwise and shared Dupire PV differ");

    std::cout << "\nFixed-plan Dupire quote Greeks\n"
              << "PV (USD): " << ExampleFloat(risk.Value(), 8) << "   Quote unit: " << result.QuoteUnit() << '\n'
              << "One direction: " << pointwise->Execution().method_ << "   Three directions: " << shared->Execution().method_ << "\n\n"
              << std::left << std::setw(10) << "Quote" << std::right << std::setw(8) << "Strike" << std::setw(8) << "Years" << std::setw(10)
              << "Spread" << std::setw(15) << "Vega" << std::setw(15) << "H e3" << std::setw(15) << "H e2" << std::setw(15) << "H mixed" << '\n';
    for (int col = 0; col < directions.Cols(); ++col) {
        const auto& quote = result.InputAxis()[col];
        REQUIRE(std::abs(pointwise->Gradient()[col] - risk.Gradient()[col]) < 2e-6, "Pointwise and shared Dupire Vega differ");
        REQUIRE(std::abs(pointwise->HessianProducts()(0, col) - hvp(0, col)) < 1e-5, "Pointwise and shared Dupire HVP differ");
        std::cout << std::left << std::setw(10) << quote.id_ << std::right << std::setw(8) << ExampleFloat(quote.strike_, 0) << std::setw(8)
                  << ExampleFloat(quote.maturity_, 1) << std::setw(10) << ExampleFloat(risk.Point()[col], 3) << std::setw(15)
                  << ExampleFloat(risk.Gradient()[col], 8) << std::setw(15) << ExampleFloat(hvp(0, col), 8) << std::setw(15)
                  << ExampleFloat(hvp(1, col), 8) << std::setw(15) << ExampleFloat(hvp(2, col), 8) << '\n';
    }

    for (int row = 0; row < directions.Rows(); ++row) {
        const auto seeds = directions.Row(row);
        const double expected = std::inner_product(seeds.begin(), seeds.end(), risk.Gradient().begin(), 0.0);
        REQUIRE(std::abs(risk.DirectionalDerivatives()[row] - expected) < 2e-6, "Dupire directional derivative differs from g^T v");
    }
    std::cout << "\nVolga (quote:3): " << ExampleFloat(hvp(0, 3), 8) << '\n'
              << "Cross-tenor (quote:3, quote:2): " << ExampleFloat(hvp(0, 2), 8) << '\n'
              << "Cross-strike (quote:3, quote:1): " << ExampleFloat(hvp(0, 1), 8) << '\n';
}
