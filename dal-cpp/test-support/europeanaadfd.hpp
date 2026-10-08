//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

namespace Dal::AAD::Example {
    struct EuropeanThetaSettings_ {
        int gridPoints_ = 61;
        int ordinarySteps_ = 120;
    };

    struct EuropeanThetaRecording_ {
        std::array<Number_, 2> prices_;
        Vector_<SolveAccuracyEvent_> events_;
        double largestForwardError_ = 0.0;
    };

    namespace Detail {
        inline Vector_<> EuropeanGrid(int points) {
            Vector_<> result(points);
            for (int row = 0; row < points; ++row)
                result[row] = 400.0 * row / (points - 1);
            return result;
        }

        inline SampledThetaStepBindings_ EuropeanBindings(const Vector_<>& grid, const std::array<Number_, 3>& parameters) {
            const auto& rate = parameters[0];
            const auto& sigma = parameters[1];
            const auto& strike = parameters[2];
            Number_ zero = 0.0 * strike;
            const int points = static_cast<int>(grid.size());
            SampledThetaStepBindings_ result;
            result.rates_ = Vector_<Number_>(points - 2, rate);
            result.drifts_ = Vector_<Number_>(points - 2);
            result.variances_ = Vector_<Number_>(points - 2);
            result.oldValues_ = Matrix_<Number_>(points, 2, zero);
            result.externalValues_ = Matrix_<Number_>(2, 2, zero);
            for (int row = 1; row < points - 1; ++row) {
                result.drifts_[row - 1] = (rate - 0.02) * grid[row];
                result.variances_[row - 1] = sigma * sigma * grid[row] * grid[row];
            }
            for (int row = 0; row < points; ++row) {
                if (grid[row] > Value(strike))
                    result.oldValues_(row, 0) = grid[row] - strike;
                if (grid[row] < Value(strike))
                    result.oldValues_(row, 1) = strike - grid[row];
            }
            return result;
        }

        inline void SetEuropeanBoundaries(SampledThetaStepBindings_* bindings, const std::array<Number_, 3>& parameters, double tau) {
            Number_ discountedStrike = parameters[2] * exp(-parameters[0] * tau);
            bindings->externalValues_(0, 1) = discountedStrike;
            bindings->externalValues_(1, 0) = 400.0 * std::exp(-0.02 * tau) - discountedStrike;
        }
    } // namespace Detail

    inline EuropeanThetaRecording_
    RecordEuropeanOptions(RecordingScope_* scope, const EuropeanThetaSettings_& settings, const std::array<Number_, 3>& parameters) {
        REQUIRE(settings.gridPoints_ >= 5 && (settings.gridPoints_ - 1) % 4 == 0,
                "European theta example requires spot 100 on a uniform [0,400] grid");
        REQUIRE(settings.ordinarySteps_ >= 2 && settings.ordinarySteps_ <= std::numeric_limits<int>::max() - 2,
                "European theta example requires a representable schedule of at least two ordinary time steps");
        PDE::SampledThetaStepInputs_ numeric;
        numeric.x_ = Detail::EuropeanGrid(settings.gridPoints_);
        numeric.externalBoundaries_ = {true, true};
        auto active = Detail::EuropeanBindings(numeric.x_, parameters);
        EuropeanThetaRecording_ result;
        result.events_.reserve(settings.ordinarySteps_ + 2);
        for (int step = 0; step < settings.ordinarySteps_ + 2; ++step) {
            const bool damping = step < 4;
            numeric.dt_ = (damping ? 0.5 : 1.0) / settings.ordinarySteps_;
            numeric.theta_ = damping ? 1.0 : 0.5;
            const double tau = (damping ? 0.5 * (step + 1) : step - 1.0) / settings.ordinarySteps_;
            Detail::SetEuropeanBoundaries(&active, parameters, tau);
            auto rolled = SampledThetaStepWithAccuracy(scope, numeric, active, LinearSolveAccuracyPolicy_{1e-12, 1e-12});
            result.events_.push_back(rolled.event_);
            for (double error : rolled.diagnostics_.forwardBackwardErrors_)
                result.largestForwardError_ = std::max(result.largestForwardError_, error);
            active.oldValues_ = std::move(rolled.solution_);
        }
        const int spotRow = (settings.gridPoints_ - 1) / 4;
        result.prices_ = {active.oldValues_(spotRow, 0), active.oldValues_(spotRow, 1)};
        return result;
    }
} // namespace Dal::AAD::Example
