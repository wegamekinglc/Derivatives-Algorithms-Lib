#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/math/aad/accumulate.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>

using namespace Dal::AAD;

TEST(AADAccumulateTest, TestActiveCoefficientsAndVariablesRecordOneNode) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ initial;
    std::array<Number_, 3> coefficients, variables;
    const std::array<double, 3> a{1.0, -2.0, 0.5}, x{3.0, 4.0, -6.0};
    recording.RegisterInput(initial, 2.0);
    for (size_t i = 0; i < a.size(); ++i) {
        recording.RegisterInput(coefficients[i], a[i]);
        recording.RegisterInput(variables[i], x[i]);
    }
    recording.StartRecording();
    const auto before = MeasureTape(*Tape());
    Number_ result = Accumulate<3>(initial, [&](size_t i) { return coefficients[i] * variables[i]; });
    const auto after = MeasureTape(*Tape());
    ASSERT_EQ(after.nodes_ - before.nodes_, 1U);
    ASSERT_EQ(after.edges_ - before.edges_, 7U);
    ASSERT_DOUBLE_EQ(Value(result), -6.0);
    recording.FinishRecording();
    Adjoint(result) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Adjoint(initial), 1.0);
    for (size_t i = 0; i < a.size(); ++i) {
        ASSERT_DOUBLE_EQ(Adjoint(coefficients[i]), x[i]);
        ASSERT_DOUBLE_EQ(Adjoint(variables[i]), a[i]);
    }
}

TEST(AADAccumulateTest, TestDoubleOrderAndEmptyFactory) {
    const std::array<double, 3> terms{-1e16, 1.0, 2.0};
    std::vector<size_t> order;
    const double result = Accumulate<3>(1e16, [&](size_t i) {
        order.push_back(i);
        return terms[i];
    });
    ASSERT_DOUBLE_EQ(result, 3.0);
    ASSERT_EQ(order, std::vector<size_t>({0, 1, 2}));
    size_t calls = 0;
    const double empty = Accumulate<0>(7.0, [&](size_t) {
        ++calls;
        return 99.0;
    });
    ASSERT_DOUBLE_EQ(empty, 7.0);
    ASSERT_EQ(calls, 0U);
}

TEST(AADAccumulateTest, TestZeroActiveCoefficientRetainsItsDerivative) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ coefficient, variable;
    recording.RegisterInput(coefficient, 0.0);
    recording.RegisterInput(variable, 3.0);
    recording.StartRecording();
    const auto before = MeasureTape(*Tape());
    Number_ result = Accumulate<1>(0.0, [&](size_t) { return coefficient * variable; });
    ASSERT_EQ(MeasureTape(*Tape()).nodes_ - before.nodes_, 1U);
    ASSERT_EQ(MeasureTape(*Tape()).edges_ - before.edges_, 2U);
    recording.FinishRecording();
    Adjoint(result) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Value(result), 0.0);
    ASSERT_DOUBLE_EQ(Adjoint(coefficient), 3.0);
    ASSERT_DOUBLE_EQ(Adjoint(variable), 0.0);
}

TEST(AADAccumulateTest, TestPassiveCoefficientsAndEmptyActiveIdentity) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    const auto before = MeasureTape(*Tape());
    const Number_ identity = Accumulate<0>(x, [](size_t) { return Number_(99.0); });
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
    Number_ result = Accumulate<3>(identity, [&](size_t i) { return static_cast<double>(i + 1) * x; });
    ASSERT_EQ(MeasureTape(*Tape()).nodes_ - before.nodes_, 1U);
    ASSERT_DOUBLE_EQ(Value(result), 14.0);
    recording.FinishRecording();
    Adjoint(result) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Adjoint(x), 7.0);
}

TEST(AADAccumulateTest, TestSnapshotsAndNonlinearComposition) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 0.5);
    const Number_ input = x;
    recording.StartRecording();
    const auto expression = Accumulate<3>(0.0, [&](size_t i) { return x * x * static_cast<double>(i + 1); });
    x = x + 1.0;
    const auto before = MeasureTape(*Tape());
    Number_ result = exp(expression);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_ - before.nodes_, 1U);
    ASSERT_EQ(MeasureTape(*Tape()).edges_ - before.edges_, 6U);
    ASSERT_DOUBLE_EQ(Value(result), std::exp(1.5));
    recording.FinishRecording();
    Adjoint(result) = 1.0;
    recording.Reverse();
    ASSERT_NEAR(Adjoint(input), 6.0 * std::exp(1.5), 1e-12);
}

TEST(AADAccumulateTest, TestVectorAdjointsAndRepeatedAliasedSweeps) {
    Clear(*Tape());
    const auto mode = SetNumResultsForAAD(true, 3);
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    Number_ result = Accumulate<3>(x, [&](size_t i) { return x * x * static_cast<double>(i + 1); });
    recording.FinishRecording();
    ASSERT_DOUBLE_EQ(Value(result), 26.0);
    const std::array<double, 3> seeds{1.0, -2.0, 0.5};
    for (const double scale : {1.0, 3.0}) {
        recording.ClearAdjoints();
        for (size_t channel = 0; channel < seeds.size(); ++channel)
            NativeOperations_::SetSeed(result, scale * seeds[channel], channel);
        recording.Reverse();
        for (size_t channel = 0; channel < seeds.size(); ++channel)
            ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, channel), 25.0 * scale * seeds[channel]);
    }
}

TEST(AADAccumulateTest, TestCheckpointReplayWithPassiveVariables) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ initial;
    std::array<Number_, 3> coefficients;
    recording.RegisterInput(initial, 2.0);
    for (auto& coefficient : coefficients)
        recording.RegisterInput(coefficient, 0.5);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    for (const double weight : {1.0, 2.0}) {
        recording.Restore(checkpoint);
        Number_ result = Accumulate<3>(initial, [&](size_t i) { return coefficients[i] * (weight + static_cast<double>(i)); });
        recording.FinishRecording();
        Adjoint(result) = weight;
        recording.ReverseSuffix(checkpoint);
    }
    recording.ReversePrefix(checkpoint);
    ASSERT_DOUBLE_EQ(Adjoint(initial), 3.0);
    for (size_t i = 0; i < coefficients.size(); ++i)
        ASSERT_DOUBLE_EQ(Adjoint(coefficients[i]), 5.0 + 3.0 * static_cast<double>(i));
}

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
TEST(AADAccumulateTest, TestPreviousRecordingOperandRejectedBeforeOutputAllocation) {
    Clear(*Tape());
    const auto expression = [] {
        RecordingScope_ recording;
        Number_ x;
        recording.RegisterInput(x, 2.0);
        recording.StartRecording();
        const auto result = Accumulate<1>(0.0, [&](size_t) { return x * x; });
        recording.FinishRecording();
        return result;
    }();
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 3.0);
    recording.StartRecording();
    const auto before = MeasureTape(*Tape());
    ASSERT_THROW(static_cast<void>(Number_(expression)), Dal::Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
}

TEST(AADAccumulateTest, TestStaleOperandRejectedBeforeOutputAllocation) {
    Clear(*Tape());
    RecordingScope_ recording;
    Number_ x;
    recording.RegisterInput(x, 2.0);
    recording.StartRecording();
    const auto checkpoint = recording.MakeCheckpoint();
    Number_ temporary = x * x;
    const auto expression = Accumulate<3>(0.0, [&](size_t i) { return temporary * static_cast<double>(i + 1); });
    recording.FinishRecording();
    recording.Restore(checkpoint);
    const auto before = MeasureTape(*Tape());
    ASSERT_THROW(static_cast<void>(Number_(expression)), Dal::Exception_);
    ASSERT_EQ(MeasureTape(*Tape()).nodes_, before.nodes_);
}
#endif
