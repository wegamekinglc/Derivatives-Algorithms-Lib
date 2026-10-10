//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

namespace Dal::AAD {
    struct EuropeanThetaSettings_ {
        int gridPoints_ = 61;
        int ordinarySteps_ = 120;
        double upper_ = 400.0;
        std::optional<int> spotIndex_ = std::nullopt;
        double expiry_ = 1.0;
        double dividendYield_ = 0.02;
        LinearSolveAccuracyPolicy_ accuracy_ = {1e-12, 1e-12};
    };

    struct EuropeanThetaRecording_ {
        std::array<Number_, 2> prices_;
        Vector_<SolveAccuracyEvent_> events_;
        double largestForwardError_ = 0.0;
        Matrix_<> forwardBackwardErrors_;
    };

    namespace EuropeanThetaDetail {
        inline void ValidatePhysicalSettings(const EuropeanThetaSettings_& settings) {
            REQUIRE(std::isfinite(settings.upper_) && settings.upper_ > 0.0, "EuropeanPdeRisk: upper must be positive and finite");
            const double spacing = settings.upper_ / (settings.gridPoints_ - 1);
            REQUIRE(std::isfinite(spacing) && spacing > 0.0 && settings.upper_ - spacing < settings.upper_,
                    "EuropeanPdeRisk: grid spacing must be positive, finite and representably distinct");
            REQUIRE(std::isfinite(settings.expiry_) && settings.expiry_ > 0.0, "EuropeanPdeRisk: expiry must be positive and finite");
            REQUIRE(0.5 * (settings.expiry_ / settings.ordinarySteps_) > 0.0, "EuropeanPdeRisk: half time step must be representably positive");
            REQUIRE(std::isfinite(settings.dividendYield_), "EuropeanPdeRisk: dividend_yield must be finite");
            REQUIRE(std::isfinite(settings.accuracy_.forwardBackwardErrorLimit_) && settings.accuracy_.forwardBackwardErrorLimit_ >= 0.0,
                    "EuropeanPdeRisk: forward_backward_error_limit must be finite and nonnegative");
            REQUIRE(std::isfinite(settings.accuracy_.transposeBackwardErrorLimit_) && settings.accuracy_.transposeBackwardErrorLimit_ >= 0.0,
                    "EuropeanPdeRisk: transpose_backward_error_limit must be finite and nonnegative");
        }
    } // namespace EuropeanThetaDetail

    [[nodiscard]] inline EuropeanThetaSettings_ ResolveEuropeanThetaSettings(const EuropeanThetaSettings_& settings) {
        REQUIRE(settings.gridPoints_ >= 5, "EuropeanPdeRisk: grid_points must be at least five");
        REQUIRE(settings.ordinarySteps_ >= 2 && settings.ordinarySteps_ <= std::numeric_limits<int>::max() - 2,
                "EuropeanPdeRisk: ordinary_steps must be in 2..INT_MAX-2");
        EuropeanThetaDetail::ValidatePhysicalSettings(settings);
        auto resolved = settings;
        if (!resolved.spotIndex_) {
            REQUIRE((settings.gridPoints_ - 1) % 4 == 0, "EuropeanPdeRisk: omitted spot_index requires a quarter-grid node");
            resolved.spotIndex_ = (settings.gridPoints_ - 1) / 4;
        }
        REQUIRE(*resolved.spotIndex_ > 0 && *resolved.spotIndex_ < settings.gridPoints_ - 1,
                "EuropeanPdeRisk: spot_index must select an interior grid node");
        return resolved;
    }

    [[nodiscard]] inline Vector_<> EuropeanThetaGrid(const EuropeanThetaSettings_& settings, const std::array<double, 3>& point) {
        static_cast<void>(ResolveEuropeanThetaSettings(settings));
        REQUIRE(std::isfinite(point[0]), "EuropeanPdeRisk: rate must be finite");
        REQUIRE(std::isfinite(point[1]) && point[1] > 0.0, "EuropeanPdeRisk: volatility must be positive and finite");
        REQUIRE(std::isfinite(point[2]) && point[2] > 0.0 && point[2] < settings.upper_,
                "EuropeanPdeRisk: strike must be finite, positive and below upper");
        REQUIRE(std::isfinite(point[2] * std::exp(-point[0] * settings.expiry_)), "EuropeanPdeRisk: discounted strike boundary must be finite");
        REQUIRE(std::isfinite(settings.upper_ * std::exp(-settings.dividendYield_ * settings.expiry_)),
                "EuropeanPdeRisk: discounted upper boundary must be finite");
        Vector_<> grid(settings.gridPoints_);
        for (int row = 0; row < settings.gridPoints_; ++row) {
            grid[row] = settings.upper_ * (static_cast<double>(row) / (settings.gridPoints_ - 1));
            REQUIRE(row == 0 || grid[row] > grid[row - 1], "EuropeanPdeRisk: grid nodes must be representably distinct");
            REQUIRE(point[2] != grid[row], "EuropeanPdeRisk: strike must lie away from grid nodes");
        }
        return grid;
    }

    namespace EuropeanThetaDetail {
        inline SampledThetaStepBindings_
        Bindings(const Vector_<>& grid, const EuropeanThetaSettings_& settings, const std::array<Number_, 3>& parameters) {
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
                result.drifts_[row - 1] = (rate - settings.dividendYield_) * grid[row];
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

        inline void SetBoundaries(SampledThetaStepBindings_* bindings,
                                  const EuropeanThetaSettings_& settings,
                                  const std::array<Number_, 3>& parameters,
                                  double tau) {
            Number_ discountedStrike = parameters[2] * exp(-parameters[0] * tau);
            bindings->externalValues_(0, 1) = discountedStrike;
            bindings->externalValues_(1, 0) = settings.upper_ * std::exp(-settings.dividendYield_ * tau) - discountedStrike;
        }
    } // namespace EuropeanThetaDetail

    inline EuropeanThetaRecording_ RecordEuropeanOptions(RecordingScope_* scope,
                                                         const EuropeanThetaSettings_& settings,
                                                         const std::array<Number_, 3>& parameters,
                                                         bool captureForwardErrors = false) {
        const auto fixed = ResolveEuropeanThetaSettings(settings);
        PDE::SampledThetaStepInputs_ numeric;
        numeric.x_ = EuropeanThetaGrid(fixed, {Value(parameters[0]), Value(parameters[1]), Value(parameters[2])});
        numeric.externalBoundaries_ = {true, true};
        auto active = EuropeanThetaDetail::Bindings(numeric.x_, fixed, parameters);
        EuropeanThetaRecording_ result;
        const int steps = fixed.ordinarySteps_ + 2;
        result.events_.reserve(steps);
        if (captureForwardErrors)
            result.forwardBackwardErrors_ = Matrix_<>(steps, 2);
        for (int step = 0; step < steps; ++step) {
            const bool damping = step < 4;
            numeric.dt_ = (damping ? 0.5 : 1.0) * (fixed.expiry_ / fixed.ordinarySteps_);
            numeric.theta_ = damping ? 1.0 : 0.5;
            const double tau = ((damping ? 0.5 * (step + 1) : step - 1.0) / fixed.ordinarySteps_) * fixed.expiry_;
            EuropeanThetaDetail::SetBoundaries(&active, fixed, parameters, tau);
            auto rolled = SampledThetaStepWithAccuracy(scope, numeric, active, fixed.accuracy_);
            result.events_.push_back(rolled.event_);
            for (int layer = 0; layer < 2; ++layer) {
                const double error = rolled.diagnostics_.forwardBackwardErrors_[layer];
                if (captureForwardErrors)
                    result.forwardBackwardErrors_(step, layer) = error;
                result.largestForwardError_ = std::max(result.largestForwardError_, error);
            }
            active.oldValues_ = std::move(rolled.solution_);
        }
        result.prices_ = {active.oldValues_(*fixed.spotIndex_, 0), active.oldValues_(*fixed.spotIndex_, 1)};
        return result;
    }
} // namespace Dal::AAD
