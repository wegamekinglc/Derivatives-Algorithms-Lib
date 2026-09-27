//
// Created by Codex on 2026/9/27.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/platform/consts.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/event.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct BasketInputs_ {
        double spotA_ = 100.0;
        double spotB_ = 120.0;
        double volA_ = 0.20;
        double volB_ = 0.30;
        double rate_ = 0.05;
        double correlation_ = 0.40;
        double strike_ = 110.0;
        double maturity_ = 1.0;
    };

    Handle_<ModelData_> Model(const BasketInputs_& inputs) {
        CorrelatedBSSettings_ settings;
        settings.assets_ = {{"EQ[AAA]", inputs.spotA_, inputs.volA_, 0.0}, {"EQ[BBB]", inputs.spotB_, inputs.volB_, 0.0}};
        settings.rate_ = inputs.rate_;
        settings.correlations_ = Matrix_<>(2, 2, 0.0);
        settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
        settings.correlations_(0, 1) = settings.correlations_(1, 0) = inputs.correlation_;
        return Handle_<ModelData_>(new CorrelatedBSModelData_("two_equities", settings));
    }

    double ConditionalQuadrature(const BasketInputs_& inputs, int nodes) {
        const NormalExpectation_<> normal(nodes);
        const auto& x = normal.Abscissa();
        const auto& w = normal.Weight();
        const OptionType_ call("CALL");
        const double rootTime = std::sqrt(inputs.maturity_);
        const double conditionalVol = inputs.volB_ * rootTime * std::sqrt(1.0 - inputs.correlation_ * inputs.correlation_);
        double expectation = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            const double stockA =
                inputs.spotA_ * std::exp((inputs.rate_ - 0.5 * inputs.volA_ * inputs.volA_) * inputs.maturity_ + inputs.volA_ * rootTime * x[i]);
            const double forwardB =
                inputs.spotB_ *
                std::exp((inputs.rate_ - 0.5 * inputs.volB_ * inputs.volB_ * inputs.correlation_ * inputs.correlation_) * inputs.maturity_ +
                         inputs.volB_ * rootTime * inputs.correlation_ * x[i]);
            expectation += 0.5 * w[i] * Distribution::BlackOpt(forwardB, conditionalVol, 2.0 * inputs.strike_ - stockA, call);
        }
        return std::exp(-inputs.rate_ * inputs.maturity_) * expectation;
    }

    template <class T_> SimResults_ MonteCarlo(const ScriptProductData_& product, const BasketInputs_& inputs, size_t paths) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        return MCSimulation<T_>(product, Model(inputs), paths, {}, simulation);
    }

    double Price(const SimResults_& results, size_t paths) { return results.aggregated_ / static_cast<double>(paths); }

    void PrintRow(double correlation,
                  const std::string& method,
                  const std::string& work,
                  double pv,
                  double reference,
                  std::optional<double> deltaA,
                  std::optional<double> deltaB,
                  double milliseconds) {
        std::cout << std::setw(8) << std::left << std::fixed << std::setprecision(2) << correlation << std::setw(18) << method << std::setw(14)
                  << std::right << work << std::setprecision(6) << std::setw(14) << pv << std::setw(14) << pv - reference;
        if (deltaA)
            std::cout << std::setw(14) << *deltaA;
        else
            std::cout << std::setw(14) << "-";
        if (deltaB)
            std::cout << std::setw(14) << *deltaB;
        else
            std::cout << std::setw(14) << "-";
        std::cout << std::setw(12) << std::setprecision(3) << milliseconds << '\n';
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const Date_ today(2026, 9, 27);
    const Date_ maturity(2027, 9, 27);
    Global::Dates_::SetEvaluationDate(today);

    BasketInputs_ inputs;
    inputs.maturity_ = (maturity - today) / DAYS_PER_YEAR;
    // Named FIX observations read both assets from the same correlated path.
    const ScriptProductData_ product(
        "basket_call", {Cell_("STRIKE"), Cell_(maturity)},
        {String_(std::to_string(inputs.strike_).c_str()), "pay PAYS MAX(0.5 * FIX(EQ[AAA]) + 0.5 * FIX(EQ[BBB]) - STRIKE, 0)"});
    constexpr size_t N_PATHS = size_t{1} << 18;
    constexpr int N_NODES = 48;
    constexpr double BUMP = 0.01;
    const std::array<double, 4> correlations = {-0.60, 0.00, 0.40, 0.80};

    std::cout << "\nTwo-asset basket call: 0.5 * AAA + 0.5 * BBB, strike 110\n"
              << "Sobol paths: " << N_PATHS << ", finite-difference spot bump: " << BUMP << '\n'
              << std::setw(8) << std::left << "rho" << std::setw(18) << "Method" << std::setw(14) << std::right << "Work" << std::setw(14) << "PV"
              << std::setw(14) << "PV - ref" << std::setw(14) << "dPV/dS_A" << std::setw(14) << "dPV/dS_B" << std::setw(12) << "Time (ms)" << '\n'
              << std::string(108, '-') << '\n';

    double previousReference = -1.0;
    for (const double correlation : correlations) {
        inputs.correlation_ = correlation;
        Timer_ timer;
        const double reference = ConditionalQuadrature(inputs, N_NODES);
        auto up = inputs;
        auto down = inputs;
        up.spotA_ += BUMP;
        down.spotA_ -= BUMP;
        const double referenceDeltaA = (ConditionalQuadrature(up, N_NODES) - ConditionalQuadrature(down, N_NODES)) / (2.0 * BUMP);
        up = down = inputs;
        up.spotB_ += BUMP;
        down.spotB_ -= BUMP;
        const double referenceDeltaB = (ConditionalQuadrature(up, N_NODES) - ConditionalQuadrature(down, N_NODES)) / (2.0 * BUMP);
        const double referenceMs = timer.Elapsed<microseconds>() / 1000.0;

        timer.Reset();
        const auto plain = MonteCarlo<double>(product, inputs, N_PATHS);
        const double plainMs = timer.Elapsed<microseconds>() / 1000.0;
        const double plainPv = Price(plain, N_PATHS);

        timer.Reset();
        up = down = inputs;
        up.spotA_ += BUMP;
        down.spotA_ -= BUMP;
        const double fdDeltaA =
            (Price(MonteCarlo<double>(product, up, N_PATHS), N_PATHS) - Price(MonteCarlo<double>(product, down, N_PATHS), N_PATHS)) / (2.0 * BUMP);
        up = down = inputs;
        up.spotB_ += BUMP;
        down.spotB_ -= BUMP;
        const double fdDeltaB =
            (Price(MonteCarlo<double>(product, up, N_PATHS), N_PATHS) - Price(MonteCarlo<double>(product, down, N_PATHS), N_PATHS)) / (2.0 * BUMP);
        const double fdMs = timer.Elapsed<microseconds>() / 1000.0;

        timer.Reset();
        const auto aad = MonteCarlo<AAD::Number_>(product, inputs, N_PATHS);
        const double aadMs = timer.Elapsed<microseconds>() / 1000.0;
        const double aadPv = Price(aad, N_PATHS);
        const double aadDeltaA = aad["spot:EQ[AAA]"];
        const double aadDeltaB = aad["spot:EQ[BBB]"];
        const std::array<double, 7> values = {reference, plainPv, aadPv, fdDeltaA, fdDeltaB, aadDeltaA, aadDeltaB};
        REQUIRE(std::all_of(values.begin(), values.end(), [](double value) { return std::isfinite(value); }), "basket call result must be finite");
        REQUIRE(reference > previousReference && std::abs(plainPv - reference) < 0.20 && std::abs(aadPv - plainPv) < 1.0e-6 &&
                    std::abs(aadDeltaA - fdDeltaA) < 0.02 && std::abs(aadDeltaB - fdDeltaB) < 0.02,
                "basket call correlation sweep must agree with the independent reference and finite differences");
        previousReference = reference;

        PrintRow(correlation, "Cond. GH + Black", std::to_string(N_NODES) + " nodes", reference, reference, referenceDeltaA, referenceDeltaB,
                 referenceMs);
        PrintRow(correlation, "MC double", std::to_string(N_PATHS) + " paths", plainPv, reference, {}, {}, plainMs);
        PrintRow(correlation, "MC double + FD", "5 x " + std::to_string(N_PATHS), plainPv, reference, fdDeltaA, fdDeltaB, plainMs + fdMs);
        PrintRow(correlation, "MC AAD", std::to_string(N_PATHS) + " paths", aadPv, reference, aadDeltaA, aadDeltaB, aadMs);
        std::cout << std::string(108, '-') << '\n';
    }
    std::cout << "Reference: condition on AAA, then apply the Black call formula and normal quadrature.\n"
              << "FD: central spot bumps with the same Sobol paths; the sweep reuses the same sequence at each rho.\n"
              << "Times include each row's PV and deltas.\n";
    return 0;
}
