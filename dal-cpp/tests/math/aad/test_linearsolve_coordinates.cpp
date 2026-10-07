//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>

using Dal::LinearSolveCoordinates_;
using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::Vector_;
using Dal::AAD::Clear;
using Dal::AAD::LinearSolve;
using Dal::AAD::NativeOperations_;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::Tape;
using Dal::AAD::Value;

TEST(AADLinearSolveTest, TestCoordinateSymmetricProducersConsumersAndRepeatedSeeds) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q, p;
    scope.RegisterInput(q, 2.0);
    scope.RegisterInput(p, 3.0);
    scope.StartRecording();
    const auto layout = LinearSolveCoordinates_::Symmetric(2);
    Vector_<Number_> parameters{Number_(3.0), 0.5 * q, Number_(2.0)};
    Matrix_<Number_> rhs(2, 1);
    rhs(0, 0) = p - 2.0;
    rhs(1, 0) = p - 1.0;
    const auto solution = LinearSolve(&scope, layout, parameters, rhs);
    Number_ objective = 3.0 * solution(0, 0) - solution(1, 0) + q + p * p;
    scope.FinishRecording();
    ASSERT_NEAR(Value(solution(0, 0)), 0.0, 1e-10);
    ASSERT_NEAR(Value(solution(1, 0)), 1.0, 1e-10);
    ASSERT_NEAR(Value(objective), 10.0, 1e-10);
    for (double seed : {1.0, -2.0, 0.0}) {
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(objective, seed);
        scope.Reverse();
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), 6.2 * seed, 1e-10);
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), 0.3 * seed, 1e-10);
        for (const auto& output : solution)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(output), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(objective), 0.0);
    }
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateAliasedParametersAndMultipleRhs) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ p, q;
    scope.RegisterInput(p, 3.0);
    scope.RegisterInput(q, 1.0);
    scope.StartRecording();
    Vector_<Number_> parameters{p, q, p};
    Matrix_<> rhs(2, 2);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    rhs(0, 1) = 3.0;
    rhs(1, 1) = 4.0;
    const auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs);
    Number_ objective = 3.0 * solution(0, 0) - solution(1, 0) + 2.0 * solution(0, 1) + 4.0 * solution(1, 1) + p;
    scope.FinishRecording();
    ASSERT_NEAR(Value(objective), 8.5, 1e-10);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), -0.25, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -1.75, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateParameterRhsAndConsumerCrossAlias) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    Vector_<Number_> parameters{q};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    const auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Banded(1, 0, 0), parameters, rhs);
    Number_ objective = solution(0, 0) * solution(0, 0) + q;
    scope.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(solution(0, 0)), 1.0);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 1.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateZeroParameterRetainsGradientWithoutFixedZeroNodes) {
    Clear(*Tape());
    RecordingScope_ scope;
    Vector_<Number_> parameters(3);
    scope.RegisterInput(parameters[0], 2.0);
    scope.RegisterInput(parameters[1], 0.0);
    scope.RegisterInput(parameters[2], 3.0);
    scope.StartRecording();
    Matrix_<> rhs(2, 1);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    const auto nodesBefore = Tape()->nodes_.OccupiedSlots();
    auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Banded(2, 1, 0), parameters, rhs);
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots() - nodesBefore, 2);
    ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 1);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(1, 0), 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(parameters[0]), 0.0);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(parameters[1]), -1.0 / 6.0, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(parameters[2]), -2.0 / 9.0, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateSerialSharedAndOrdinaryEventsAccumulate) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    const auto layout = LinearSolveCoordinates_::Symmetric(1);
    Vector_<Number_> parameters{q};
    Matrix_<> rhs(1, 1);
    rhs(0, 0) = 3.0;
    const auto first = LinearSolve(&scope, layout, parameters, rhs);
    Matrix_<Number_> sharedRhs(1, 1);
    sharedRhs(0, 0) = first(0, 0);
    const auto serial = LinearSolve(&scope, layout, parameters, sharedRhs);
    SquareMatrix_<> ordinaryMatrix(1);
    ordinaryMatrix(0, 0) = 2.0;
    sharedRhs(0, 0) = first(0, 0) + 1.0;
    const auto shared = LinearSolve(&scope, ordinaryMatrix, sharedRhs);
    Number_ objective = serial(0, 0) + 2.0 * shared(0, 0) + first(0, 0);
    scope.FinishRecording();
    ASSERT_NEAR(Value(objective), 4.75, 1e-10);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -2.25, 1e-10);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(first(0, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinateSnapshotsSurviveSourceContainerMutation) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q, p;
    scope.RegisterInput(q, 2.0);
    scope.RegisterInput(p, 3.0);
    scope.StartRecording();
    Vector_<Number_> parameters{q};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = p;
    auto solution = LinearSolve(&scope, LinearSolveCoordinates_::Symmetric(1), parameters, rhs);
    parameters.clear();
    rhs.Resize(0, 0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(Value(solution(0, 0)), 1.5);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), -0.75);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(p), 0.5);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestCoordinatePrefixCacheSurvivesMixedSuffixRestore) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q, p;
    scope.RegisterInput(q, 2.0);
    scope.RegisterInput(p, 3.0);
    scope.StartRecording();
    Vector_<Number_> parameters{2.0 * q};
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = p * p;
    const auto layout = LinearSolveCoordinates_::Banded(1, 0, 0);
    const auto prefix = LinearSolve(&scope, layout, parameters, rhs);
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    SquareMatrix_<> ordinaryMatrix(1);
    ordinaryMatrix(0, 0) = 2.0;
    Vector_<> suffixParameters{2.0};
    for (double shift : {1.0, 2.0}) {
        scope.Restore(checkpoint);
        ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 1);
        rhs(0, 0) = prefix(0, 0) + shift;
        const auto suffix = shift == 1.0 ? LinearSolve(&scope, ordinaryMatrix, rhs) : LinearSolve(&scope, layout, suffixParameters, rhs);
        Number_ objective = suffix(0, 0) * suffix(0, 0);
        scope.FinishRecording();
        NativeOperations_::SetSeed(objective, 1.0);
        scope.ReverseSuffix(checkpoint);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(p), 0.0);
    }
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(prefix(0, 0)), 3.75, 1e-10);
    scope.ReversePrefix(checkpoint);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), 5.625, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -4.21875, 1e-10);
    scope.Close();
    ASSERT_EQ(Dal::AAD::MeasureTape(*Tape()).reverseEvents_, 0);
    Clear(*Tape());
}
