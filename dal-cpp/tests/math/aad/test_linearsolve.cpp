//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>

using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::AAD::Clear;
using Dal::AAD::LinearSolve;
using Dal::AAD::NativeOperations_;
using Dal::AAD::Number_;
using Dal::AAD::RecordingScope_;
using Dal::AAD::Tape;
using Dal::AAD::Value;

TEST(AADLinearSolveTest, TestScalarProducersConsumersAndRepeatedNegativeSeed) {
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
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = 3.0 * solution(0, 0) * solution(0, 0) + 2.0 * p;
    scope.FinishRecording();
    ASSERT_NEAR(Value(solution(0, 0)), 2.25, 1e-10);
    ASSERT_NEAR(Value(objective), 21.1875, 1e-10);

    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), 22.25, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -15.1875, 1e-10);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 0.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(objective), 0.0);

    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, -2.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), -44.5, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), 30.375, 1e-10);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestAliasedMatrixEntriesAndMultipleRhs) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ p, q;
    scope.RegisterInput(p, 3.0);
    scope.RegisterInput(q, 1.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(2);
    matrix(0, 0) = matrix(1, 1) = p;
    matrix(0, 1) = matrix(1, 0) = q;
    Matrix_<Number_> rhs(2, 2);
    rhs(0, 0) = 1.0;
    rhs(1, 0) = 2.0;
    rhs(0, 1) = 3.0;
    rhs(1, 1) = 4.0;
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = 3.0 * solution(0, 0) - solution(1, 0) + 2.0 * solution(0, 1) + 4.0 * solution(1, 1) + p;
    scope.FinishRecording();
    ASSERT_NEAR(Value(solution(0, 0)), 0.125, 1e-10);
    ASSERT_NEAR(Value(solution(1, 0)), 0.625, 1e-10);
    ASSERT_NEAR(Value(solution(0, 1)), 0.625, 1e-10);
    ASSERT_NEAR(Value(solution(1, 1)), 1.125, 1e-10);
    ASSERT_NEAR(Value(objective), 8.5, 1e-10);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), -0.25, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -1.75, 1e-10);
    for (const auto& output : solution)
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(output), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestOneInputAliasesMatrixRhsAndConsumer) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    Matrix_<Number_> rhs(1, 1);
    matrix(0, 0) = rhs(0, 0) = q;
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = solution(0, 0) * solution(0, 0) + q;
    scope.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(solution(0, 0)), 1.0);
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 1.0);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestVectorChannelsUseDistinctAndZeroSeeds) {
    for (size_t width : {1U, 2U, 3U, 4U, 8U}) {
        SCOPED_TRACE(width);
        Clear(*Tape());
        auto mode = Dal::AAD::SetNumResultsForAAD(true, width);
        RecordingScope_ scope;
        Number_ q, p;
        scope.RegisterInput(q, 2.0);
        scope.RegisterInput(p, 3.0);
        scope.StartRecording();
        SquareMatrix_<Number_> matrix(1);
        Matrix_<Number_> rhs(1, 1);
        matrix(0, 0) = 2.0 * q;
        rhs(0, 0) = p * p;
        auto solution = LinearSolve(&scope, matrix, rhs);
        Number_ objective = 3.0 * solution(0, 0) * solution(0, 0) + 2.0 * p;
        scope.FinishRecording();
        for (double multiplier : {1.0, -2.0}) {
            scope.ClearAdjoints();
            for (size_t channel = 0; channel < width; ++channel) {
                const double seed = multiplier * (static_cast<int>(channel % 3) - 1);
                NativeOperations_::SetSeed(objective, seed, channel);
            }
            scope.Reverse();
            for (size_t channel = 0; channel < width; ++channel) {
                const double seed = multiplier * (static_cast<int>(channel % 3) - 1);
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(p, channel), 22.25 * seed, 1e-10);
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(q, channel), -15.1875 * seed, 1e-10);
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(solution(0, 0), channel), 0.0);
            }
        }
        scope.Close();
        Clear(*Tape());
    }
}

TEST(AADLinearSolveTest, TestPrefixCacheSurvivesRepeatedSuffixSolves) {
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
    auto prefix = LinearSolve(&scope, matrix, rhs);
    const auto checkpoint = scope.MakeCheckpoint();
    scope.ClearAdjoints();
    for (double shift : {1.0, 2.0}) {
        scope.Restore(checkpoint);
        SquareMatrix_<Number_> suffixMatrix(1);
        Matrix_<Number_> suffixRhs(1, 1);
        suffixMatrix(0, 0) = 2.0;
        suffixRhs(0, 0) = prefix(0, 0) + shift;
        auto suffix = LinearSolve(&scope, suffixMatrix, suffixRhs);
        Number_ objective = suffix(0, 0) * suffix(0, 0);
        scope.FinishRecording();
        NativeOperations_::SetSeed(objective, 1.0);
        scope.ReverseSuffix(checkpoint);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(p), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(suffix(0, 0)), 0.0);
    }
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(prefix(0, 0)), 3.75, 1e-10);
    scope.ReversePrefix(checkpoint);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(p), 5.625, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -4.21875, 1e-10);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(prefix(0, 0)), 0.0);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestLateChannelFailureHidesPartialGradientsAndRecovers) {
    Clear(*Tape());
    {
        auto mode = Dal::AAD::SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        Number_ q;
        scope.RegisterInput(q, 2.0);
        scope.StartRecording();
        SquareMatrix_<Number_> matrix(1);
        Matrix_<Number_> rhs(1, 1);
        matrix(0, 0) = q;
        rhs(0, 0) = 3.0;
        auto solution = LinearSolve(&scope, matrix, rhs);
        scope.FinishRecording();
        scope.ClearAdjoints();
        NativeOperations_::SetSeed(solution(0, 0), 1.0, 0);
        NativeOperations_::SetSeed(solution(0, 0), std::numeric_limits<double>::infinity(), 1);
        ASSERT_THROW(scope.Reverse(), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(NativeOperations_::ReadAdjoint(q, 0)), Dal::Exception_);
        ASSERT_THROW(static_cast<void>(Dal::AAD::Adjoint(q)), Dal::Exception_);
        ASSERT_THROW(Dal::AAD::PropagateToStart(*Tape()), Dal::Exception_);
        ASSERT_THROW(Dal::AAD::ZeroAdjoints(*Tape()), Dal::Exception_);
        ASSERT_THROW(scope.ClearAdjoints(), Dal::Exception_);
        scope.Close();
        Clear(*Tape());
    }
    RecordingScope_ recovered;
    Number_ q;
    recovered.RegisterInput(q, 2.0);
    recovered.StartRecording();
    Number_ objective = q * q;
    recovered.FinishRecording();
    NativeOperations_::SetSeed(objective, 1.0);
    recovered.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(q), 4.0);
    recovered.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestPassiveMatrixRetainsRhsAndScalarContributions) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 3.0);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 2.0;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = q;
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = solution(0, 0) * solution(0, 0) + q;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(solution(0, 0)), 1.5, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), 2.5, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestPassiveRhsProducesMatrixContribution) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ q;
    scope.RegisterInput(q, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> matrix(1);
    matrix(0, 0) = q;
    Matrix_<> rhs(1, 1);
    rhs(0, 0) = 3.0;
    auto solution = LinearSolve(&scope, matrix, rhs);
    Number_ objective = solution(0, 0) * solution(0, 0);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(objective, 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(solution(0, 0)), 1.5, 1e-10);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(q), -2.25, 1e-10);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestPassiveMatrixAvoidsUnusedGradientOverflow) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 1e150);
    scope.StartRecording();
    SquareMatrix_<> matrix(1);
    matrix(0, 0) = 1e-150;
    Matrix_<Number_> rhs(1, 1);
    rhs(0, 0) = input;
    auto solution = LinearSolve(&scope, matrix, rhs);
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(solution(0, 0), 1.0);
    scope.Reverse();
    ASSERT_NEAR(Value(solution(0, 0)), 1e300, 1e288);
    ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), 1e150, 1e138);
    scope.Close();
    Clear(*Tape());
}

TEST(AADLinearSolveTest, TestSerialAndSharedSolvesAccumulateDirectSeedsAndRepeatedSweeps) {
    Clear(*Tape());
    RecordingScope_ scope;
    Number_ input;
    scope.RegisterInput(input, 2.0);
    scope.StartRecording();
    SquareMatrix_<Number_> firstMatrix(1), secondMatrix(1), thirdMatrix(1);
    Matrix_<Number_> firstRhs(1, 1), secondRhs(1, 1), thirdRhs(1, 1);
    firstMatrix(0, 0) = input;
    firstRhs(0, 0) = 3.0;
    auto first = LinearSolve(&scope, firstMatrix, firstRhs);
    secondMatrix(0, 0) = input + 1.0;
    secondRhs(0, 0) = input * input;
    auto second = LinearSolve(&scope, secondMatrix, secondRhs);
    thirdMatrix(0, 0) = first(0, 0) + 1.0;
    thirdRhs(0, 0) = second(0, 0);
    auto third = LinearSolve(&scope, thirdMatrix, thirdRhs);
    Number_ objective = third(0, 0) * third(0, 0) + input + 2.0 * first(0, 0);
    scope.FinishRecording();
    ASSERT_NEAR(Value(third(0, 0)), 8.0 / 15.0, 1e-10);
    ASSERT_NEAR(Value(objective), 5.0 + 64.0 / 225.0, 1e-10);
    scope.ClearAdjoints();
    for (int sweep = 1; sweep <= 2; ++sweep) {
        NativeOperations_::SetSeed(objective, 1.0);
        NativeOperations_::SetSeed(first(0, 0), 0.5);
        NativeOperations_::SetSeed(second(0, 0), -0.25);
        scope.Reverse();
        ASSERT_NEAR(NativeOperations_::ReadAdjoint(input), sweep * (-14777.0 / 27000.0), 1e-10);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(first(0, 0)), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(second(0, 0)), 0.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(third(0, 0)), 0.0);
    }
    scope.Close();
    Clear(*Tape());
}
