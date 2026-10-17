//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <future>
#include <limits>
#include <utility>
#include <vector>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/linearsolvediagnostics.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::AAD::Clear;
using Dal::AAD::LinearSolveWithDiagnostics;
using Dal::AAD::NativeOperations_;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::Tape;
using Dal::AAD::Value;

namespace {
    double Determinant(const SquareMatrix_<>& matrix) {
        return matrix(0, 0) * (matrix(1, 1) * matrix(2, 2) - matrix(1, 2) * matrix(2, 1)) -
               matrix(0, 1) * (matrix(1, 0) * matrix(2, 2) - matrix(1, 2) * matrix(2, 0)) +
               matrix(0, 2) * (matrix(1, 0) * matrix(2, 1) - matrix(1, 1) * matrix(2, 0));
    }

    double ReferenceObjective(const SquareMatrix_<>& matrix, const Matrix_<>& rhs, const Matrix_<>& seeds) {
        const double determinant = Determinant(matrix);
        double result = 0.0;
        for (int column = 0; column < rhs.Cols(); ++column)
            for (int variable = 0; variable < 3; ++variable) {
                auto replaced = matrix;
                for (int row = 0; row < 3; ++row)
                    replaced(row, variable) = rhs(row, column);
                result += seeds(variable, column) * Determinant(replaced) / determinant;
            }
        return result;
    }

    struct SolveSample_ {
        SquareMatrix_<> matrix_{3};
        Matrix_<> rhs_{3, 2}, seeds_{3, 2};

        SolveSample_() {
            const double entries[3][3] = {{0.0, 2.0, 1.0}, {1.0, 0.0, 3.0}, {4.0, 1.0, 0.0}};
            const double right[3][2] = {{1.0, 4.0}, {2.0, -1.0}, {3.0, 2.0}};
            const double weights[3][2] = {{0.5, -2.0}, {3.0, 0.25}, {-1.0, 2.0}};
            for (int row = 0; row < 3; ++row) {
                for (int column = 0; column < 3; ++column)
                    matrix_(row, column) = entries[row][column];
                for (int column = 0; column < 2; ++column) {
                    rhs_(row, column) = right[row][column];
                    seeds_(row, column) = weights[row][column];
                }
            }
        }
    };

    struct NativeSolveSample_ {
        SquareMatrix_<Number_> matrix_{3};
        Matrix_<Number_> rhs_{3, 2};

        NativeSolveSample_(RecordingScope_* scope, const SolveSample_& sample) {
            for (int row = 0; row < 3; ++row) {
                for (int column = 0; column < 3; ++column)
                    scope->RegisterInput(matrix_(row, column), sample.matrix_(row, column));
                for (int column = 0; column < 2; ++column)
                    scope->RegisterInput(rhs_(row, column), sample.rhs_(row, column));
            }
        }
    };

    Dal::AAD::DiagnosedLinearSolveResult_
    SolveSample(RecordingScope_* scope, const SolveSample_& sample, const NativeSolveSample_& inputs, int activity) {
        return activity == 1   ? LinearSolveWithDiagnostics(scope, inputs.matrix_, sample.rhs_)
               : activity == 2 ? LinearSolveWithDiagnostics(scope, sample.matrix_, inputs.rhs_)
                               : LinearSolveWithDiagnostics(scope, inputs.matrix_, inputs.rhs_);
    }

    double ChannelWeight(size_t channel) { return channel % 3 == 0 ? 1.0 : channel % 3 == 1 ? -2.0 : 0.0; }

    void CheckAndSeed(Dal::AAD::DiagnosedLinearSolveResult_* result,
                      const SolveSample_& sample,
                      const Dal::DiagnosedLinearSolve_& numeric,
                      size_t channels) {
        ASSERT_NEAR(result->diagnostics_.reciprocalConditionInfinity_, 5.0 / 17.0, 1e-10);
        ASSERT_EQ(result->diagnostics_.componentwiseBackwardErrors_.size(), 2);
        for (int column = 0; column < 2; ++column) {
            ASSERT_DOUBLE_EQ(result->diagnostics_.componentwiseBackwardErrors_[column], numeric.Diagnostics().componentwiseBackwardErrors_[column]);
            for (int row = 0; row < 3; ++row) {
                ASSERT_DOUBLE_EQ(Value(result->solution_(row, column)), numeric.Solve().Solution()(row, column));
                for (size_t channel = 0; channel < channels; ++channel)
                    NativeOperations_::SetSeed(result->solution_(row, column), ChannelWeight(channel) * sample.seeds_(row, column), channel);
            }
        }
    }

    template <class C_, class F_> double CoordinateDifference(const C_& values, int row, int column, double step, const F_& objective) {
        auto above = values, below = values;
        above(row, column) += step;
        below(row, column) -= step;
        return (objective(above) - objective(below)) / (2.0 * step);
    }

    void CheckAdjointChannels(const Number_& input, double expected, size_t channels) {
        for (size_t channel = 0; channel < channels; ++channel)
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(input, channel), ChannelWeight(channel) * expected, 2e-8);
    }

    void CheckSampleDifferences(const SolveSample_& sample, const NativeSolveSample_& inputs, int activity, double step, size_t channels) {
        const auto matrixObjective = [&](const SquareMatrix_<>& matrix) { return ReferenceObjective(matrix, sample.rhs_, sample.seeds_); };
        const auto rhsObjective = [&](const Matrix_<>& rhs) { return ReferenceObjective(sample.matrix_, rhs, sample.seeds_); };
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const double expected = activity == 2 ? 0.0 : CoordinateDifference(sample.matrix_, row, column, step, matrixObjective);
                ASSERT_NO_FATAL_FAILURE(CheckAdjointChannels(inputs.matrix_(row, column), expected, channels));
            }
            for (int column = 0; column < 2; ++column) {
                const double expected = activity == 1 ? 0.0 : CoordinateDifference(sample.rhs_, row, column, step, rhsObjective);
                ASSERT_NO_FATAL_FAILURE(CheckAdjointChannels(inputs.rhs_(row, column), expected, channels));
            }
        }
    }

    size_t DiagnosedTapePeak(size_t extraLimit) {
        Clear(*Tape());
        const auto initial = Dal::AAD::MeasureTape(*Tape()).capacityBytes_;
        Dal::AAD::TapeCapacityBudget_ budget(initial + Dal::AAD::TapeCleanupCapacityBytes() + extraLimit);
        Dal::AAD::TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        SquareMatrix_<> matrix(2);
        matrix(0, 0) = matrix(1, 1) = 2.0;
        Matrix_<Number_> rhs(2, 2);
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column)
                scope.RegisterInput(rhs(row, column), 3.0);
        scope.StartRecording();
        auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
        scope.FinishRecording();
        NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
        scope.Reverse();
        REQUIRE(NativeOperations_::ReadAdjoint(rhs(0, 0)) == 0.5, "Diagnosed solve gradient changed");
        REQUIRE(result.diagnostics_.reciprocalConditionInfinity_ == 1.0, "Diagnosed solve condition changed");
        const auto peak = budget.PeakCapacityBytes() - initial;
        scope.Close();
        REQUIRE(budget.CapacityBytes() == initial, "Diagnosed solve did not refund tape storage");
        return peak;
    }
} // namespace

TEST(AADLinearSolveTest, TestDiagnosticsComposeAndReportSurvivesClose) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q, p;
    scope.RegisterInput(q, 2.0);
    scope.RegisterInput(p, 3.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = 2.0 * q;
    rhs(0, 0) = p * p;
    auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
    Number_ objective = 3.0 * result.solution_(0, 0) * result.solution_(0, 0) + 2.0 * p;
    scope.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(result.solution_(0, 0)), 2.25);
    ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 1.0);
    ASSERT_EQ(result.diagnostics_.componentwiseBackwardErrors_.size(), 1);
    ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[0], 0.0);
    for (double seed : {1.0, -2.0}) {
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, seed);
        scope.Reverse();
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), 22.25 * seed, 1e-10);
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -15.1875 * seed, 1e-10);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(result.solution_(0, 0)), 0.0);
    }
    scope.Close();
    Clear(*Tape());
    ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 1.0);
    ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[0], 0.0);
}

TEST(AADLinearSolveTest, TestDiagnosticsThreeActivityCombinationsAndIndependentDifferences) {
    const SolveSample_ sample;
    const Dal::DiagnosedLinearSolve_ numeric(sample.matrix_, sample.rhs_);
    for (size_t width : {0U, 1U, 4U, 8U}) {
        SCOPED_TRACE(width);
        for (int activity : {1, 2, 3}) {
            SCOPED_TRACE(activity);
            Clear(*Tape());
            auto mode = Dal::AAD::SetNumResultsForAAD(width != 0, width == 0 ? 1 : width);
            const size_t channels = std::max(size_t(1), width);
            RecordingScope_ scope;
            const NativeSolveSample_ inputs(&scope, sample);
            scope.StartRecording();
            auto result = SolveSample(&scope, sample, inputs, activity);
            scope.FinishRecording();
            ASSERT_NO_FATAL_FAILURE(CheckAndSeed(&result, sample, numeric, channels));
            scope.Reverse();
            for (double step : {1e-5, 5e-6, 2e-5})
                ASSERT_NO_FATAL_FAILURE(CheckSampleDifferences(sample, inputs, activity, step, channels));
            scope.Close();
            Clear(*Tape());
        }
    }
}

TEST(AADLinearSolveTest, TestDiagnosticsAliasesMultipleRhsAndVectorWidths) {
    for (size_t width : {1U, 4U, 8U}) {
        SCOPED_TRACE(width);
        Clear(*Tape());
        auto mode = Dal::AAD::SetNumResultsForAAD(true, width);
        RecordingScope_ scope;
        Number_ p, q;
        scope.RegisterInput(p, 3.0);
        scope.RegisterInput(q, 1.0);
        scope.StartRecording();
        SquareMatrix_<Number_> matrix(2);
        matrix(0, 0) = matrix(1, 1) = p;
        matrix(0, 1) = matrix(1, 0) = q;
        Matrix_<> rhs(2, 2);
        rhs(0, 0) = 1.0;
        rhs(1, 0) = 2.0;
        rhs(0, 1) = 3.0;
        rhs(1, 1) = 4.0;
        const auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
        Number_ objective = 3.0 * result.solution_(0, 0) - result.solution_(1, 0) + 2.0 * result.solution_(0, 1) + 4.0 * result.solution_(1, 1) + p;
        scope.FinishRecording();
        ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 0.5);
        const auto errors = result.diagnostics_.componentwiseBackwardErrors_;
        for (double error : errors) {
            ASSERT_GE(error, 0.0);
            ASSERT_LT(error, 1e-15);
        }
        for (double multiplier : {1.0, -2.0}) {
            scope.ClearAdjoints();
            for (size_t channel = 0; channel < width; ++channel)
                NativeOperations_::SetSeed(objective, multiplier * (static_cast<int>(channel % 3) - 1), channel);
            scope.Reverse();
            for (size_t channel = 0; channel < width; ++channel) {
                const double seed = multiplier * (static_cast<int>(channel % 3) - 1);
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(p, channel), -0.25 * seed, 1e-10);
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(q, channel), -1.75 * seed, 1e-10);
                for (const auto& output : result.solution_)
                    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(output, channel), 0.0);
            }
        }
        scope.Close();
        Clear(*Tape());
        ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[0], errors[0]);
        ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[1], errors[1]);
    }
}

TEST(AADLinearSolveTest, TestDiagnosticsOneInputAliasesMatrixRhsAndConsumer) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = rhs(0, 0) = q;
    const auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
    Number_ objective = result.solution_(0, 0) * result.solution_(0, 0) + q;
    scope.FinishRecording();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(Value(result.solution_(0, 0)), 1.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 1.0);
    ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 1.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsMixedEventsRestoreAndDetachedReportOwnership) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto prefix = LinearSolveWithDiagnostics(&scope, matrix, rhs);
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    Dal::LinearSolveDiagnostics_ detached;
    for (int suffix = 1; suffix <= 2; ++suffix) {
        scope.Restore(checkpoint);
        rhs(0, 0) = prefix.solution_(0, 0) + suffix;
        Matrix_<Number_> outputs;
        if (suffix == 1)
            outputs = Dal::AAD::LinearSolve(&scope, matrix, rhs);
        else {
            auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
            outputs = std::move(result.solution_);
            detached = std::move(result.diagnostics_);
        }
        const auto retained = Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_;
        matrix(0, 0) = 17.0;
        Number_ objective = outputs(0, 0) * outputs(0, 0);
        scope.FinishRecording();
        NativeOperations_::SetSeed(objective, 1.0);
        scope.ReverseSuffix(checkpoint);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.0);
        scope.Restore(checkpoint);
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 1);
        ASSERT_LT(Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_, retained);
        matrix(0, 0) = 2.0;
    }
    scope.FinishRecording();
    scope.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 1.5);
    scope.Close();
    Clear(*Tape());
    ASSERT_DOUBLE_EQ(prefix.diagnostics_.reciprocalConditionInfinity_, 1.0);
    ASSERT_DOUBLE_EQ(detached.reciprocalConditionInfinity_, 1.0);
    ASSERT_DOUBLE_EQ(detached.componentwiseBackwardErrors_[0], 0.0);
}

TEST(AADLinearSolveTest, TestDiagnosticsCallerBudgetOwnsDetachedReportAndReverseScratch) {
    Clear(*Tape());
    const auto initial = Dal::AAD::MeasureTape(*Tape()).capacityBytes_;
    Dal::AAD::TapeCapacityBudget_ tapeBudget(initial + Dal::AAD::TapeCleanupCapacityBytes() + 1024 * 1024);
    Dal::AAD::TapeCapacityScope_ capacity(&tapeBudget, true);
    Dal::BufferCapacityBudget_ buffers(sizeof(Number_) + 3 * sizeof(double));
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    {
        Dal::BufferCapacityScope_ caller(&buffers);
        {
            auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
            const auto retained = Dal::AAD::MeasureTape(*Tape()).capacityBytes_;
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_) + sizeof(double));
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            scope.FinishRecording();
            NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
            scope.Reverse();
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
            ASSERT_EQ(buffers.PeakCapacityBytes(), sizeof(Number_) + 3 * sizeof(double));
            ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseScratchPeakBytes_, 2 * sizeof(double));
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            scope.Close();
            ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
            result.solution_ = Matrix_<Number_>();
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(double));
            ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 1.0);
            ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[0], 0.0);
        }
        ASSERT_EQ(buffers.CapacityBytes(), 0);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsReportAdmissionFailurePublishesNoOutputsAndRecovers) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto before = Dal::AAD::MeasureTape(*Tape());
    Dal::BufferCapacityBudget_ buffers(sizeof(double) - 1);
    {
        Dal::BufferCapacityScope_ caller(&buffers);
        ASSERT_THROW(static_cast<void>(LinearSolveWithDiagnostics(&scope, matrix, rhs)), Dal::Exception_);
        ASSERT_EQ(buffers.CapacityBytes(), 0);
        const auto after = Dal::AAD::MeasureTape(*Tape());
        ASSERT_EQ(after.nodes_, before.nodes_);
        ASSERT_EQ(after.reverseEvents_, before.reverseEvents_);
        ASSERT_EQ(after.reverseEventCapacityBytes_, before.reverseEventCapacityBytes_);
        ASSERT_THROW(scope.FinishRecording(), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
        scope.Close();
    }
    Clear(*Tape());
    RecordingScope_ recovered;
    recovered.RegisterInput(input, 3.0);
    recovered.StartRecording();
    rhs(0, 0) = input;
    auto result = LinearSolveWithDiagnostics(&recovered, matrix, rhs);
    recovered.FinishRecording();
    NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
    recovered.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsExactTapePeakAndOneByteShortRefund) {
    const auto measured = DiagnosedTapePeak(1024 * 1024);
    ASSERT_GT(measured, 0);
    ASSERT_EQ(DiagnosedTapePeak(measured), measured);
    ASSERT_THROW(static_cast<void>(DiagnosedTapePeak(measured - 1)), Dal::Exception_);
    const auto recovered = DiagnosedTapePeak(measured);
    ASSERT_GE(recovered, measured);
    ASSERT_LE(recovered, measured + Dal::AAD::TapeCleanupCapacityBytes());
    ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 0);
    ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsInverseRangeFailureInvalidatesAndRecovers) {
    Clear(*Tape());
    SquareMatrix_<> matrix(2);
    matrix(0, 0) = 1.0;
    matrix(1, 1) = 1e-310;
    Matrix_<Number_> rhs(2, 1);
    for (bool diagnosed : {false, true}) {
        RecordingScope_ scope;
        for (int row = 0; row < 2; ++row)
            scope.RegisterInput(rhs(row, 0), 0.0);
        scope.StartRecording();
        const auto before = Dal::AAD::MeasureTape(*Tape());
        if (diagnosed) {
            ASSERT_THROW(static_cast<void>(LinearSolveWithDiagnostics(&scope, matrix, rhs, 1e-311)), Dal::Exception_);
            ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).nodes_, before.nodes_);
            ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
            ASSERT_THROW(scope.FinishRecording(), Dal::Exception_);
            ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(rhs(0, 0))), Dal::Exception_);
        } else {
            const auto result = Dal::AAD::LinearSolve(&scope, matrix, rhs, 1e-311);
            scope.FinishRecording();
            ASSERT_DOUBLE_EQ(Value(result(1, 0)), 0.0);
            scope.Reverse();
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(rhs(1, 0)), 0.0);
        }
        scope.Close();
        Clear(*Tape());
    }
    RecordingScope_ recovered;
    Number_ input;
    recovered.RegisterInput(input, 3.0);
    recovered.StartRecording();
    Matrix_<Number_> validRhs(1, 1);
    validRhs(0, 0) = input;
    auto result = LinearSolveWithDiagnostics(&recovered, SquareMatrix_<>(1, 2.0), validRhs);
    recovered.FinishRecording();
    NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), 0.5);
    recovered.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsOutputAdmissionRefundsReportWithoutPublishingNodes) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    const auto nodes = Dal::AAD::MeasureTape(*Tape()).nodes_;
    Dal::BufferCapacityBudget_ buffers(sizeof(Number_) + sizeof(double) - 1);
    {
        Dal::BufferCapacityScope_ caller(&buffers);
        ASSERT_THROW(static_cast<void>(LinearSolveWithDiagnostics(&scope, matrix, rhs)), Dal::Exception_);
        ASSERT_EQ(buffers.CapacityBytes(), 0);
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).nodes_, nodes);
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 0);
        ASSERT_THROW(scope.FinishRecording(), Dal::Exception_);
        scope.Close();
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    }
    Clear(*Tape());
    RecordingScope_ recovered;
    recovered.Close();
}

TEST(AADLinearSolveTest, TestDiagnosticsReverseBufferOneByteShortRefundsScratch) {
    Clear(*Tape());
    const auto initial = Dal::AAD::MeasureTape(*Tape()).capacityBytes_;
    Dal::AAD::TapeCapacityBudget_ tapeBudget(initial + Dal::AAD::TapeCleanupCapacityBytes() + 1024 * 1024);
    Dal::AAD::TapeCapacityScope_ capacity(&tapeBudget, true);
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 2.0);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    Dal::BufferCapacityBudget_ buffers(sizeof(Number_) + 3 * sizeof(double) - 1);
    {
        Dal::BufferCapacityScope_ caller(&buffers);
        {
            auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
            const auto retained = tapeBudget.CapacityBytes();
            scope.FinishRecording();
            NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
            ASSERT_THROW(scope.Reverse(), Dal::Exception_);
            ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
            ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
            ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_) + sizeof(double));
            scope.Close();
            ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
            ASSERT_DOUBLE_EQ(result.diagnostics_.componentwiseBackwardErrors_[0], 0.0);
        }
        ASSERT_EQ(buffers.CapacityBytes(), 0);
    }
    capacity.Close();
    Clear(*Tape());
    RecordingScope_ recovered;
    recovered.Close();
}

TEST(AADLinearSolveTest, TestDiagnosticsPassiveMatrixOmitsUnusedOverflowingContribution) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 1e150);
    scope.StartRecording();
    SquareMatrix_<> matrix(1, 1e-150);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
    scope.FinishRecording();
    NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(result.solution_(0, 0)), 1e300, 1e288);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 1e150, 1e138);
    ASSERT_DOUBLE_EQ(result.diagnostics_.reciprocalConditionInfinity_, 1.0);
    ASSERT_LT(result.diagnostics_.componentwiseBackwardErrors_[0], 1e-15);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestDiagnosticsConcurrentRecordingsReturnDetachedReports) {
    std::vector<std::future<std::pair<double, Dal::LinearSolveDiagnostics_>>> tasks;
    for (int worker = 0; worker < 4; ++worker)
        tasks.emplace_back(std::async(std::launch::async, [worker] {
            Clear(*Tape());
            RecordingScope_ scope;
            Number_ input;
            scope.RegisterInput(input, 3.0 + worker);
            scope.StartRecording();
            SquareMatrix_<> matrix(1, 2.0);
            Matrix_<Number_> rhs(1, 1);
            rhs(0, 0) = input;
            auto result = LinearSolveWithDiagnostics(&scope, matrix, rhs);
            scope.FinishRecording();
            NativeOperations_::SetSeed(result.solution_(0, 0), 1.0);
            scope.Reverse();
            const auto gradient = NativeOperations_::ReadAdjoint(input);
            scope.Close();
            Clear(*Tape());
            return std::make_pair(gradient, std::move(result.diagnostics_));
        }));
    for (auto& task : tasks) {
        const auto result = task.get();
        ASSERT_DOUBLE_EQ(result.first, 0.5);
        ASSERT_DOUBLE_EQ(result.second.reciprocalConditionInfinity_, 1.0);
        ASSERT_DOUBLE_EQ(result.second.componentwiseBackwardErrors_[0], 0.0);
    }
}
