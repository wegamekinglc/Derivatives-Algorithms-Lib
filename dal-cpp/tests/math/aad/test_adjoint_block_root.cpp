//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <dal/math/aad/adjointblockroot.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <limits>

using namespace Dal;
using namespace Dal::AAD;

namespace {
    void AssertBlockValues(const Vector_<Number_>& roots, const AdjointBlock_& block, const Vector_<double>& expected) {
        for (size_t lane = 0; lane < block.outputs_; ++lane)
            ASSERT_DOUBLE_EQ(Value(roots[lane]), expected[block.firstOutput_ + lane]);
        for (size_t lane = block.outputs_; lane < block.width_; ++lane)
            ASSERT_DOUBLE_EQ(Value(roots[lane]), 0.0);
    }
} // namespace

TEST(AADAdjointBlockRootTest, TestAliasesConstantsDirectAndPrefixOutputsAcrossPaths) {
    Vector_<double> expectedX = {3.0, 4.0, 3.0, 0.0, 1.0, 3.0};
    Vector_<double> expectedY = {2.0, 1.0, 2.0, 0.0, 0.0, 2.0};
    Vector_<double> expectedValues = {6.0, 7.0, 6.0, 5.0, 2.0, 6.0};
    for (size_t row = expectedX.size(); row < 17; ++row) {
        expectedX.push_back(3.0);
        expectedY.push_back(2.0);
        expectedValues.push_back(6.0);
    }
    for (const size_t width : {1, 2, 4, 16}) {
        AdjointBlockSettings_ settings;
        settings.maxWidth_ = width;
        const auto plan = PlanAdjointBlocks(17, 2, settings);
        for (size_t blockIndex = 0; blockIndex < plan.BlockCount(); ++blockIndex) {
            const auto block = plan.Block(blockIndex);
            const auto mode = SetNumResultsForAAD(true, block.width_);
            RecordingScope_ recording;
            Number_ x, y, zero;
            recording.RegisterInput(x, 2.0);
            recording.RegisterInput(y, 3.0);
            recording.RegisterInput(zero, 0.0);
            recording.StartRecording();
            const Number_ prefix = x * y;
            const auto checkpoint = recording.MakeCheckpoint();
            Vector_<Number_> roots;
            roots.reserve(block.width_);
            constexpr size_t PATHS = 3;
            for (size_t path = 0; path < PATHS; ++path) {
                recording.Restore(checkpoint);
                const Number_ u = x * y;
                const Number_ v = x * x + y;
                Vector_<Number_> outputs = {u, v, u, Number_(5.0), x, prefix};
                for (size_t row = outputs.size(); row < 17; ++row)
                    outputs.push_back(u);
                SeedAdjointBlock(outputs, block, zero, &roots);
                ASSERT_NO_FATAL_FAILURE(AssertBlockValues(roots, block, expectedValues));
                recording.FinishRecording();
                recording.ReverseSuffix(checkpoint);
            }
            recording.ReversePrefix(checkpoint);
            for (size_t lane = 0; lane < block.outputs_; ++lane) {
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(x, lane) / PATHS, expectedX[block.firstOutput_ + lane], 1.0e-10);
                ASSERT_NEAR(NativeOperations_::ReadAdjoint(y, lane) / PATHS, expectedY[block.firstOutput_ + lane], 1.0e-10);
            }
            for (size_t lane = block.outputs_; lane < block.width_; ++lane) {
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 0.0);
                ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(y, lane), 0.0);
            }
        }
    }
}

TEST(AADAdjointBlockRootTest, TestPadsUnusedLanesInReusedTerminalRootStorage) {
    const auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ recording;
    Number_ x, zero;
    recording.RegisterInput(x, 2.0);
    recording.RegisterInput(zero, 0.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ output = x * x;
    Vector_<Number_> roots;
    roots.reserve(4);
    NativeOperations_::SetSeed(output, 17.0, 3);
    SeedAdjointBlock({output}, {0, 1, 4}, zero, &roots);
    recording.FinishRecording();
    recording.ReverseSuffix(checkpoint);
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 0), 4.0);
    for (size_t lane = 1; lane < 4; ++lane)
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, lane), 0.0);
}

TEST(AADAdjointBlockRootTest, TestClearsStaleLiveLanesBeforeSeedingAliases) {
    for (const size_t width : {2, 3}) {
        const auto mode = SetNumResultsForAAD(true, width);
        RecordingScope_ recording;
        Number_ x, zero;
        recording.RegisterInput(x, 2.0);
        recording.RegisterInput(zero, 0.0);
        recording.StartRecording();
        const auto checkpoint = recording.MakeCheckpoint();
        Number_ square = x * x;
        Vector_<Number_> outputs = {square, x};
        if (width == 3)
            outputs.push_back(square);
        NativeOperations_::SetSeed(square, 17.0, 1);
        Vector_<Number_> roots;
        roots.reserve(width);
        SeedAdjointBlock(outputs, {0, width, width}, zero, &roots);
        recording.FinishRecording();
        recording.ReverseSuffix(checkpoint);
        recording.ReversePrefix(checkpoint);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 0), 4.0);
        ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 1), 1.0);
        if (width == 3)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 2), 4.0);
    }
}

TEST(AADAdjointBlockRootTest, TestRejectsInvalidRequestsBeforeChangingGraph) {
    const auto mode = SetNumResultsForAAD(true, 4);
    RecordingScope_ recording;
    Number_ x, zero;
    recording.RegisterInput(x, 2.0);
    recording.RegisterInput(zero, 0.0);
    recording.StartRecording();
    const Vector_<Number_> outputs = {x * x, x};
    Vector_<Number_> roots;
    roots.reserve(4);
    const auto before = Tape()->nodes_.End();
    const auto storage = roots.data();
    ASSERT_THROW(SeedAdjointBlock(outputs, {0, 1, 4}, zero, nullptr), Exception_);
    ASSERT_THROW(SeedAdjointBlock(roots, {0, 1, 4}, zero, &roots), Exception_);
    for (const auto& block :
         {AdjointBlock_{0, 0, 4}, AdjointBlock_{0, 5, 4}, AdjointBlock_{0, 1, 0}, AdjointBlock_{0, 1, ADJ_SIZE + 1}, AdjointBlock_{3, 1, 4},
          AdjointBlock_{2, 1, 4}, AdjointBlock_{0, std::numeric_limits<size_t>::max(), 4}, AdjointBlock_{0, 1, 2}})
        ASSERT_THROW(SeedAdjointBlock(outputs, block, zero, &roots), Exception_);
    Vector_<Number_> insufficient;
    ASSERT_THROW(SeedAdjointBlock(outputs, {0, 1, 4}, zero, &insufficient), Exception_);
    ASSERT_THROW(SeedAdjointBlock(outputs, {0, 1, 4}, Number_(), &roots), Exception_);
    ASSERT_THROW(SeedAdjointBlock(outputs, {0, 1, 4}, x, &roots), Exception_);
    ASSERT_TRUE(Tape()->nodes_.End() == before);
    ASSERT_TRUE(roots.empty());
    ASSERT_EQ(roots.data(), storage);
    for (const double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        const Vector_<Number_> invalidOutputs = {Number_(invalid)};
        const auto invalidBefore = Tape()->nodes_.End();
        ASSERT_THROW(SeedAdjointBlock(invalidOutputs, {0, 1, 4}, zero, &roots), Exception_);
        ASSERT_TRUE(Tape()->nodes_.End() == invalidBefore);
        ASSERT_TRUE(roots.empty());
        ASSERT_EQ(roots.data(), storage);
    }
}
