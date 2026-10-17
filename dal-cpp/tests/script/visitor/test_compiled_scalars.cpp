#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/segmentedtrace.hpp>
#include <dal/script/event.hpp>

using Dal::Vector_;
using Dal::AAD::Number_;

namespace {
    struct ExtremumCase_ {
        int opcode_;
        double input_, scalar_, value_, slope_;
        std::uint64_t choice_;
        size_t nodes_;
    };

    Vector_<ExtremumCase_> ExtremumCases() {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        const int max = Dal::Script::Max2Const, min = Dal::Script::Min2Const;
        return {{max, 2.0, 1.0, 2.0, 1.0, 1, 0},   {max, 0.5, 1.0, 1.0, 0.0, 2, 1},   {max, 1.0, 1.0, 1.0, 1.0, 3, 0},
                {max, -0.0, 0.0, -0.0, 1.0, 3, 0}, {max, 0.0, -0.0, 0.0, 1.0, 3, 0},  {max, nan, 1.0, nan, 1.0, 1, 0},
                {max, 1.0, nan, 1.0, 1.0, 1, 0},   {max, inf, 1.0, inf, 1.0, 1, 0},   {max, -inf, 1.0, 1.0, 0.0, 2, 1},
                {max, 1.0, inf, inf, 0.0, 2, 1},   {max, 1.0, -inf, 1.0, 1.0, 1, 0},  {max, inf, inf, inf, 1.0, 3, 0},
                {min, 2.0, 1.0, 1.0, 0.0, 2, 1},   {min, 0.5, 1.0, 0.5, 1.0, 1, 0},   {min, 1.0, 1.0, 1.0, 1.0, 3, 0},
                {min, -0.0, 0.0, -0.0, 1.0, 3, 0}, {min, 0.0, -0.0, 0.0, 1.0, 3, 0},  {min, nan, 1.0, nan, 1.0, 1, 0},
                {min, 1.0, nan, 1.0, 1.0, 1, 0},   {min, inf, 1.0, 1.0, 0.0, 2, 1},   {min, -inf, 1.0, -inf, 1.0, 1, 0},
                {min, 1.0, inf, 1.0, 1.0, 1, 0},   {min, 1.0, -inf, -inf, 0.0, 2, 1}, {min, -inf, -inf, -inf, 1.0, 3, 0}};
    }

    void CheckExtremum(const ExtremumCase_& data, bool traced) {
        const Dal::Script::ScriptCompiled_ program({{Dal::Script::Spot, data.opcode_, 0}}, {{data.scalar_}});
        const Dal::Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
        Vector_<std::uint64_t> trace(plan.MaxWords(), 0);
        const Vector_<Number_> observations;
        using TracePolicy_ = Dal::Script::Detail::SegmentedCompiledPolicy_<Number_>;
        const TracePolicy_ policy(nullptr, observations, plan.View(0, &trace));
        Dal::Script::EvalState_<Number_> state(Vector_<>{});
        Dal::AAD::Sample_<Number_> sample;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, data.input_);
        recording.StartRecording();
        const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
        if (traced) {
            Dal::Script::Detail::EvalCompiledEvents<false>(
                1,
                [&](size_t) {
                    return Dal::Script::Detail::CompiledEventView_<Number_, TracePolicy_>{
                        program.NodeStreams()[0], program.ConstStreams()[0], sample, 0, 0, true, policy};
                },
                &state);
            ASSERT_EQ(trace[0], data.choice_);
        } else {
            Dal::Script::EvalCompiled(program.NodeStreams()[0], program.ConstStreams()[0], sample, state);
        }
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, data.nodes_);
        const double value = Dal::AAD::Value(state.dStack_.Top());
        if (std::isnan(data.value_))
            ASSERT_TRUE(std::isnan(value));
        else
            ASSERT_DOUBLE_EQ(value, data.value_);
        if (data.value_ == 0.0)
            ASSERT_EQ(std::signbit(value), std::signbit(data.value_));
        Dal::AAD::Adjoint(state.dStack_.Top()) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.spot_), data.slope_);
    }

    void CheckConstantPayment(int mode, bool delayed, double amount, size_t payoff = 0) {
        const int opcodes[3][2] = {{Dal::Script::PaysConst, Dal::Script::PaysOnConst},
                                   {Dal::Script::LsmcPaysConst, Dal::Script::LsmcPaysOnConst},
                                   {Dal::Script::LsmcFuzzyPaysConst, Dal::Script::LsmcFuzzyPaysOnConst}};
        Vector_<int> stream{opcodes[mode][delayed], 0};
        if (delayed)
            stream.push_back(0);
        stream.push_back(0);
        Dal::Script::EvalState_<Number_> state(Vector_<>{3.0});
        Dal::AAD::Sample_<Number_> sample;
        sample.discounts_.Resize(1);
        Dal::Script::LsmcRows_ rows(1, 1);
        rows.ZeroPaths(0, 1);
        const Vector_<size_t> events{0};
        Dal::Script::LsmcSinks_ hard;
        hard.pays_ = &rows;
        hard.eventToPays_ = &events;
        hard.payoffIdx_ = payoff;
        state.lsmcSinks_ = &hard;
        Vector_<Number_> payments(1);
        Dal::Script::LsmcFuzzySinks_<Number_> fuzzy;
        fuzzy.pays_ = &payments;
        fuzzy.payoffIdx_ = payoff;
        state.lsmcFuzzySinks_ = &fuzzy;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(state.variables_[0], 3.0);
        const Number_ initial = state.variables_[0];
        recording.RegisterInput(sample.numeraire_, 2.0);
        recording.RegisterInput(sample.discounts_[0], 0.75);
        recording.RegisterInput(payments[0], 0.0);
        recording.StartRecording();
        auto first = Dal::AAD::Tape()->nodes_.End();
        Dal::Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{amount}, sample, state, 0, 0, true);
        size_t leaves = 0;
        for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
            leaves += first->NumArguments() == 0;
        ASSERT_EQ(leaves, 0U) << "mode=" << mode << " delayed=" << delayed << " amount=" << amount;
        const double raw = amount * (delayed ? 0.75 : 1.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.variables_[0]), 3.0 + raw / 2.0);
        const double stored = mode == 1 ? rows[0][0] : Dal::AAD::Value(payments[0]);
        ASSERT_DOUBLE_EQ(stored, mode == 0 || payoff != 0 ? 0.0 : raw);
        Dal::AAD::Adjoint(state.variables_[0]) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(initial), 1.0);
        ASSERT_NEAR(Dal::AAD::Adjoint(sample.numeraire_), -raw / 4.0, 1e-12);
        ASSERT_NEAR(Dal::AAD::Adjoint(sample.discounts_[0]), delayed ? amount / 2.0 : 0.0, 1e-12);
    }

    void CheckStoredPayment(bool delayed, double amount) {
        const Vector_<int> stream =
            delayed ? Vector_<int>{Dal::Script::LsmcFuzzyPaysOnConst, 0, 0, 0} : Vector_<int>{Dal::Script::LsmcFuzzyPaysConst, 0, 0};
        Dal::Script::EvalState_<Number_> state(Vector_<>{0.0});
        Dal::AAD::Sample_<Number_> sample;
        sample.discounts_.Resize(1);
        Vector_<Number_> payments(1);
        Dal::Script::LsmcFuzzySinks_<Number_> sinks;
        sinks.pays_ = &payments;
        sinks.payoffIdx_ = 0;
        state.lsmcFuzzySinks_ = &sinks;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(state.variables_[0], 0.0);
        recording.RegisterInput(sample.numeraire_, 2.0);
        recording.RegisterInput(sample.discounts_[0], 0.75);
        recording.RegisterInput(payments[0], 0.0);
        recording.StartRecording();
        Dal::Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{amount}, sample, state, 0, 0, true);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(payments[0]), amount * (delayed ? 0.75 : 1.0));
        Dal::AAD::Adjoint(payments[0]) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.numeraire_), 0.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.discounts_[0]), delayed ? amount : 0.0);
    }
} // namespace

TEST(CompiledScalarsTest, TestUnselectedExtremumDoesNotRecordConstantLeaf) {
    Dal::Script::EvalState_<Number_> state(Vector_<>{});
    Dal::AAD::Sample_<Number_> sample;
    Dal::AAD::RecordingScope_ recording;
    recording.RegisterInput(sample.spot_, 2.0);
    recording.StartRecording();
    state.dStack_.Push(sample.spot_);
    const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
    Dal::Script::EvalCompiled(Vector_<int>{Dal::Script::Max2Const, 0}, Vector_<>{1.0}, sample, state, 0, 0, false);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, 0U);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.dStack_.Top()), 2.0);
    recording.FinishRecording();
}

TEST(CompiledScalarsTest, TestExtremumTraceDoesNotMaterializeUnselectedScalar) {
    const Dal::Script::ScriptCompiled_ program({{Dal::Script::Spot, Dal::Script::Max2Const, 0}}, {{1.0}});
    const Dal::Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
    ASSERT_EQ(plan.MaxWords(), 1U);
    Vector_<std::uint64_t> trace(1, 0);
    const Vector_<Number_> observations;
    using TracePolicy_ = Dal::Script::Detail::SegmentedCompiledPolicy_<Number_>;
    const TracePolicy_ policy(nullptr, observations, plan.View(0, &trace));
    Dal::Script::EvalState_<Number_> state(Vector_<>{});
    Dal::AAD::Sample_<Number_> sample;
    Dal::AAD::RecordingScope_ recording;
    recording.RegisterInput(sample.spot_, 2.0);
    recording.StartRecording();
    const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
    Dal::Script::Detail::EvalCompiledEvents<false>(
        1,
        [&](size_t) {
            return Dal::Script::Detail::CompiledEventView_<Number_, TracePolicy_>{
                program.NodeStreams()[0], program.ConstStreams()[0], sample, 0, 0, true, policy};
        },
        &state);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, 0U);
    ASSERT_EQ(trace[0], 1U);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.dStack_.Top()), 2.0);
    recording.FinishRecording();
}

TEST(CompiledScalarsTest, TestConstantPaymentsKeepRisksWithoutOperandLeaves) {
    for (int mode = 0; mode < 3; ++mode)
        for (const bool delayed : {false, true})
            for (const double amount : {-8.0, 0.0, 8.0})
                ASSERT_NO_FATAL_FAILURE(CheckConstantPayment(mode, delayed, amount));
}

TEST(CompiledScalarsTest, TestExtremaKeepChoicesGradientsAndNonfiniteBehavior) {
    for (const auto& data : ExtremumCases())
        for (const bool traced : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckExtremum(data, traced));
}

TEST(CompiledScalarsTest, TestLsmcFilteringKeepsStatePaymentWithoutRecordingOtherPayoff) {
    for (const int mode : {1, 2})
        for (const bool delayed : {false, true})
            for (const double amount : {-8.0, 0.0, 8.0})
                ASSERT_NO_FATAL_FAILURE(CheckConstantPayment(mode, delayed, amount, 1));
}

TEST(CompiledScalarsTest, TestFuzzyStoredPaymentsKeepRawDiscountRisk) {
    for (const bool delayed : {false, true})
        for (const double amount : {-8.0, 0.0, 8.0})
            ASSERT_NO_FATAL_FAILURE(CheckStoredPayment(delayed, amount));
}

TEST(CompiledScalarsTest, TestMissingLsmcSinksKeepExecutionModeError) {
    for (const int op :
         {Dal::Script::LsmcPaysConst, Dal::Script::LsmcPaysOnConst, Dal::Script::LsmcFuzzyPaysConst, Dal::Script::LsmcFuzzyPaysOnConst}) {
        Dal::Script::EvalState_<double> state(Vector_<>{0.0});
        Dal::AAD::Sample_<double> sample{};
        sample.numeraire_ = 2.0;
        sample.discounts_ = {0.75};
        Vector_<int> stream{op, 0};
        if (op == Dal::Script::LsmcPaysOnConst || op == Dal::Script::LsmcFuzzyPaysOnConst)
            stream.push_back(0);
        stream.push_back(0);
        try {
            Dal::Script::Detail::EvalCompiledRange<true, true>(stream, Vector_<>{8.0}, sample, state, 0, 0, true);
            FAIL() << "Missing LSMC sinks must reject execution";
        } catch (const Dal::ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find("UnsupportedExecutionMode"), std::string::npos);
        }
    }
}

TEST(CompiledScalarsTest, TestSelectedConstantsAndPaymentsSurviveCheckpointReuse) {
    const Vector_<int> stream{Dal::Script::Spot, Dal::Script::Max2Const, 0, Dal::Script::Pays, 0, Dal::Script::PaysConst, 1, 0};
    const Vector_<> constants{3.0, 2.0};
    for (int generation = 0; generation < 2; ++generation) {
        Dal::Script::EvalState_<Number_> state(Vector_<>{3.0});
        Dal::AAD::Sample_<Number_> sample;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, 1.0);
        recording.RegisterInput(sample.numeraire_, 2.0);
        recording.RegisterInput(state.variables_[0], 3.0);
        const Number_ initialSpot = sample.spot_, initialPayoff = state.variables_[0];
        recording.StartRecording();
        const auto checkpoint = recording.MakeCheckpoint();
        recording.FinishRecording();
        for (const double offset : {0.0, 4.0}) {
            recording.Restore(checkpoint);
            sample.spot_ = initialSpot + offset;
            state.variables_[0] = initialPayoff;
            auto first = Dal::AAD::Tape()->nodes_.End();
            Dal::Script::EvalCompiled(stream, constants, sample, state);
            size_t leaves = 0;
            for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                leaves += first->NumArguments() == 0;
            ASSERT_EQ(leaves, offset == 0.0 ? 1U : 0U);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.variables_[0]), 4.0 + std::max(1.0 + offset, 3.0) / 2.0);
            Dal::AAD::Adjoint(state.variables_[0]) = 1.0;
            recording.FinishRecording();
            recording.ReverseSuffix(checkpoint);
        }
        recording.ReversePrefix(checkpoint);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(initialSpot), 0.5);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(initialPayoff), 2.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(sample.numeraire_), -3.0);
    }
}
