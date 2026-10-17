//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct ResourceRun_ {
        size_t retained_;
        size_t scratch_;
        size_t peak_;
        size_t publishedNodes_;
    };

    Vector_<> Parameters(const LinearSolveCoordinates_& layout) {
        Vector_<> parameters(layout.Count());
        for (size_t index = 0; index < parameters.size(); ++index) {
            const auto [row, column] = layout.Location(index);
            parameters[index] = row == column ? 4.0 + row : 0.125;
        }
        return parameters;
    }

    Matrix_<> RightHandSide(const LinearSolveCoordinates_& layout, const Vector_<>& parameters) {
        const auto matrix = layout.Expand(parameters);
        Matrix_<> rhs(layout.Size(), 2);
        for (int row = 0; row < layout.Size(); ++row) {
            double sum = 0.0;
            for (int column = 0; column < layout.Size(); ++column)
                sum += matrix(row, column);
            rhs(row, 0) = sum;
            rhs(row, 1) = -2.0 * sum;
        }
        return rhs;
    }

    void Register(
        RecordingScope_* scope, const Vector_<>& parameters, const Matrix_<>& rhs, Vector_<Number_>* activeParameters, Matrix_<Number_>* activeRhs) {
        for (size_t index = 0; index < parameters.size(); ++index)
            scope->RegisterInput((*activeParameters)[index], parameters[index]);
        for (int row = 0; row < rhs.Rows(); ++row)
            for (int column = 0; column < rhs.Cols(); ++column)
                scope->RegisterInput((*activeRhs)(row, column), rhs(row, column));
    }

    Matrix_<Number_> Solve(RecordingScope_* scope,
                           const LinearSolveCoordinates_& layout,
                           const Vector_<>& parameters,
                           const Matrix_<>& rhs,
                           const Vector_<Number_>& activeParameters,
                           const Matrix_<Number_>& activeRhs,
                           int activity) {
        return activity == 1   ? LinearSolve(scope, layout, activeParameters, rhs)
               : activity == 2 ? LinearSolve(scope, layout, parameters, activeRhs)
                               : LinearSolve(scope, layout, activeParameters, activeRhs);
    }

    void Seed(Matrix_<Number_>* solution) {
        for (int row = 0; row < solution->Rows(); ++row)
            for (int column = 0; column < solution->Cols(); ++column) {
                REQUIRE(std::abs(Value((*solution)(row, column)) - (column == 0 ? 1.0 : -2.0)) < 1e-10, "Coordinate resource solution changed");
                NativeOperations_::SetSeed((*solution)(row, column), 1.0);
            }
    }

    ResourceRun_ RunCoordinateResource(const LinearSolveCoordinates_& layout, int activity, size_t extraLimit = 1024 * 1024, size_t width = 0) {
        Clear(*Tape());
        // Recover the previous failed recording before measuring this operator's storage.
        {
            RecordingScope_ recovery;
            recovery.Close();
        }
        auto mode = SetNumResultsForAAD(width != 0, width == 0 ? 1 : width);
        const auto initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + extraLimit);
        TapeCapacityScope_ capacity(&budget, true);
        RecordingScope_ scope;
        const auto parameters = Parameters(layout);
        const auto rhs = RightHandSide(layout, parameters);
        Vector_<Number_> activeParameters(parameters.size());
        Matrix_<Number_> activeRhs(rhs.Rows(), rhs.Cols());
        Register(&scope, parameters, rhs, &activeParameters, &activeRhs);
        scope.StartRecording();
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        auto solution = Solve(&scope, layout, parameters, rhs, activeParameters, activeRhs, activity);
        ResourceRun_ result{};
        result.retained_ = MeasureTape(*Tape()).reverseEventCapacityBytes_;
        result.publishedNodes_ = Tape()->nodes_.OccupiedSlots() - nodesBefore;
        scope.FinishRecording();
        scope.ClearAdjoints();
        Seed(&solution);
        scope.Reverse();
        result.scratch_ = MeasureTape(*Tape()).reverseScratchPeakBytes_;
        result.peak_ = budget.PeakCapacityBytes() - initial;
        REQUIRE(budget.CapacityBytes() == MeasureTape(*Tape()).capacityBytes_, "Coordinate resource accounting diverged");
        scope.Close();
        REQUIRE(budget.CapacityBytes() == initial, "Coordinate resource close did not refund storage");
        return result;
    }
} // namespace

TEST(AADLinearSolveTest, TestCoordinateBindingStorageScalesWithActualParameters) {
    for (const auto& layout : {LinearSolveCoordinates_::Banded(4, 0, 0), LinearSolveCoordinates_::Banded(4, 1, 1),
                               LinearSolveCoordinates_::Banded(4, 3, 3), LinearSolveCoordinates_::Symmetric(4)}) {
        SCOPED_TRACE(layout.Count());
        const auto both = RunCoordinateResource(layout, 3), passiveParameters = RunCoordinateResource(layout, 2),
                   passiveRhs = RunCoordinateResource(layout, 1);
        ASSERT_EQ(both.retained_ - passiveParameters.retained_, layout.Count() * sizeof(Number_));
        ASSERT_EQ(both.retained_ - passiveRhs.retained_, 8 * sizeof(Number_));
        ASSERT_EQ(both.publishedNodes_, 8);
        ASSERT_EQ(passiveParameters.publishedNodes_, 8);
        ASSERT_EQ(passiveRhs.publishedNodes_, 8);
        ASSERT_EQ(both.scratch_, (16 + layout.Count()) * sizeof(double));
        ASSERT_EQ(passiveParameters.scratch_, 16 * sizeof(double));
        ASSERT_EQ(passiveRhs.scratch_, both.scratch_);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateExactTapePeakAndOneByteShortRefund) {
    const auto layout = LinearSolveCoordinates_::Banded(4, 1, 1);
    const auto measured = RunCoordinateResource(layout, 3);
    const auto exact = RunCoordinateResource(layout, 3, measured.peak_);
    ASSERT_EQ(exact.peak_, measured.peak_);
    ASSERT_EQ(exact.retained_, measured.retained_);
    ASSERT_THROW(static_cast<void>(RunCoordinateResource(layout, 3, measured.peak_ - 1)), Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    ASSERT_EQ(RunCoordinateResource(layout, 3, measured.peak_).peak_, measured.peak_);
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateVectorChannelsRetainBoundedScratch) {
    const auto layout = LinearSolveCoordinates_::Symmetric(4);
    const auto scalar = RunCoordinateResource(layout, 3);
    for (size_t width : {1U, 4U, 8U}) {
        const auto multi = RunCoordinateResource(layout, 3, 1024 * 1024, width);
        ASSERT_EQ(multi.scratch_, scalar.scratch_);
        ASSERT_EQ(multi.retained_, scalar.retained_);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateCallerOwnsOutputsAndExactScratchAdmission) {
    const auto layout = LinearSolveCoordinates_::Banded(1, 0, 0);
    const size_t exact = sizeof(Number_) + 3 * sizeof(double);
    for (size_t limit : {exact, exact - 1}) {
        Clear(*Tape());
        const auto initial = MeasureTape(*Tape()).capacityBytes_;
        TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
        TapeCapacityScope_ capacity(&tapeBudget, true);
        BufferCapacityBudget_ buffers(limit);
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 2.0);
        scope.StartRecording();
        Vector_<Number_> parameters{q};
        Matrix_<> rhs(1, 1);
        rhs(0, 0) = 3.0;
        {
            BufferCapacityScope_ caller(&buffers);
            {
                auto solution = LinearSolve(&scope, layout, parameters, rhs);
                ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
                const auto retained = tapeBudget.CapacityBytes();
                scope.FinishRecording();
                scope.ClearAdjoints();
                NativeOperations_::SetSeed(solution(0, 0), 1.0);
                if (limit == exact) {
                    scope.Reverse();
                    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), -0.75);
                    ASSERT_EQ(buffers.PeakCapacityBytes(), exact);
                    ASSERT_EQ(MeasureTape(*Tape()).reverseScratchPeakBytes_, 3 * sizeof(double));
                } else {
                    ASSERT_THROW(scope.Reverse(), Exception_);
                    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q)), Exception_);
                }
                ASSERT_EQ(buffers.CapacityBytes(), sizeof(Number_));
                ASSERT_EQ(tapeBudget.CapacityBytes(), retained);
                scope.Close();
                ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
            }
            ASSERT_EQ(buffers.CapacityBytes(), 0);
        }
    }
    ASSERT_NO_THROW(static_cast<void>(RunCoordinateResource(LinearSolveCoordinates_::Banded(4, 0, 0), 3)));
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateOutputBudgetFailurePublishesNoSlots) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ tapeBudget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
    TapeCapacityScope_ capacity(&tapeBudget, true);
    BufferCapacityBudget_ buffers(sizeof(Number_) - 1);
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    Vector_<> parameters{2.0};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    {
        BufferCapacityScope_ caller(&buffers);
        ASSERT_THROW(static_cast<void>(LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs)), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 0);
        ASSERT_EQ(buffers.CapacityBytes(), 0);
        ASSERT_EQ(tapeBudget.CapacityBytes(), MeasureTape(*Tape()).capacityBytes_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q)), Exception_);
        scope.Close();
        ASSERT_EQ(tapeBudget.CapacityBytes(), initial);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateRestoreRefundsSuffixAndRetainsPrefixCache) {
    Clear(*Tape());
    const auto initial = MeasureTape(*Tape()).capacityBytes_;
    TapeCapacityBudget_ budget(initial + TapeCleanupCapacityBytes() + 1024 * 1024);
    TapeCapacityScope_ capacity(&budget, true);
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    const auto layout = LinearSolveCoordinates_::Symmetric(1);
    Vector_<> parameters{2.0};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    const auto prefix = LinearSolve(&scope, layout, parameters, rhs);
    const auto checkpoint = scope.MakeCheckpoint();
    const auto prefixBytes = MeasureTape(*Tape()).reverseEventCapacityBytes_;
    scope.ClearAdjoints();
    size_t restoredBytes = 0;
    for (int replay = 0; replay < 2; ++replay) {
        rhs(0, 0) = prefix(0, 0);
        auto suffix = LinearSolve(&scope, layout, parameters, rhs);
        const auto bothBytes = MeasureTape(*Tape()).reverseEventCapacityBytes_;
        scope.FinishRecording();
        NativeOperations_::SetSeed(suffix(0, 0), 1.0);
        scope.ReverseSuffix(checkpoint);
        scope.Restore(checkpoint);
        const auto currentBytes = MeasureTape(*Tape()).reverseEventCapacityBytes_;
        ASSERT_GE(currentBytes, prefixBytes);
        ASSERT_LT(currentBytes, bothBytes);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 1);
        ASSERT_EQ(budget.CapacityBytes(), MeasureTape(*Tape()).capacityBytes_);
        if (replay == 0)
            restoredBytes = currentBytes;
        else
            ASSERT_EQ(currentBytes, restoredBytes);
    }
    scope.FinishRecording();
    scope.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 0.5);
    scope.Close();
    ASSERT_EQ(budget.CapacityBytes(), initial);
    Clear(*Tape());
}
