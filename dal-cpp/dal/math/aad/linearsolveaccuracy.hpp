//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <cstdint>

#include <dal/math/aad/recording.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>

namespace Dal::AAD {
    struct SolveAccuracyAccess_;

    class SolveAccuracyEvent_ {
        std::uint64_t recording_ = 0;
        std::uint64_t event_ = 0;
        friend struct SolveAccuracyAccess_;

    public:
        [[nodiscard]] std::uint64_t RecordingId() const { return recording_; }
        [[nodiscard]] std::uint64_t EventId() const { return event_; }
        [[nodiscard]] bool operator==(const SolveAccuracyEvent_& other) const { return recording_ == other.recording_ && event_ == other.event_; }
    };

    struct SolveAccuracyReport_ {
        SolveAccuracyEvent_ event_;
        std::uint64_t invocationId_ = 0;
        bool isMulti_ = false;
        Matrix_<> transposeBackwardErrors_;
    };

    class SolveAccuracyReports_ {
        std::uint64_t invocationId_ = 0;
        bool multi_ = false;
        size_t width_ = 1;
        Vector_<SolveAccuracyReport_> entries_;
        friend struct SolveAccuracyAccess_;

    public:
        [[nodiscard]] const Vector_<SolveAccuracyReport_>& Entries() const { return entries_; }
        [[nodiscard]] const SolveAccuracyReport_& Report(const SolveAccuracyEvent_& event) const;
        [[nodiscard]] std::uint64_t InvocationId() const { return invocationId_; }
        [[nodiscard]] bool IsMulti() const { return multi_; }
        [[nodiscard]] size_t Channels() const { return width_; }
    };

    struct CheckedLinearSolveResult_ {
        Matrix_<Number_> solution_;
        LinearSolveDiagnostics_ diagnostics_;
        SolveAccuracyEvent_ event_;
    };

    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const SquareMatrix_<Number_>& matrix,
                                                                    const Matrix_<Number_>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const SquareMatrix_<>& matrix,
                                                                    const Matrix_<Number_>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());
    [[nodiscard]] CheckedLinearSolveResult_ LinearSolveWithAccuracy(RecordingScope_* recording,
                                                                    const SquareMatrix_<Number_>& matrix,
                                                                    const Matrix_<>& rhs,
                                                                    const LinearSolveAccuracyPolicy_& policy,
                                                                    double relativePivotTolerance = 64.0 * std::numeric_limits<double>::epsilon());

    [[nodiscard]] SolveAccuracyReports_ ReverseWithSolveAccuracy(RecordingScope_* recording);
    [[nodiscard]] SolveAccuracyReports_ ReverseSuffixWithSolveAccuracy(RecordingScope_* recording, const Checkpoint_& checkpoint);
    [[nodiscard]] SolveAccuracyReports_ ReversePrefixWithSolveAccuracy(RecordingScope_* recording, const Checkpoint_& checkpoint);
} // namespace Dal::AAD
