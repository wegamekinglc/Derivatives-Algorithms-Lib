//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/visitor/vectorops.hpp>

using namespace Dal;

TEST(NativeVectorTest, TestSparseWritesBindZeroHolesAndPreserveScalarAndBlockDerivatives) {
    for (const size_t width : {1, 3}) {
        auto mode = AAD::SetNumResultsForAAD(width > 1, width);
        AAD::RecordingScope_ recording;
        AAD::Number_ input;
        recording.RegisterInput(input, 5.0);
        recording.StartRecording();
        Vector_<AAD::Number_> values;
        Script::WriteVectorEntry(&values, 3, input);
        ASSERT_EQ(values.size(), 4);
        ASSERT_DOUBLE_EQ(Value(values[0]), 0.0);
        ASSERT_NO_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(values[0])));
        ASSERT_NO_THROW(static_cast<void>(AAD::NativeOperations_::ReadAdjoint(values[2])));
        const AAD::Number_ doubled = 2.0 * input;
        Script::WriteVectorEntry(&values, 1, doubled);
        auto root = Script::SumVectorValues(values);
        ASSERT_DOUBLE_EQ(Value(root), 15.0);
        recording.FinishRecording();
        for (size_t lane = 0; lane < width; ++lane)
            AAD::NativeOperations_::SetSeed(root, static_cast<double>(lane + 1), lane);
        recording.Reverse();
        for (size_t lane = 0; lane < width; ++lane)
            ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, lane), 3.0 * static_cast<double>(lane + 1));
        recording.Close();
    }
}
