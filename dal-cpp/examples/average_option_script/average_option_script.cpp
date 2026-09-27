//
// Created by Codex on 2026/9/27.
//

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/model/blackscholes.hpp>
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
    struct AverageInputs_ {
        double spot_ = 100.0;
        double vol_ = 0.20;
        double rate_ = 0.05;
        double strike_ = 100.0;
        Date_ today_ = Date_(2026, 9, 27);
        std::array<Date_, 4> observations_ = {Date_(2026, 12, 27), Date_(2027, 3, 27), Date_(2027, 6, 27), Date_(2027, 9, 27)};
    };

    ScriptProductData_ Product(const AverageInputs_& inputs) {
        Vector_<Cell_> dates{Cell_("STRIKE")};
        Vector_<String_> events;
        events.emplace_back(std::to_string(inputs.strike_).c_str());
        for (size_t i = 0; i < inputs.observations_.size(); ++i) {
            dates.emplace_back(inputs.observations_[i]);
            events.emplace_back(i + 1 == inputs.observations_.size() ? "APPEND(fixings, SPOT()) pay PAYS MAX(AVERAGE(fixings) - STRIKE, 0)"
                                                                     : "APPEND(fixings, SPOT())");
        }
        return {"average_call", dates, events};
    }

    std::array<double, 4> StepYears(const AverageInputs_& inputs) {
        std::array<double, 4> steps{};
        Date_ previous = inputs.today_;
        for (size_t i = 0; i < steps.size(); ++i) {
            steps[i] = (inputs.observations_[i] - previous) / DAYS_PER_YEAR;
            previous = inputs.observations_[i];
        }
        return steps;
    }

    double ConditionalQuadrature(const AverageInputs_& inputs, int nodes) {
        const auto steps = StepYears(inputs);
        const NormalExpectation_<> normal(nodes);
        const auto& x = normal.Abscissa();
        const auto& w = normal.Weight();
        const OptionType_ call("CALL");
        const double drift = inputs.rate_ - 0.5 * inputs.vol_ * inputs.vol_;
        const double discount = std::exp(-inputs.rate_ * (inputs.observations_.back() - inputs.today_) / DAYS_PER_YEAR);
        double expectation = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            const double first = inputs.spot_ * std::exp(drift * steps[0] + inputs.vol_ * std::sqrt(steps[0]) * x[i]);
            for (size_t j = 0; j < x.size(); ++j) {
                const double second = first * std::exp(drift * steps[1] + inputs.vol_ * std::sqrt(steps[1]) * x[j]);
                for (size_t k = 0; k < x.size(); ++k) {
                    const double third = second * std::exp(drift * steps[2] + inputs.vol_ * std::sqrt(steps[2]) * x[k]);
                    const double forwardLast = third * std::exp(inputs.rate_ * steps[3]);
                    const double strikeLast = 4.0 * inputs.strike_ - first - second - third;
                    expectation +=
                        w[i] * w[j] * w[k] * 0.25 * Distribution::BlackOpt(forwardLast, inputs.vol_ * std::sqrt(steps[3]), strikeLast, call);
                }
            }
        }
        return discount * expectation;
    }

    template <class T_> SimResults_ MonteCarlo(const ScriptProductData_& product, const AverageInputs_& inputs, size_t paths) {
        const Handle_<ModelData_> model(new BSModelData_("equity", inputs.spot_, inputs.vol_, inputs.rate_, 0.0));
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        return MCSimulation<T_>(product, model, paths, {}, simulation);
    }

    double Price(const SimResults_& results, size_t paths) { return results.aggregated_ / static_cast<double>(paths); }

    void PrintRow(const std::string& method, const std::string& work, double pv, double reference, std::optional<double> delta, double milliseconds) {
        std::cout << std::setw(18) << std::left << method << std::setw(14) << std::right << work << std::fixed << std::setprecision(6)
                  << std::setw(14) << pv << std::setw(14) << pv - reference;
        if (delta)
            std::cout << std::setw(14) << *delta;
        else
            std::cout << std::setw(14) << "-";
        std::cout << std::setw(12) << std::setprecision(3) << milliseconds << '\n';
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const AverageInputs_ inputs;
    Global::Dates_::SetEvaluationDate(inputs.today_);
    const auto product = Product(inputs);
    constexpr size_t N_PATHS = size_t{1} << 18;
    constexpr int N_NODES = 48;
    constexpr double BUMP = 0.01;
    Timer_ timer;

    const double reference = ConditionalQuadrature(inputs, N_NODES);
    auto up = inputs;
    auto down = inputs;
    up.spot_ += BUMP;
    down.spot_ -= BUMP;
    const double referenceDelta = (ConditionalQuadrature(up, N_NODES) - ConditionalQuadrature(down, N_NODES)) / (2.0 * BUMP);
    const double referenceMs = timer.Elapsed<microseconds>() / 1000.0;

    timer.Reset();
    const auto plain = MonteCarlo<double>(product, inputs, N_PATHS);
    const double plainMs = timer.Elapsed<microseconds>() / 1000.0;
    const double plainPv = Price(plain, N_PATHS);

    timer.Reset();
    const double fdDelta =
        (Price(MonteCarlo<double>(product, up, N_PATHS), N_PATHS) - Price(MonteCarlo<double>(product, down, N_PATHS), N_PATHS)) / (2.0 * BUMP);
    const double fdMs = timer.Elapsed<microseconds>() / 1000.0;

    timer.Reset();
    const auto aad = MonteCarlo<AAD::Number_>(product, inputs, N_PATHS);
    const double aadMs = timer.Elapsed<microseconds>() / 1000.0;
    const double aadPv = Price(aad, N_PATHS);
    const double aadDelta = aad["spot"];
    REQUIRE(std::isfinite(reference) && std::isfinite(plainPv) && std::isfinite(aadPv) && std::isfinite(fdDelta) && std::isfinite(aadDelta),
            "average call result must be finite");
    REQUIRE(std::abs(plainPv - reference) < 0.20 && std::abs(aadPv - plainPv) < 1.0e-6 && std::abs(aadDelta - fdDelta) < 0.02,
            "average call Monte Carlo and AAD must agree with the independent reference and finite difference");

    std::cout << "\nFour-fixing arithmetic-average call, strike 100\n"
              << "Sobol paths: " << N_PATHS << ", finite-difference spot bump: " << BUMP << '\n'
              << std::setw(18) << std::left << "Method" << std::setw(14) << std::right << "Work" << std::setw(14) << "PV" << std::setw(14)
              << "PV - ref" << std::setw(14) << "dPV/dS0" << std::setw(12) << "Time (ms)" << '\n'
              << std::string(86, '-') << '\n';
    PrintRow("Cond. GH + Black", std::to_string(N_NODES) + "^3 nodes", reference, reference, referenceDelta, referenceMs);
    PrintRow("MC double", std::to_string(N_PATHS) + " paths", plainPv, reference, {}, plainMs);
    PrintRow("MC double + FD", "3 x " + std::to_string(N_PATHS), plainPv, reference, fdDelta, plainMs + fdMs);
    PrintRow("MC AAD", std::to_string(N_PATHS) + " paths", aadPv, reference, aadDelta, aadMs);
    std::cout << std::string(86, '-') << "\n"
              << "Reference: condition on the first three fixings, then apply the Black call formula and normal quadrature.\n"
              << "FD: central spot bumps with the same Sobol paths; times include each row's PV and delta.\n";
    return 0;
}
