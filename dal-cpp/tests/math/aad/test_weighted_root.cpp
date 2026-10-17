//
// Created by Codex on 2026-10-06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/weightedroot.hpp>

using namespace Dal;
using namespace Dal::AAD;

TEST(AADWeightedRootTest, TestAnalyticWeightedObjective) {
    RecordingScope_ recording;
    Number_ x, y;
    recording.RegisterInput(x, 2.0);
    recording.RegisterInput(y, 3.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    const Vector_<Number_> outputs = {x * y, x + y, Number_(5.0)};
    Number_ root = WeightedPayoffRoot(outputs, {2.0, -1.0, 0.5});
    ASSERT_NEAR(Value(root), 9.5, 1.0e-10);
    recording.FinishRecording();
    Adjoint(root) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_NEAR(AdjointValue(x), 5.0, 1.0e-10);
    ASSERT_NEAR(AdjointValue(y), 3.0, 1.0e-10);
}

TEST(AADWeightedRootTest, TestAliasedPrefixOutputsAccumulateAcrossPathsAndRequests) {
    const Vector_<Vector_<double>> weights = {{2.0, 3.0}, {0.0, -2.0}};
    const Vector_<double> expectedValues = {30.0, -12.0};
    const Vector_<double> expectedX = {15.0, -6.0};
    const Vector_<double> expectedY = {10.0, -4.0};
    constexpr size_t PATHS = 257;
    for (size_t request = 0; request < weights.size(); ++request) {
        RecordingScope_ recording;
        Number_ x, y;
        recording.RegisterInput(x, 2.0);
        recording.RegisterInput(y, 3.0);
        recording.StartRecording();
        const Number_ shared = x * y;
        const Vector_<Number_> outputs = {shared, shared};
        const auto checkpoint = recording.MakeCheckpoint();
        for (size_t path = 0; path < PATHS; ++path) {
            recording.Restore(checkpoint);
            const auto before = Tape()->nodes_.End();
            Number_ root = WeightedPayoffRoot(outputs, weights[request]);
            ASSERT_FALSE(Tape()->nodes_.End() == before);
            ASSERT_NEAR(Value(root), expectedValues[request], 1.0e-10);
            recording.FinishRecording();
            Adjoint(root) = 1.0;
            recording.ReverseSuffix(checkpoint);
        }
        recording.ReversePrefix(checkpoint);
        ASSERT_NEAR(AdjointValue(x) / PATHS, expectedX[request], 1.0e-10);
        ASSERT_NEAR(AdjointValue(y) / PATHS, expectedY[request], 1.0e-10);
    }
}

TEST(AADWeightedRootTest, TestDirectInputAndConstantOutputsBeforeCheckpoint) {
    RecordingScope_ recording;
    Number_ x, y;
    recording.RegisterInput(x, 2.0);
    recording.RegisterInput(y, 3.0);
    recording.StartRecording();
    const Vector_<Number_> outputs = {x * y, x, Number_(5.0)};
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ root = WeightedPayoffRoot(outputs, {2.0, -1.0, 0.5});
    ASSERT_NEAR(Value(root), 12.5, 1.0e-10);
    recording.FinishRecording();
    Adjoint(root) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_NEAR(AdjointValue(x), 5.0, 1.0e-10);
    ASSERT_NEAR(AdjointValue(y), 4.0, 1.0e-10);
}

TEST(AADWeightedRootTest, TestAllZeroWeightsHaveZeroValueAndGradient) {
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ root = WeightedPayoffRoot({x * x, x, Number_(5.0)}, {0.0, 0.0, 0.0});
    ASSERT_DOUBLE_EQ(Value(root), 0.0);
    recording.FinishRecording();
    Adjoint(root) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(x), 0.0);
}

TEST(AADWeightedRootTest, TestInvalidWeightsRejectBeforeRecording) {
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    const Vector_<Number_> outputs = {x};
    const auto before = Tape()->nodes_.End();
    ASSERT_THROW(WeightedPayoffRoot({}, {}), Exception_);
    const Vector_<Vector_<double>> invalid = {{}, {1.0, 2.0}, {std::numeric_limits<double>::quiet_NaN()}, {std::numeric_limits<double>::infinity()}};
    for (const auto& weights : invalid) {
        ASSERT_THROW(WeightedPayoffRoot(outputs, weights), Exception_);
        ASSERT_TRUE(Tape()->nodes_.End() == before);
    }
}

TEST(AADWeightedRootTest, TestZeroWeightStillRejectsNonfiniteComponent) {
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    for (const double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        const Vector_<Number_> outputs = {x, Number_(invalid)};
        const auto before = Tape()->nodes_.End();
        ASSERT_THROW(WeightedPayoffRoot(outputs, {1.0, 0.0}), Exception_);
        ASSERT_TRUE(Tape()->nodes_.End() == before);
    }
}

TEST(AADWeightedRootTest, TestOverflowRejectsAndSuffixRestoreRecovers) {
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    const double maximum = std::numeric_limits<double>::max();
    ASSERT_THROW(WeightedPayoffRoot({Number_(maximum), Number_(maximum)}, {1.0, 1.0}), Exception_);
    recording.Restore(checkpoint);
    Number_ root = WeightedPayoffRoot({x}, {1.0});
    ASSERT_DOUBLE_EQ(Value(root), 2.0);
    recording.FinishRecording();
    Adjoint(root) = 1.0;
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(AdjointValue(x), 1.0);
}
