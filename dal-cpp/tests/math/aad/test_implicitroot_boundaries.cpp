//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    class BoundaryRootEquation_ final : public ImplicitRootEquation_ {
        double jacobian_;
        size_t* evaluations_;

    public:
        explicit BoundaryRootEquation_(size_t* evaluations, double jacobian = 1.0) : jacobian_(jacobian), evaluations_(evaluations) {}
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            ++*evaluations_;
            return {Vector_<>{jacobian_ * theta[0] - inputs[0]}, SquareMatrix_<>(1, jacobian_), Matrix_<>(1, 1, -1.0)};
        }
    };

    class SerialRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            return {Vector_<>{theta[0] * theta[0] - inputs[0]}, SquareMatrix_<>(1, 2.0 * theta[0]), Matrix_<>(1, 1, -1.0)};
        }
    };

    class AliasOverflowRootEquation_ final : public ImplicitRootEquation_ {
    public:
        [[nodiscard]] ImplicitRootEvaluation_ Evaluate(const Vector_<>& theta, const Vector_<>& inputs) const override {
            return {Vector_<>{theta[0] - inputs[0] - inputs[1]}, SquareMatrix_<>(1, 1.0), Matrix_<>(1, 2, -1.0)};
        }
    };

    template <class F_> void CheckRecordedOperationFailure(const F_& operation) {
        try {
            operation();
            FAIL() << "Expected a recorded-operation failure";
        } catch (const Exception_& error) {
            const std::string message(error.what());
            ASSERT_NE(message.find("RecordedOperation"), std::string::npos);
            ASSERT_EQ(message.find("LinearSolve"), std::string::npos);
        }
    }

    void CheckUnpublishedReverseFailure(RecordingScope_* scope, Number_* input, bool checkOperation = false) {
        BufferCapacityBudget_ budget(4096);
        BufferCapacityScope_ caller(&budget);
        SolveAccuracyReports_ unpublished;
        if (checkOperation)
            ASSERT_NO_FATAL_FAILURE(CheckRecordedOperationFailure([&] { unpublished = ReverseWithSolveAccuracy(scope); }));
        else
            ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(scope), Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(*input)), Exception_);
        ASSERT_THROW(scope->Reverse(), Exception_);
    }
} // namespace

TEST(AADLinearSolveTest, TestRecordedImplicitRootSerialEventsMatchElementaryComposition) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 4.0);
    scope.StartRecording();
    const SerialRootEquation_ equation;
    const ImplicitRootAccuracyPolicy_ policy{Vector_<>{1e-14}, 1e-14};
    const auto first = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{2.0}, Vector_<Number_>{input}, policy);
    const auto second = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{std::sqrt(2.0)}, first.parameters_, policy);
    Number_ difference = second.parameters_[0] - sqrt(sqrt(input));
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(difference, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_EQ(reports.Entries().size(), 2);
    ASSERT_NE(first.event_.EventId(), second.event_.EventId());
    ASSERT_EQ(first.event_.RecordingId(), second.event_.RecordingId());
    ASSERT_DOUBLE_EQ(Value(difference), 0.0);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 0.0, 1e-14);
    ASSERT_DOUBLE_EQ(reports.Report(first.event_).transposeBackwardErrors_(0, 0), 0.0);
    ASSERT_LE(reports.Report(second.event_).transposeBackwardErrors_(0, 0), 1e-14);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootInclusiveActualTransposeErrorsAcrossChannels) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    const double delta = std::ldexp(1.0, -27);
    const double error = std::ldexp(1.0, -55);
    size_t evaluations = 0;
    const BoundaryRootEquation_ equation(&evaluations, 1.0 + delta);
    auto root =
        ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, error});
    scope.FinishRecording();
    scope.ClearAdjoints();
    const Vector_<> weights{1.0, -1.0, 0.0, 2.0};
    for (size_t channel = 0; channel < weights.size(); ++channel)
        NativeOperations_::SetSeed(root.parameters_[0], weights[channel], channel);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    const auto& actual = reports.Report(root.event_).transposeBackwardErrors_;
    ASSERT_EQ(actual.Rows(), 1);
    ASSERT_EQ(actual.Cols(), 4);
    for (size_t channel = 0; channel < weights.size(); ++channel) {
        ASSERT_DOUBLE_EQ(actual(0, static_cast<int>(channel)), weights[channel] == 0.0 ? 0.0 : error);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, channel), weights[channel] * (1.0 - delta));
    }
    ASSERT_EQ(evaluations, 1);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootAliasAccumulationOverflowInvalidatesRecording) {
    const AliasOverflowRootEquation_ equation;
    const ImplicitRootAccuracyPolicy_ policy{Vector_<>{0.0}, 0.0};
    const double maximum = std::numeric_limits<double>::max();
    {
        const ImplicitRootLinearization_ numeric(equation, Vector_<>{0.0}, Vector_<>{0.0, 0.0}, policy);
        const auto reverse = numeric.Reverse(Matrix_<>(1, 1, maximum));
        ASSERT_DOUBLE_EQ(reverse.inputs_(0, 0), maximum);
        ASSERT_DOUBLE_EQ(reverse.inputs_(1, 0), maximum);
    }
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input, input}, policy);
    scope.FinishRecording();
    scope.ClearAdjoints();
    const auto historical = ReverseWithSolveAccuracy(&scope);
    NativeOperations_::SetSeed(root.parameters_[0], maximum);
    ASSERT_NO_FATAL_FAILURE(CheckUnpublishedReverseFailure(&scope, &input, true));
    ASSERT_DOUBLE_EQ(historical.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootWrongPhaseAndNullFailBeforeEquation) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    size_t evaluations = 0;
    const BoundaryRootEquation_ equation(&evaluations);
    const ImplicitRootAccuracyPolicy_ policy{Vector_<>{0.0}, 0.0};
    const auto before = MeasureTape(*Tape());
    ASSERT_NO_FATAL_FAILURE(CheckRecordedOperationFailure(
        [&] { static_cast<void>(ImplicitRootWithAccuracy(nullptr, equation, Vector_<>{0.0}, Vector_<Number_>{input}, policy)); }));
    ASSERT_NO_FATAL_FAILURE(CheckRecordedOperationFailure(
        [&] { static_cast<void>(ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input}, policy)); }));
    ASSERT_EQ(evaluations, 0);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    scope.StartRecording();
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input}, policy);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(root.parameters_[0], 1.0);
    scope.Reverse();
    ASSERT_EQ(evaluations, 1);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 1.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootStaleUnoccupiedSlotRejectedBeforeEquation) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    Number_ stale;
    {
        RecordingScope_ old;
        Number_ dummy;
        old.RegisterInput(dummy, 0.0);
        old.RegisterInput(stale, 0.0);
        old.Close();
    }
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ fresh;
    scope.RegisterInput(fresh, 0.0);
    scope.StartRecording();
    size_t evaluations = 0;
    const BoundaryRootEquation_ equation(&evaluations);
    const auto before = MeasureTape(*Tape());
    ASSERT_NO_FATAL_FAILURE(CheckRecordedOperationFailure([&] {
        static_cast<void>(
            ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{stale}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0}));
    }));
    ASSERT_EQ(evaluations, 0);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_THROW(scope.FinishRecording(), Exception_);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestRecordedImplicitRootTinyJacobianRejectsUnrepresentableTranspose) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(false, 1);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    size_t evaluations = 0;
    const BoundaryRootEquation_ equation(&evaluations, 1e-310);
    auto root = ImplicitRootWithAccuracy(&scope, equation, Vector_<>{0.0}, Vector_<Number_>{input}, ImplicitRootAccuracyPolicy_{Vector_<>{0.0}, 0.0});
    ASSERT_DOUBLE_EQ(root.diagnostics_.reciprocalConditionInfinity_, 1.0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    const auto historical = ReverseWithSolveAccuracy(&scope);
    NativeOperations_::SetSeed(root.parameters_[0], 1.0);
    ASSERT_NO_FATAL_FAILURE(CheckUnpublishedReverseFailure(&scope, &input));
    ASSERT_DOUBLE_EQ(historical.Report(root.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}
