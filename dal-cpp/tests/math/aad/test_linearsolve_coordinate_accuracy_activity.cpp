//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <dal/math/aad/linearsolveaccuracy.hpp>
#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveAccuracyPolicy_;
using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::AAD::CheckedLinearSolveResult_;
using Dal::AAD::Clear;
using Dal::AAD::LinearSolveWithAccuracy;
using Dal::AAD::NativeOperations_;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::ReverseWithSolveAccuracy;
using Dal::AAD::Tape;
using Dal::AAD::Value;

namespace {
    double Weight(size_t channel) {
        const double weights[] = {1.0, -2.0, 0.0, 3.0};
        return weights[channel % 4];
    }

    CheckedLinearSolveResult_ ActivitySolve(RecordingScope_* scope, const Number_& input, int activity) {
        const auto layout = LinearSolveCoordinates_::Banded(2, 0, 1);
        const Vector_<> parameters{2.0, 1.0, 4.0};
        const Vector_<Number_> active{Number_(2.0), input, Number_(4.0)};
        Matrix_<> rhs(2, 1);
        rhs(0, 0) = 5.0;
        rhs(1, 0) = 8.0;
        Matrix_<Number_> activeRhs(2, 1);
        activeRhs(0, 0) = input;
        activeRhs(1, 0) = 8.0;
        const LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
        return activity == 1   ? LinearSolveWithAccuracy(scope, layout, active, rhs, policy)
               : activity == 2 ? LinearSolveWithAccuracy(scope, layout, parameters, activeRhs, policy)
                               : LinearSolveWithAccuracy(scope, layout, active, activeRhs, policy);
    }

    void CheckActivity(size_t width, int activity) {
        SCOPED_TRACE(width);
        SCOPED_TRACE(activity);
        Clear(*Tape());
        const size_t channels = std::max(size_t(1), width);
        auto mode = Dal::AAD::SetNumResultsForAAD(width != 0, channels);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, activity == 2 ? 5.0 : 1.0);
        scope.StartRecording();
        const auto checked = ActivitySolve(&scope, input, activity);
        Number_ objective = 3.0 * checked.solution_(0, 0) - checked.solution_(1, 0);
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < channels; ++channel)
            NativeOperations_::SetSeed(objective, Weight(channel), channel);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        const auto& report = reports.Report(checked.event_);
        ASSERT_EQ(reports.Channels(), channels);
        ASSERT_EQ(reports.IsMulti(), width != 0);
        ASSERT_EQ(report.invocationId_, reports.InvocationId());
        ASSERT_EQ(report.transposeBackwardErrors_.Rows(), 1);
        ASSERT_EQ(report.transposeBackwardErrors_.Cols(), static_cast<int>(channels));
        ASSERT_DOUBLE_EQ(Value(objective), activity == 0 ? -3.5 : 2.5);
        const double gradients[] = {-1.5, -3.0, 1.5};
        for (size_t channel = 0; channel < channels; ++channel) {
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, channel), gradients[activity] * Weight(channel));
            ASSERT_DOUBLE_EQ(report.transposeBackwardErrors_(0, static_cast<int>(channel)), 0.0);
        }
        scope.Close();
        Clear(*Tape());
    }

    struct Resources_ {
        size_t retained_;
        size_t scratch_;
        size_t caller_;
        size_t callerPeak_;
    };

    void RegisterDiagonalInputs(
        RecordingScope_* scope, const Vector_<>& parameters, Vector_<Number_>* active, Matrix_<>* rhs, Matrix_<Number_>* activeRhs) {
        for (int row = 0; row < 4; ++row) {
            scope->RegisterInput((*active)[row], parameters[row]);
            for (int column = 0; column < 2; ++column) {
                (*rhs)(row, column) = parameters[row] * (column == 0 ? 1.0 : -2.0);
                scope->RegisterInput((*activeRhs)(row, column), (*rhs)(row, column));
            }
        }
    }

    void SeedOutputs(Matrix_<Number_>* outputs, size_t channels) {
        for (int row = 0; row < outputs->Rows(); ++row)
            for (int column = 0; column < outputs->Cols(); ++column)
                for (size_t channel = 0; channel < channels; ++channel)
                    NativeOperations_::SetSeed((*outputs)(row, column), Weight(channel), channel);
    }

    Resources_ PackedResources(int activity, size_t width, size_t callerLimit = 1024 * 1024) {
        Clear(*Tape());
        const size_t channels = std::max(size_t(1), width);
        auto mode = Dal::AAD::SetNumResultsForAAD(width != 0, channels);
        RecordingScope_ scope;
        const auto layout = LinearSolveCoordinates_::Banded(4, 0, 0);
        const Vector_<> parameters{1.0, 2.0, 4.0, 8.0};
        Vector_<Number_> active(4);
        Matrix_<> rhs(4, 2);
        Matrix_<Number_> activeRhs(4, 2);
        RegisterDiagonalInputs(&scope, parameters, &active, &rhs, &activeRhs);
        scope.StartRecording();
        const LinearSolveAccuracyPolicy_ policy{0.0, 0.0};
        const auto before = Dal::AAD::MeasureTape(*Tape());
        auto checked = activity == 1   ? LinearSolveWithAccuracy(&scope, layout, active, rhs, policy)
                       : activity == 2 ? LinearSolveWithAccuracy(&scope, layout, parameters, activeRhs, policy)
                                       : LinearSolveWithAccuracy(&scope, layout, active, activeRhs, policy);
        REQUIRE(Dal::AAD::MeasureTape(*Tape()).nodes_ - before.nodes_ == 8, "Packed checked capture must publish only its outputs");
        scope.FinishRecording();
        scope.ClearAdjoints();
        SeedOutputs(&checked.solution_, channels);
        Dal::BufferCapacityBudget_ budget(callerLimit);
        Dal::BufferCapacityScope_ caller(&budget);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        for (double error : reports.Report(checked.event_).transposeBackwardErrors_)
            REQUIRE(error == 0.0, "Diagonal physical report must be exact");
        const auto tape = Dal::AAD::MeasureTape(*Tape());
        const Resources_ result{tape.reverseEventCapacityBytes_, tape.reverseScratchPeakBytes_, budget.CapacityBytes(), budget.PeakCapacityBytes()};
        scope.Close();
        return result;
    }
} // namespace

TEST(AADLinearSolveTest, TestCheckedCoordinateActivityAliasesAndChannelAxes) {
    for (size_t width : {0U, 1U, 4U, 8U})
        for (int activity = 0; activity < 3; ++activity)
            ASSERT_NO_FATAL_FAILURE(CheckActivity(width, activity));
}

TEST(AADLinearSolveTest, TestCheckedCoordinateAliasedParametersAndMultipleRhs) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ diagonal, offDiagonal;
    scope.RegisterInput(diagonal, 3.0);
    scope.RegisterInput(offDiagonal, 1.0);
    scope.StartRecording();
    Vector_<Number_> parameters{diagonal, offDiagonal, diagonal};
    Matrix_<> rhs(2, 2);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    rhs(0, 1) = 3.0;
    rhs(1, 1) = 4.0;
    const auto checked =
        LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs, LinearSolveAccuracyPolicy_{1e-12, 1e-12});
    Number_ objective =
        3.0 * checked.solution_(0, 0) - checked.solution_(1, 0) + 2.0 * checked.solution_(0, 1) + 4.0 * checked.solution_(1, 1) + diagonal;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(Value(objective), 8.5);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(diagonal), -0.25, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(offDiagonal), -1.75, 1e-10);
    ASSERT_EQ(reports.Report(checked.event_).transposeBackwardErrors_.Rows(), 2);
    for (double error : reports.Report(checked.event_).transposeBackwardErrors_)
        ASSERT_LE(error, 1e-12);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateZeroSymmetricParameterKeepsBothContributions) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    Vector_<Number_> parameters{Number_(3.0), input, Number_(2.0)};
    Matrix_<> rhs(2, 1);
    rhs(0, 0) = 3.0;
    rhs(1, 0) = 2.0;
    auto checked = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs, LinearSolveAccuracyPolicy_{0.0, 0.0});
    Number_ objective = 3.0 * checked.solution_(0, 0) + 2.0 * checked.solution_(1, 0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_DOUBLE_EQ(Value(objective), 5.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input), -2.0);
    ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateRationalLimitsKeepActualChannelErrors) {
    const double delta = std::ldexp(1.0, -27), error = std::ldexp(1.0, -55);
    for (double rhsValue : {0.0, 1.0}) {
        Clear(*Tape());
        auto mode = Dal::AAD::SetNumResultsForAAD(true, 4);
        RecordingScope_ scope;
        Number_ input;
        scope.RegisterInput(input, rhsValue);
        scope.StartRecording();
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = input;
        auto checked = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), Vector_<>{1.0 + delta}, rhs,
                                               LinearSolveAccuracyPolicy_{error, error});
        ASSERT_DOUBLE_EQ(checked.diagnostics_.componentwiseBackwardErrors_[0], rhsValue == 0.0 ? 0.0 : error);
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t channel = 0; channel < 4; ++channel)
            NativeOperations_::SetSeed(checked.solution_(0, 0), Weight(channel), channel);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        for (size_t channel = 0; channel < 4; ++channel) {
            ASSERT_DOUBLE_EQ(reports.Report(checked.event_).transposeBackwardErrors_(0, static_cast<int>(channel)), channel == 2 ? 0.0 : error);
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(input, channel), Weight(channel) * (1.0 - delta));
        }
        scope.Close();
        Clear(*Tape());
    }
}

TEST(AADLinearSolveTest, TestCheckedCoordinateBindingAndScratchUsePackedExtent) {
    const auto both = PackedResources(0, 0), onlyParameters = PackedResources(1, 0), onlyRhs = PackedResources(2, 0);
    ASSERT_EQ(both.retained_ - onlyRhs.retained_, 4 * sizeof(Number_));
    ASSERT_EQ(both.retained_ - onlyParameters.retained_, 8 * sizeof(Number_));
    ASSERT_EQ(both.scratch_, 22 * sizeof(double));
    ASSERT_EQ(onlyParameters.scratch_, both.scratch_);
    ASSERT_EQ(onlyRhs.scratch_, 18 * sizeof(double));
    const size_t retainedReport = sizeof(Dal::AAD::SolveAccuracyReport_) + 2 * sizeof(double);
    ASSERT_EQ(both.caller_, retainedReport);
    ASSERT_EQ(both.callerPeak_, retainedReport + 22 * sizeof(double));
    ASSERT_EQ(PackedResources(0, 0, both.callerPeak_).callerPeak_, both.callerPeak_);
    for (size_t width : {1U, 4U, 8U}) {
        const auto multi = PackedResources(0, width);
        ASSERT_EQ(multi.retained_, both.retained_);
        ASSERT_EQ(multi.scratch_, both.scratch_);
        ASSERT_EQ(multi.caller_, sizeof(Dal::AAD::SolveAccuracyReport_) + 2 * width * sizeof(double));
        ASSERT_EQ(multi.callerPeak_, multi.caller_ + both.scratch_);
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinateReportOrPackedScratchOneByteShortRefunds) {
    const size_t retainedReport = sizeof(Dal::AAD::SolveAccuracyReport_) + 2 * sizeof(double);
    for (size_t limit : {retainedReport - 1, retainedReport + 22 * sizeof(double) - 1}) {
        ASSERT_THROW(static_cast<void>(PackedResources(0, 0, limit)), Dal::Exception_);
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_EQ(PackedResources(0, 0).scratch_, 22 * sizeof(double));
    }
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinatePassiveParametersOmitUnusedOverflow) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 1e150);
    scope.StartRecording();
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto checked =
        LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(1), Vector_<>{1e-150}, rhs, LinearSolveAccuracyPolicy_{1.0, 1.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(checked.solution_(0, 0), 1.0);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    ASSERT_TRUE(std::isfinite(Value(checked.solution_(0, 0))));
    ASSERT_TRUE(std::isfinite(NativeOperations_::ReadAdjoint(input)));
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input) / 1e150, 1.0, 1e-10);
    ASSERT_LE(reports.Report(checked.event_).transposeBackwardErrors_(0, 0), 1.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCheckedCoordinatePairedOverflowRejectsWithoutPartialReport) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 0.0);
    scope.StartRecording();
    Vector_<Number_> parameters{Number_(1.0), input, Number_(1.0)};
    auto checked = LinearSolveWithAccuracy(&scope, LinearSolveCoordinates_::Symmetric(2), parameters, Matrix_<>(2, 1, 1.0),
                                           LinearSolveAccuracyPolicy_{0.0, 0.0});
    scope.FinishRecording();
    scope.ClearAdjoints();
    for (int row = 0; row < checked.solution_.Rows(); ++row)
        NativeOperations_::SetSeed(checked.solution_(row, 0), 0.75 * std::numeric_limits<double>::max());
    Dal::BufferCapacityBudget_ budget(4096);
    {
        Dal::BufferCapacityScope_ caller(&budget);
        Dal::AAD::SolveAccuracyReports_ unpublished;
        ASSERT_THROW(unpublished = ReverseWithSolveAccuracy(&scope), Dal::Exception_);
        ASSERT_TRUE(unpublished.Entries().empty());
        ASSERT_EQ(unpublished.InvocationId(), 0);
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
    ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(input)), Dal::Exception_);
    ASSERT_THROW(scope.Reverse(), Dal::Exception_);
    scope.Close();
    Clear(*Tape());
}
