//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <utility>

#include <dal/math/aad/linearsolveaccuracyinternal.hpp>
#include <dal/math/aad/linearsolveinternal.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

namespace Dal::AAD {
    namespace {
        template <class T_> bool HasBindings(const Vector_<T_>& values) { return !values.empty(); }
        template <class T_> bool HasBindings(const Matrix_<T_>& values) { return !values.Empty(); }

        template <class T_, class P_> P_ BoundValues(const NativeInputSlots_& slots, const T_& active, const P_& passive) {
            return HasBindings(active) ? Snapshot(slots, active) : passive;
        }

        double BoundValues(const NativeInputSlots_& slots, const std::optional<Number_>& active, double passive) {
            if (!active)
                return passive;
            NativeRecordedOperation_::ValidateInput(slots, *active);
            return Value(*active);
        }

        PDE::SampledThetaStepPullback_ CaptureStep(Tape_* tape,
                                                   const PDE::SampledThetaStepInputs_& passive,
                                                   const SampledThetaStepBindings_& active,
                                                   const LinearSolveAccuracyPolicy_& policy,
                                                   double tolerance) {
            REQUIRE(HasBindings(active.rates_) || HasBindings(active.drifts_) || HasBindings(active.variances_) || HasBindings(active.oldValues_) ||
                        HasBindings(active.externalValues_) || active.dt_ || active.theta_,
                    "SampledThetaStepWithAccuracy requires at least one active field");
            const NativeInputSlots_ slots(tape);
            const PDE::SampledThetaStepInputs_ values{passive.x_,
                                                      BoundValues(slots, active.rates_, passive.rates_),
                                                      BoundValues(slots, active.drifts_, passive.drifts_),
                                                      BoundValues(slots, active.variances_, passive.variances_),
                                                      BoundValues(slots, active.dt_, passive.dt_),
                                                      BoundValues(slots, active.theta_, passive.theta_),
                                                      BoundValues(slots, active.oldValues_, passive.oldValues_),
                                                      passive.externalBoundaries_,
                                                      BoundValues(slots, active.externalValues_, passive.externalValues_)};
            return PDE::SampledThetaStepPullback_(values, policy, tolerance);
        }

        class SampledThetaStepPayload_ {
            SampledThetaStepBindings_ inputs_;
            PDE::SampledThetaStepPullback_ step_;
            Matrix_<Number_> outputs_;
            SolveAccuracyEvent_ event_;

            void ReverseChannel(size_t channel, SolveAccuracyReport_* report) {
                const auto seeds = CollectSeeds(outputs_, channel);
                if (!std::all_of(seeds.begin(), seeds.end(), [](double value) { return value == 0.0; })) {
                    const auto risk = step_.Reverse(seeds);
                    CopySolveAccuracyErrors(risk.transposeBackwardErrors_, report, channel);
                    Scatter(&inputs_.rates_, risk.rates_, channel);
                    Scatter(&inputs_.drifts_, risk.drifts_, channel);
                    Scatter(&inputs_.variances_, risk.variances_, channel);
                    Scatter(&inputs_.oldValues_, risk.oldValues_, channel);
                    Scatter(&inputs_.externalValues_, risk.externalValues_, channel);
                    if (inputs_.dt_)
                        AddContribution(&*inputs_.dt_, risk.dt_, channel);
                    if (inputs_.theta_)
                        AddContribution(&*inputs_.theta_, risk.theta_, channel);
                }
                ClearOutputs(&outputs_, channel);
            }

        public:
            SampledThetaStepPayload_(const NativeInputSlots_&,
                                     const SampledThetaStepBindings_& inputs,
                                     const PDE::SampledThetaStepPullback_& captured,
                                     const SolveAccuracyEvent_& event)
                : inputs_(inputs), step_(captured), outputs_(step_.Solution().Rows(), step_.Solution().Cols()), event_(event) {}

            [[nodiscard]] Matrix_<Number_> MakeOutputs() { return AAD::MakeOutputs(step_.Solution(), &outputs_); }
            [[nodiscard]] SampledThetaStepDiagnostics_ Diagnostics() const { return {step_.ForwardBackwardErrors(), step_.Policy()}; }

            void ReverseWithScratch(bool multi, size_t width, EventBufferAccount_* account) {
                auto* report = PrepareSolveAccuracyReport(event_, step_.Solution().Cols(), multi, width);
                ScratchMeasurement_ measurement(account);
                Detail::OwnedBufferScope_ scratch(account);
                for (size_t channel = 0; channel < (multi ? width : 1); ++channel)
                    ReverseChannel(channel, report);
            }
        };
    } // namespace

    CheckedSampledThetaStepResult_ SampledThetaStepWithAccuracy(RecordingScope_* recording,
                                                                const PDE::SampledThetaStepInputs_& numericInputs,
                                                                const SampledThetaStepBindings_& activeBindings,
                                                                const LinearSolveAccuracyPolicy_& policy,
                                                                double tolerance) {
        return WithRecordingFailure(recording, [&](Tape_* tape) {
            const SampledThetaStepBindings_ bindings(activeBindings);
            const auto captured = CaptureStep(tape, numericInputs, bindings, policy, tolerance);
            const auto identity = NativeRecordedOperation_::AccuracyRecording(recording, false);
            const auto token = NewSolveAccuracyEvent(identity.recording_);
            auto event =
                NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<SampledThetaStepPayload_, true>>(tape, tape, bindings, captured, token);
            auto diagnostics = event->Diagnostics();
            auto solution = PublishSolve(recording, tape, std::move(event));
            return CheckedSampledThetaStepResult_{std::move(solution), std::move(diagnostics), token};
        });
    }
} // namespace Dal::AAD
