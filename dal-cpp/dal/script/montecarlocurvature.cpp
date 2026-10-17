//
// Created by Codex on 2026/10/09.
//

#include <algorithm>

#include <dal/math/aad/detail/gradientbumps.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/script/montecarlocurvature.hpp>

namespace Dal::Script {
    namespace {
        std::optional<size_t> RecordingCap(const std::optional<size_t>& bump, const std::optional<size_t>& path) {
            if (!bump)
                return path;
            return path ? std::min(*bump, *path) : bump;
        }

        void ValidatePoint(const BlackScholesSegmentedPath_& kernel,
                           const Vector_<>& point,
                           const AAD::SegmentedPathSettings_& settings,
                           const String_& context) {
            try {
                kernel.ValidateRequest(point, settings);
            } catch (const Exception_& error) {
                THROW("MonteCarloCurvature: " + context + "; " + error.what());
            }
        }

        struct Gradient_ {
            SegmentedMonteCarloResult_ result_;
            [[nodiscard]] const Vector_<>& Gradient() const { return result_.MeanGradient(); }
        };
    } // namespace

    MonteCarloCurvatureResult_ EvaluateBlackScholesMonteCarloCurvature(const BlackScholesSegmentedPath_& kernel,
                                                                       const Vector_<>& parameters,
                                                                       size_t pathCount,
                                                                       const AAD::BumpOverAADRequest_& bumps,
                                                                       const SegmentedMonteCarloSettings_& settings) {
        namespace Bumps = AAD::GradientBumpsDetail;
        AAD::RequireRecordingModeChangeAllowed();
        const BlackScholesSegmentedPath_ fixedKernel = kernel;
        Vector_<> fixedPoint = parameters;
        AAD::BumpOverAADRequest_ fixedBumps = bumps;
        const SegmentedMonteCarloSettings_ fixedSettings = settings;
        auto effectiveSettings = fixedSettings;
        effectiveSettings.path_.recordingCapacityBudgetBytes_ =
            RecordingCap(fixedBumps.recordingCapacityBudgetBytes_, fixedSettings.path_.recordingCapacityBudgetBytes_);
        MonteCarloCurvatureExecution_ execution;
        execution.numericPayloadBytes_ = Bumps::Validate(fixedPoint, fixedBumps);
        execution.gradientEvaluations_ = Bumps::Sum(1, Bumps::Product(2, fixedBumps.steps_.size()));
        execution.recordingCapacityBudgetBytes_ = effectiveSettings.path_.recordingCapacityBudgetBytes_;
        ValidatePoint(fixedKernel, fixedPoint, effectiveSettings.path_, "base");
        for (int row = 0; row < fixedBumps.directions_.Rows(); ++row) {
            const String_ context = Bumps::DirectionContext(row);
            ValidatePoint(fixedKernel, Bumps::BumpedPoint(fixedPoint, fixedBumps, row, 1.0), effectiveSettings.path_, context + "; plus");
            ValidatePoint(fixedKernel, Bumps::BumpedPoint(fixedPoint, fixedBumps, row, -1.0), effectiveSettings.path_, context + "; minus");
        }
        auto* tape = AAD::Tape();
        const AAD::NumResultsResetterForAAD_ mode(tape, tape->multi_, tape->numAdj_);
        tape->multi_ = false;
        tape->numAdj_ = 1;
        auto result = Bumps::Evaluate(
            [&](const Vector_<>& point, const String_& context) {
                try {
                    auto evaluated = EvaluateBlackScholesSegmentedMonteCarlo(fixedKernel, point, pathCount, effectiveSettings);
                    const auto& usage = evaluated.Execution();
                    execution.maxPathTapeBytes_ = std::max(execution.maxPathTapeBytes_, usage.maxPathTapeBytes_);
                    execution.maxPathCheckpointBytes_ = std::max(execution.maxPathCheckpointBytes_, usage.maxPathCheckpointBytes_);
                    execution.maxPathCleanupReserveBytes_ = std::max(execution.maxPathCleanupReserveBytes_, usage.maxPathCleanupReserveBytes_);
                    return Gradient_{std::move(evaluated)};
                } catch (const Exception_& error) {
                    THROW("MonteCarloCurvature: " + context + "; " + error.what());
                }
            },
            fixedPoint, fixedBumps);
        return {std::move(result.base_.result_), std::move(fixedPoint),       std::move(fixedBumps), fixedSettings,
                fixedKernel.PreparedHandle(),    std::move(result.products_), std::move(execution)};
    }
} // namespace Dal::Script
