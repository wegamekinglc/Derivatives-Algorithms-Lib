//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <exception>
#include <future>
#include <limits>
#include <utility>
#include <vector>

#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    void CheckHealthyRecording() {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 3.0);
        scope.StartRecording();
        Vector_<> parameters{2.0};
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = q;
        auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(solution(0, 0), 1.0);
        scope.Reverse();
        ASSERT_DOUBLE_EQ(Value(solution(0, 0)), 1.5);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 0.5);
        scope.Close();
        Clear(*Tape());
    }

    void CheckBadNumeric(double parameter, double rightHandSide, double tolerance, bool parameterActive = false) {
        Clear(*Tape());
        RecordingScope_ scope;
        Matrix_<Number_> rhs(1, 1);
        scope.RegisterInput(rhs(0, 0), rightHandSide);
        Vector_<Number_> activeParameters(1);
        scope.RegisterInput(activeParameters[0], parameter);
        scope.StartRecording();
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        const auto layout = LinearSolveCoordinates_::Symmetric(1);
        ASSERT_THROW(static_cast<void>(parameterActive ? LinearSolve(&scope, layout, activeParameters, rhs, tolerance)
                                                       : LinearSolve(&scope, layout, Vector_<>{parameter}, rhs, tolerance)),
                     Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 0);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(rhs(0, 0))), Exception_);
        scope.Close();
        Clear(*Tape());
    }

    void CheckBadShape(int variant) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 2.0);
        scope.StartRecording();
        Vector_<Number_> parameters{q};
        Matrix_<Number_> rhs(1, 1);
        rhs(0, 0) = q;
        if (variant == 0)
            parameters.clear();
        else
            rhs.Resize(variant == 1 ? 0 : 2, 1);
        const auto checkpoint = scope.MakeCheckpoint();
        const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
        ASSERT_THROW(static_cast<void>(LinearSolve(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), parameters, rhs)), Exception_);
        ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
        ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
        ASSERT_THROW(scope.Restore(checkpoint), Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q)), Exception_);
        scope.Close();
        Clear(*Tape());
    }

    std::pair<double, double> ConcurrentSolve(double parameter, double seed) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, parameter);
        scope.StartRecording();
        Vector_<Number_> parameters{q};
        Matrix_<> rhs(1, 1);
        rhs(0, 0) = 3.0;
        auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), parameters, rhs);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(solution(0, 0), seed);
        scope.Reverse();
        const auto result = std::make_pair(Value(solution(0, 0)), NativeOperations_::ReadAdjoint(q));
        scope.Close();
        Clear(*Tape());
        return result;
    }
} // namespace

TEST(AADLinearSolveTest, TestCoordinateBadShapesInvalidateWithoutOutputPublication) {
    for (int variant : {0, 1, 2})
        ASSERT_NO_FATAL_FAILURE(CheckBadShape(variant));
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateNonfiniteParametersAndSingularSystemsRefund) {
    const double nan = std::numeric_limits<double>::quiet_NaN(), infinity = std::numeric_limits<double>::infinity();
    for (double parameter : {0.0, nan, infinity, -infinity})
        for (bool active : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckBadNumeric(parameter, 3.0, 64.0 * std::numeric_limits<double>::epsilon(), active));
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateNonfiniteRhsAndInvalidToleranceRefund) {
    const double nan = std::numeric_limits<double>::quiet_NaN(), infinity = std::numeric_limits<double>::infinity();
    for (double rhs : {nan, infinity, -infinity})
        ASSERT_NO_FATAL_FAILURE(CheckBadNumeric(2.0, rhs, 64.0 * std::numeric_limits<double>::epsilon()));
    for (double tolerance : {-1.0, 0.0, nan, infinity})
        ASSERT_NO_FATAL_FAILURE(CheckBadNumeric(2.0, 3.0, tolerance));
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateMissingSlotRejectsBeforeNumericConstruction) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    Vector_<Number_> parameters(1);
    Matrix_<> rhs(1, 1);
    rhs(0, 0) = 3.0;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(static_cast<void>(LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs)), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    scope.Close();
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateNullScopeAndWrongThreadLeaveOwnerUsable) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    const auto layout = LinearSolveCoordinates_::Symmetric(1);
    Vector_<> parameters{2.0};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    ASSERT_THROW(static_cast<void>(LinearSolve(nullptr, layout, parameters, rhs)), Exception_);
    auto rejected = std::async(std::launch::async, [&] {
        try {
            static_cast<void>(LinearSolve(&scope, layout, parameters, rhs));
            return false;
        } catch (const Exception_&) {
            return true;
        }
    });
    ASSERT_TRUE(rejected.get());
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    auto solution = LinearSolve(&scope, layout, parameters, rhs);
    scope.FinishRecording();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 0.5);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateForeignLiveParameterRejectsAndRecovers) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    Vector_<Number_> parameters(1);
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    std::promise<void> ready, release;
    auto readyFuture = ready.get_future(), releaseFuture = release.get_future();
    auto owner = std::async(std::launch::async, [&] {
        Number_ foreign(2.0);
        parameters[0] = foreign;
        ready.set_value();
        releaseFuture.wait();
    });
    readyFuture.wait();
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    std::exception_ptr failure;
    try {
        static_cast<void>(LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs));
    } catch (...) {
        failure = std::current_exception();
    }
    release.set_value();
    owner.get();
    ASSERT_TRUE(failure != nullptr);
    ASSERT_THROW(std::rethrow_exception(failure), Exception_);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodesBefore);
    ASSERT_EQ(MeasureTape(*Tape()).reverseEventCapacityBytes_, 0);
    scope.Close();
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateIndependentConcurrentRecordings) {
    const double seeds[4] = {1.0, -2.0, 0.0, 3.0};
    std::vector<std::future<std::pair<double, double>>> futures;
    for (int worker = 0; worker < 4; ++worker)
        futures.push_back(std::async(std::launch::async, [worker, &seeds] { return ConcurrentSolve(2.0 + worker, seeds[worker]); }));
    for (int worker = 0; worker < 4; ++worker) {
        const auto result = futures[worker].get();
        const double parameter = 2.0 + worker;
        ASSERT_NEAR(result.first, 3.0 / parameter, 1e-10);
        ASSERT_NEAR(result.second, -3.0 * seeds[worker] / (parameter * parameter), 1e-10);
    }
}

TEST(AADLinearSolveTest, TestCoordinateModeChangeRejectsAndRawReverseRemainsValid) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    Vector_<> parameters{2.0};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs);
    ASSERT_THROW(static_cast<void>(SetNumResultsForAAD(true, 4)), Exception_);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), -2.0);
    PropagateToStart(*Tape());
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), -1.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateLaterChannelFailureSuppressesPartialAdjoints) {
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 2.0);
        scope.StartRecording();
        Vector_<Number_> parameters{q};
        Matrix_<> rhs(1, 1);
        rhs(0, 0) = 3.0;
        auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(solution(0, 0), 1.0, 0);
        NativeOperations_::SetSeed(solution(0, 0), std::numeric_limits<double>::infinity(), 1);
        ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q, 0)), Exception_);
        ASSERT_THROW(scope.ClearAdjoints(), Exception_);
        ASSERT_THROW(PropagateToStart(*Tape()), Exception_);
        scope.Close();
    }
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinatePassiveParametersAvoidUnusedGradientOverflow) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 1e150);
    scope.StartRecording();
    Vector_<> parameters{1e-150};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), parameters, rhs);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(solution(0, 0)) / 1e300, 1.0, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q) / 1e150, 1.0, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateRequestedGradientOverflowInvalidatesAllChannels) {
    Clear(*Tape());
    {
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 1e-150);
        scope.StartRecording();
        Vector_<Number_> parameters{q};
        Matrix_<> rhs(1, 1);
        rhs(0, 0) = 1e150;
        auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(solution(0, 0), 1e-200, 0);
        NativeOperations_::SetSeed(solution(0, 0), 1.0, 1);
        ASSERT_THROW(scope.Reverse(), Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q, 0)), Exception_);
        ASSERT_THROW(static_cast<void>(Adjoint(q)), Exception_);
        scope.Close();
    }
    ASSERT_NO_FATAL_FAILURE(CheckHealthyRecording());
}

TEST(AADLinearSolveTest, TestCoordinateExplicitTolerancePreservesLargeFiniteRisk) {
    const auto layout = LinearSolveCoordinates_::Symmetric(2);
    Vector_<> parameters{1.0, 0.0, 1e-16};
    for (bool explicitTolerance : {false, true}) {
        Clear(*Tape());
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 3.0);
        scope.StartRecording();
        Matrix_<Number_> rhs(2, 1);
        rhs(0, 0) = 1.0;
        rhs(1, 0) = q;
        if (!explicitTolerance) {
            ASSERT_THROW(static_cast<void>(LinearSolve(&scope, layout, parameters, rhs)), Exception_);
            ASSERT_EQ(MeasureTape(*Tape()).reverseEvents_, 0);
        } else {
            auto solution = LinearSolve(&scope, layout, parameters, rhs, 1e-18);
            scope.FinishRecording();
            scope.ClearAdjoints();
            NativeOperations_::SetSeed(solution(1, 0), 1.0);
            scope.Reverse();
            ASSERT_NEAR(Value(solution(1, 0)) / 3e16, 1.0, 1e-10);
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(q) / 1e16, 1.0, 1e-10);
        }
        scope.Close();
        Clear(*Tape());
    }
}
