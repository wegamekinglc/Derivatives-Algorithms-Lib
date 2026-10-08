//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <atomic>
#include <limits>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/aad/linearsolveinternal.hpp>

namespace Dal::AAD {
    namespace {
        std::uint64_t NewAccuracyIdentity() {
            static std::atomic<std::uint64_t> nextIdentity{1};
            auto identity = nextIdentity.load(std::memory_order_relaxed);
            while (identity != 0) {
                const auto next = identity == std::numeric_limits<std::uint64_t>::max() ? 0 : identity + 1;
                if (nextIdentity.compare_exchange_weak(identity, next, std::memory_order_relaxed))
                    return identity;
            }
            THROW("SolveAccuracy: event/invocation identities exhausted");
        }
    } // namespace

    struct SolveAccuracyAccess_ {
        static SolveAccuracyEvent_ Event(std::uint64_t recording) {
            SolveAccuracyEvent_ result;
            result.recording_ = recording;
            result.event_ = NewAccuracyIdentity();
            return result;
        }
        static SolveAccuracyReports_ Reports(const NativeRecordingIdentity_& recording) {
            SolveAccuracyReports_ result;
            result.invocationId_ = NewAccuracyIdentity();
            result.multi_ = recording.multi_;
            result.width_ = recording.multi_ ? recording.width_ : 1;
            return result;
        }
        static SolveAccuracyReport_* Append(SolveAccuracyReports_* reports, const SolveAccuracyEvent_& event, int rhsColumns) {
            REQUIRE(reports->width_ <= static_cast<size_t>(std::numeric_limits<int>::max()), "SolveAccuracy: channel width exceeds matrix extent");
            REQUIRE(rhsColumns > 0 && reports->width_ != 0 &&
                        static_cast<size_t>(rhsColumns) <=
                            static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(double) / reports->width_,
                    "SolveAccuracy: RHS/channel report extent exceeds the supported buffer range");
            reports->entries_.push_back(
                {event, reports->invocationId_, reports->multi_, Matrix_<>(rhsColumns, static_cast<int>(reports->width_), 0.0)});
            return &reports->entries_.back();
        }
    };

    const SolveAccuracyReport_& SolveAccuracyReports_::Report(const SolveAccuracyEvent_& event) const {
        const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const auto& report) { return report.event_ == event; });
        REQUIRE(event.EventId() != 0 && found != entries_.end(), "SolveAccuracy.Report: event did not execute in this successful invocation");
        return *found;
    }

    namespace {
        struct AccuracyCollector_ {
            NativeRecordingIdentity_ recording_;
            SolveAccuracyReports_ reports_;
        };

        thread_local AccuracyCollector_* activeCollector = nullptr;

        class CollectionScope_ {
        public:
            explicit CollectionScope_(AccuracyCollector_* collector) {
                REQUIRE(activeCollector == nullptr, "SolveAccuracy.Reverse: nested report collection is unsupported");
                activeCollector = collector;
            }
            ~CollectionScope_() noexcept { activeCollector = nullptr; }
            CollectionScope_(const CollectionScope_&) = delete;
            CollectionScope_& operator=(const CollectionScope_&) = delete;
        };

        SolveAccuracyReport_* PrepareReport(const SolveAccuracyEvent_& event, int rhsColumns, bool multi, size_t width) {
            if (activeCollector == nullptr)
                return nullptr;
            const auto& recording = activeCollector->recording_;
            REQUIRE(event.RecordingId() == recording.recording_ && multi == recording.multi_ && width == recording.width_,
                    "SolveAccuracy.Reverse: event recording or mode does not match the invocation");
            return SolveAccuracyAccess_::Append(&activeCollector->reports_, event, rhsColumns);
        }

        void CopyErrors(const Vector_<>& errors, SolveAccuracyReport_* report, size_t channel) {
            if (report != nullptr)
                for (int row = 0; row < report->transposeBackwardErrors_.Rows(); ++row)
                    report->transposeBackwardErrors_(row, static_cast<int>(channel)) = errors[static_cast<size_t>(row)];
        }

        class CheckedLinearSolvePayload_ {
            CheckedLinearSolve_ solve_;
            SquareMatrix_<Number_> matrix_;
            Matrix_<Number_> rhs_;
            Matrix_<Number_> outputs_;
            SolveAccuracyEvent_ event_;

            void ReverseChannel(size_t channel, SolveAccuracyReport_* report) {
                const auto seeds = CollectSeeds(outputs_, channel);
                if (!std::all_of(seeds.begin(), seeds.end(), [](double value) { return value == 0.0; })) {
                    const auto contributions = matrix_.Rows() == 0 ? solve_.ReverseRhs(seeds) : solve_.Reverse(seeds);
                    CopyErrors(contributions.transposeBackwardErrors_, report, channel);
                    Scatter(&matrix_, contributions.adjoints_.matrix_, channel);
                    Scatter(&rhs_, contributions.adjoints_.rhs_, channel);
                }
                ClearOutputs(&outputs_, channel);
            }

        public:
            template <class M_, class R_>
            CheckedLinearSolvePayload_(const NativeInputSlots_& slots,
                                       const SquareMatrix_<M_>& matrix,
                                       const Matrix_<R_>& rhs,
                                       const LinearSolveAccuracyPolicy_& policy,
                                       double tolerance,
                                       const SolveAccuracyEvent_& event)
                : solve_(Snapshot(slots, matrix), Snapshot(slots, rhs), policy, tolerance), outputs_(rhs.Rows(), rhs.Cols()), event_(event) {
                if constexpr (std::is_same_v<M_, Number_>)
                    matrix_ = matrix;
                if constexpr (std::is_same_v<R_, Number_>)
                    rhs_ = rhs;
            }
            [[nodiscard]] Matrix_<Number_> MakeOutputs() { return AAD::MakeOutputs(solve_.Solution(), &outputs_); }
            [[nodiscard]] const LinearSolveDiagnostics_& Diagnostics() const { return solve_.Diagnostics(); }
            void ReverseWithScratch(bool multi, size_t width, EventBufferAccount_* account) {
                auto* report = PrepareReport(event_, outputs_.Cols(), multi, width);
                ScratchMeasurement_ measurement(account);
                Detail::OwnedBufferScope_ scratch(account);
                for (size_t channel = 0; channel < (multi ? width : 1); ++channel)
                    ReverseChannel(channel, report);
            }
        };

        template <class M_, class R_>
        CheckedLinearSolveResult_ RecordCheckedSolve(RecordingScope_* recording,
                                                     const SquareMatrix_<M_>& matrix,
                                                     const Matrix_<R_>& rhs,
                                                     const LinearSolveAccuracyPolicy_& policy,
                                                     double tolerance) {
            return WithRecordingFailure(recording, [&](Tape_* tape) {
                REQUIRE(matrix.Rows() > 0 && rhs.Rows() == matrix.Rows() && rhs.Cols() > 0,
                        "LinearSolveWithAccuracy: requires a nonempty square system and RHS");
                const auto identity = NativeRecordedOperation_::AccuracyRecording(recording, false);
                const auto token = SolveAccuracyAccess_::Event(identity.recording_);
                auto event = NativeRecordedOperation_::MakeEvent<LinearSolveEvent_<CheckedLinearSolvePayload_, true>>(tape, tape, matrix, rhs, policy,
                                                                                                                      tolerance, token);
                auto diagnostics = event->Diagnostics();
                auto solution = PublishSolve(recording, tape, std::move(event));
                return CheckedLinearSolveResult_{std::move(solution), std::move(diagnostics), token};
            });
        }

        template <class F_> SolveAccuracyReports_ CollectReports(RecordingScope_* recording, const F_& reverse) {
            const auto identity = NativeRecordedOperation_::AccuracyRecording(recording, true);
            NativeOperations_::ValidateAdjointMode(identity.multi_, identity.width_);
            AccuracyCollector_ collector{identity, SolveAccuracyAccess_::Reports(identity)};
            CollectionScope_ collection(&collector);
            reverse();
            return std::move(collector.reports_);
        }
    } // namespace

    CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                      const SquareMatrix_<Number_>& matrix,
                                                      const Matrix_<Number_>& rhs,
                                                      const LinearSolveAccuracyPolicy_& policy,
                                                      double tolerance) {
        return RecordCheckedSolve(recording, matrix, rhs, policy, tolerance);
    }
    CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                      const SquareMatrix_<>& matrix,
                                                      const Matrix_<Number_>& rhs,
                                                      const LinearSolveAccuracyPolicy_& policy,
                                                      double tolerance) {
        return RecordCheckedSolve(recording, matrix, rhs, policy, tolerance);
    }
    CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                      const SquareMatrix_<Number_>& matrix,
                                                      const Matrix_<>& rhs,
                                                      const LinearSolveAccuracyPolicy_& policy,
                                                      double tolerance) {
        return RecordCheckedSolve(recording, matrix, rhs, policy, tolerance);
    }

    SolveAccuracyReports_ ReverseWithSolveAccuracy(RecordingScope_* recording) {
        return CollectReports(recording, [&] { recording->Reverse(); });
    }
    SolveAccuracyReports_ ReverseSuffixWithSolveAccuracy(RecordingScope_* recording, const Checkpoint_& checkpoint) {
        return CollectReports(recording, [&] { recording->ReverseSuffix(checkpoint); });
    }
    SolveAccuracyReports_ ReversePrefixWithSolveAccuracy(RecordingScope_* recording, const Checkpoint_& checkpoint) {
        return CollectReports(recording, [&] { recording->ReversePrefix(checkpoint); });
    }
} // namespace Dal::AAD
