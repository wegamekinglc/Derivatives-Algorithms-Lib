//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class WindowRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            return {Vector_<>{theta[0] * theta[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * theta[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    class TransposeRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            const double jacobian = 1.0 + std::ldexp(1.0, -27);
            return {Vector_<>{jacobian * theta[0] - inputs[0]}, SquareMatrix_<>(1, jacobian), Matrix_<>(1, 1, -1.0)};
        }
    };

    class UnderflowRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            const double coefficient = std::numeric_limits<double>::min();
            ImplicitRootEvaluation_ result{Vector_<>{theta[0] + coefficient * inputs[0], theta[1] + coefficient * inputs[0]}, SquareMatrix_<>(2),
                                           Matrix_<>(2, 1, coefficient)};
            result.parameterJacobian_(0, 0) = result.parameterJacobian_(1, 1) = 1.0;
            return result;
        }
    };

    void CheckRootTransposeRejection(bool collect) {
        SCOPED_TRACE(collect);
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(false, 1);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, 0.0);
        scope.StartRecording();
        const TransposeRootEquation_ equation;
        auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input},
                                             ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, std::nextafter(std::ldexp(1.0, -55), 0.0)});
        scope.FinishRecording();
        scope.ClearAdjoints();
        const auto historical = ReverseWithSolveAccuracy(&scope);
        ASSERT_DOUBLE_EQ(historical.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(root.parameters_[0], 1.0);
        if (collect)
            ASSERT_THROW(static_cast<void>(ReverseWithSolveAccuracy(&scope)), Exception_);
        else
            ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
        ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_DOUBLE_EQ(historical.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
        scope.Close();
        Clear(*Tape());
    }
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootDenseCoordinateWindowsAndHistoricalReports) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    const WindowRootEquation_ equation;
    const auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = root.parameters_[0] + 2.0;
    const auto discarded = LinearSolveWithAccuracy(&scope, SquareMatrix_<>(1, 4.0), rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    Number_ objective = 3.0 * discarded.solution_(0, 0);
    scope.FinishRecording();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto suffix = ReverseSuffixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_EQ(suffix.Entries().size(), 1);
    ASSERT_THROW(static_cast<void>(suffix.Report(root.event_)), Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(root.parameters_[0]), 0.75);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
    scope.Restore(checkpoint);
    scope.FinishRecording();
    const auto prefix = ReversePrefixWithSolveAccuracy(&scope, checkpoint);
    ASSERT_NE(prefix.InvocationId(), suffix.InvocationId());
    ASSERT_EQ(prefix.Entries().size(), 1);
    ASSERT_DOUBLE_EQ(prefix.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.1875);
    scope.Restore(checkpoint);
    rhs(0, 0) = root.parameters_[0];
    const auto replacement =
        LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{2.0}, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    ASSERT_NE(replacement.event_.EventId(), discarded.event_.EventId());
    ASSERT_EQ(replacement.event_.RecordingId(), root.event_.RecordingId());
    scope.FinishRecording();
    scope.ClearAdjoints();
    Number_ output = replacement.solution_(0, 0);
    NativeOperations_::SetSeed(output, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(reports.Entries().size(), 2);
    ASSERT_THROW(static_cast<void>(reports.Report(discarded.event_)), Exception_);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.125);
    scope.Close();
    ASSERT_DOUBLE_EQ(suffix.Report(discarded.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_DOUBLE_EQ(prefix.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootTransposeLimitAppliesWithAndWithoutCollector) {
    for (bool collect : {false, true})
        ASSERT_NO_FATAL_FAILURE(CheckRootTransposeRejection(collect));
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootResidualRejectionPublishesNoOutputOrEvent) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    const auto before = MeasureTape(*Tape());
    const WindowRootEquation_ equation;
    ASSERT_THROW(static_cast<void>(ImplicitRootWithAccuracy(&scope, equation, Vector_<>{1.9}, Vector_<Number_>{input},
                                                            ImplicitRootAccuracyPolicy_{Vector_<>{1e-12}, 1e-12})),
                 Exception_);
    const auto after = MeasureTape(*Tape());
    ASSERT_EQ(after.nodes_, before.nodes_);
    ASSERT_EQ(after.reverseEvents_, before.reverseEvents_);
    ASSERT_EQ(after.reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootUnderflowDiscardsReportsAndInvalidatesRecording) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    const UnderflowRootEquation_ equation;
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0, 0.0}, Vector_<Number_>{input},
                                         ImplicitRootAccuracyPolicy_{Vector_<>{0.0, 0.0}, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    const auto historical = ReverseWithSolveAccuracy(&scope);
    NativeOperations_::SetSeed(root.parameters_[0], std::ldexp(1.0, -53));
    NativeOperations_::SetSeed(root.parameters_[1], std::ldexp(1.0, -53));
    BufferCapacityBudget_ budget(4096);
    {
        BufferCapacityScope_ caller(&budget);
        SolveAccuracyReports_ unpublished;
        ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_GE(budget.PeakCapacityBytes(), sizeof(SolveAccuracyReport_));
    }
    ASSERT_DOUBLE_EQ(historical.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Exception_);
    ASSERT_THROW(scope.Reverse(), Exception_);
    scope.Close();
    Clear(*Tape());
}
