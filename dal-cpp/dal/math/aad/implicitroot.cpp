//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <utility>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/linearsolveaccuracyinternal.hpp>
#include <dal/math/aad/linearsolveinternal.hpp>

namespace Dal::AAD {
    namespace {
        class ImplicitRootPayload_ {
            Vector_<Number_> inputs_;
            ImplicitRootLinearization_ root_;
            Matrix_<Number_> outputs_;
            SolveAccuracyEvent_ event_;

            void ReverseChannel(size_t channel, SolveAccuracyReport_* report) {
                const auto seeds = CollectSeeds(outputs_, channel);
                if (!std::all_of(seeds.begin(), seeds.end(), [](double value) { return value == 0.0; })) {
                    const auto contributions = root_.Reverse(seeds);
                    CopySolveAccuracyErrors(contributions.transposeBackwardErrors_, report, channel);
                    for (size_t input = 0; input < inputs_.size(); ++input)
                        AddContribution(&inputs_[input], contributions.inputs_(static_cast<int>(input), 0), channel);
                }
                ClearOutputs(&outputs_, channel);
            }

        public:
            ImplicitRootPayload_(const NativeInputSlots_& slots,
                                 const ImplicitRootEquation_& equation,
                                 const Vector_<>& candidate,
                                 const Vector_<Number_>& inputs,
                                 const ImplicitRootAccuracyPolicy_& policy,
                                 double tolerance,
                                 const SolveAccuracyEvent_& event)
                : inputs_(inputs), root_(equation, candidate, Snapshot(slots, inputs_), policy, tolerance),
                  outputs_(static_cast<int>(root_.Parameters().size()), 1), event_(event) {}

            [[nodiscard]] Vector_<Number_> MakeOutputs() {
                Vector_<Number_> result(root_.Parameters().size());
                for (size_t row = 0; row < result.size(); ++row) {
                    result[row] = root_.Parameters()[row];
                    outputs_(static_cast<int>(row), 0) = result[row];
                }
                return result;
            }

            [[nodiscard]] ImplicitRootDiagnostics_ Diagnostics() const {
                return {root_.Residuals(), root_.Policy(), root_.ReciprocalJacobianConditionInfinity()};
            }

            void ReverseWithScratch(bool multi, size_t width, EventBufferAccount_* account) {
                auto* report = PrepareSolveAccuracyReport(event_, 1, multi, width);
                ScratchMeasurement_ measurement(account);
                Detail::OwnedBufferScope_ scratch(account);
                for (size_t channel = 0; channel < (multi ? width : 1); ++channel)
                    ReverseChannel(channel, report);
            }
        };
    } // namespace

    CheckedImplicitRootResult_ ImplicitRootWithAccuracy(RecordingScope_* recording,
                                                        const ImplicitRootEquation_& equation,
                                                        const Vector_<>& candidate,
                                                        const Vector_<Number_>& inputs,
                                                        const ImplicitRootAccuracyPolicy_& policy,
                                                        double tolerance) {
        return WithRecordingFailure(recording, [&](Tape_* tape) {
            const auto identity = NativeRecordedOperation_::AccuracyRecording(recording, false);
            const auto token = NewSolveAccuracyEvent(identity.recording_);
            auto event = NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<ImplicitRootPayload_, true>>(tape, tape, equation, candidate, inputs,
                                                                                                            policy, tolerance, token);
            auto diagnostics = event->Diagnostics();
            auto parameters = PublishSolve(recording, tape, std::move(event));
            return CheckedImplicitRootResult_{std::move(parameters), std::move(diagnostics), token};
        });
    }
} // namespace Dal::AAD
