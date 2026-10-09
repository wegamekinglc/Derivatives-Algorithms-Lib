//
// Created by Codex on 2026/10/9.
//

#include <gtest/gtest.h>

#include <limits>
#include <optional>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/segmentedstate.hpp>
#include <dal/script/detail/segmentedtrace.hpp>
#include <dal/script/preparation.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    Script::PreparedScript_ VectorStateFixture() {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        Script::MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        AAD::BlackScholes_<> model(100.0, 0.2);
        const Script::ScriptProductData_ product("", {Cell_("S"), Cell_(Date_(2026, 10, 2))}, {"2", "v[1] = S pay PAYS SUM(v)"});
        return Script::PrepareScript(product, &model, valuation, simulation);
    }
} // namespace

TEST(SegmentedScriptStateTest, TestInclusiveObservationLivenessAndSlotReuse) {
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    AAD::BlackScholes_<> model(100.0, 0.2, 0.03, 0.01);
    const Script::ScriptProductData_ product(
        "", {Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4)), Cell_(Date_(2026, 10, 6)), Cell_(Date_(2026, 10, 8))},
        {"first = FIX(EQ[DAL196_TEST])", "x = FIX(EQ[DAL196_TEST], 2026-10-02) + FIX(EQ[DAL196_TEST])", "y = FIX(EQ[DAL196_TEST])",
         "pay PAYS FIX(EQ[DAL196_TEST], 2026-10-06) + y"});
    const auto prepared = Script::PrepareScript(product, &model, valuation, simulation);
    const Script::Detail::SegmentedObservationPlan_ compact(prepared.PlanHandle());
    ASSERT_EQ(compact.Slots(), 2);
    Vector_<> storage(compact.Slots(), 0.0);
    Vector_<size_t> ids(3);
    for (size_t i = 0; i < prepared.Plan().Requests().size(); ++i) {
        const auto& request = prepared.Plan().Request(i);
        ASSERT_TRUE(request.modelSlot_);
        if (request.key_.fixingTime_.Date() == Date_(2026, 10, 2))
            ids[0] = i;
        else if (request.key_.fixingTime_.Date() == Date_(2026, 10, 4))
            ids[1] = i;
        else if (request.key_.fixingTime_.Date() == Date_(2026, 10, 6))
            ids[2] = i;
    }
    AAD::Sample_<> sample;
    sample.observations_ = {101.0};
    compact.Capture(0, sample, &storage);
    ASSERT_DOUBLE_EQ(compact.Read(ids[0], storage), 101.0);
    sample.observations_ = {104.0};
    compact.Capture(1, sample, &storage);
    ASSERT_DOUBLE_EQ(compact.Read(ids[0], storage), 101.0);
    ASSERT_DOUBLE_EQ(compact.Read(ids[1], storage), 104.0);
    ASSERT_NE(compact.Slot(ids[0]), compact.Slot(ids[1]));
    sample.observations_ = {106.0};
    compact.Capture(2, sample, &storage);
    ASSERT_DOUBLE_EQ(compact.Read(ids[2], storage), 106.0);
    ASSERT_EQ(compact.Slot(ids[0]), compact.Slot(ids[2]));
    sample.observations_.clear();
    compact.Capture(3, sample, &storage);
    ASSERT_DOUBLE_EQ(compact.Read(ids[2], storage), 106.0);
}

TEST(SegmentedScriptStateTest, TestKnownValuesNativeInputsAndPlanOwnership) {
    std::optional<Script::Detail::SegmentedObservationPlan_> compact;
    size_t known = 0;
    size_t future = 0;
    {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        Script::MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        AAD::BlackScholes_<> model(100.0, 0.2, 0.03, 0.01);
        const Dal::Handle_<Dal::MarketFixingSnapshot_> history(
            new Dal::MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{Dal::DateTime_(Date_(2026, 9, 30), 0.0), 80.0}}}}));
        const Script::ScriptProductData_ product(
            "", {Cell_("SCALE"), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
            {"2", "x = SCALE * FIX(EQ[DAL196_TEST], 2026-09-30)", "pay PAYS FIX(EQ[DAL196_TEST], 2026-10-02) + x"});
        const auto prepared = Script::PrepareScript(product, &model, valuation, simulation, history);
        compact.emplace(prepared.PlanHandle());
        for (size_t i = 0; i < prepared.Plan().Requests().size(); ++i) {
            if (prepared.Plan().Request(i).historyValueId_)
                known = i;
            else if (prepared.Plan().Request(i).modelSlot_)
                future = i;
        }
    }
    ASSERT_EQ(compact->Slots(), 1);
    ASSERT_FALSE(compact->Slot(known));
    ASSERT_DOUBLE_EQ(compact->Read(known, Vector_<>{0.0}), 80.0);
    const auto mode = AAD::SetNumResultsForAAD(false, 1);
    AAD::RecordingScope_ recording;
    AAD::Number_ observation;
    AAD::Number_ scale;
    recording.RegisterInput(observation, 101.0);
    recording.RegisterInput(scale, 2.0);
    recording.StartRecording();
    AAD::Sample_<AAD::Number_> sample;
    sample.observations_ = {observation};
    Vector_<AAD::Number_> values{AAD::Number_(0.0)};
    compact->Capture(0, sample, &values);
    AAD::Number_ root = compact->Read(future, values) + scale * compact->Read(known, values);
    recording.FinishRecording();
    AAD::NativeOperations_::AddSeed(root, 1.0);
    recording.Reverse();
    ASSERT_NEAR(AAD::Value(root), 261.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(observation), 1.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(scale), 80.0, 1e-10);
    recording.Close();
}

TEST(SegmentedScriptStateTest, TestObservationShapeErrorsPreserveStorageAndRecover) {
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    AAD::BlackScholes_<> model(100.0, 0.2);
    const Script::ScriptProductData_ product("", {Cell_(Date_(2026, 10, 2))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    const auto prepared = Script::PrepareScript(product, &model, valuation, simulation);
    const Script::Detail::SegmentedObservationPlan_ compact(prepared.PlanHandle());
    Vector_<> values(compact.Slots(), 42.0);
    const AAD::Sample_<> empty{};
    ASSERT_THROW(compact.Capture(0, empty, &values), Dal::Exception_);
    ASSERT_DOUBLE_EQ(values[0], 42.0);
    ASSERT_THROW(compact.Capture(1, empty, &values), Dal::Exception_);
    ASSERT_THROW(compact.Capture(0, empty, static_cast<Vector_<>*>(nullptr)), Dal::Exception_);
    ASSERT_THROW((void)compact.Read(0, Vector_<>{}), Dal::Exception_);
    ASSERT_THROW((void)compact.Read(prepared.Plan().Requests().size(), values), Dal::Exception_);
    ASSERT_THROW((void)compact.Slot(prepared.Plan().Requests().size()), Dal::Exception_);
    AAD::Sample_<> healthy;
    healthy.observations_ = {105.0};
    compact.Capture(0, healthy, &values);
    ASSERT_DOUBLE_EQ(compact.Read(0, values), 105.0);
}

TEST(SegmentedScriptStateTest, TestCompleteScalarVectorAndObservationBoundary) {
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    AAD::BlackScholes_<> model(100.0, 0.2);
    const Script::ScriptProductData_ product("", {Cell_("S"), Cell_(Date_(2026, 9, 30)), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
                                             {"2", "APPEND(v, S) v[2] = S * 3 x = S", "APPEND(v, spot()) x = x + 1",
                                              "IF spot() > 100 THEN APPEND(v, x) ELSE v[5] = 9 END pay PAYS SUM(v) + x"});
    const auto prepared = Script::PrepareScript(product, &model, valuation, simulation);
    const Script::Detail::SegmentedEvalLayout_ layout(prepared.Product(), 1);
    ASSERT_EQ(layout.VectorBounds().size(), 1);
    ASSERT_EQ(layout.VectorBounds()[0], 9);
    auto original = prepared.BuildEvalState<double>();
    original.Init();
    for (size_t i = 0; i < original.variables_.size(); ++i)
        original.variables_[i] = 10.0 + static_cast<double>(i);
    original.vectors_[0] = {3.0, 0.0, 6.0, 7.0};
    const Vector_<> observations{101.0};
    const auto packed = layout.Pack(std::log(100.0), original, observations);
    ASSERT_EQ(packed.size(), layout.Size());
    ASSERT_EQ(layout.Size(), 1 + original.variables_.size() + 1 + 9 + observations.size());
    auto restored = prepared.BuildEvalState<double>();
    restored.variables_ = Vector_<>(original.variables_.size(), -1.0);
    restored.vectors_[0] = {999.0};
    double logSpot = 0.0;
    Vector_<> captured(1, 0.0);
    layout.Restore(packed, &logSpot, &restored, &captured);
    ASSERT_DOUBLE_EQ(logSpot, std::log(100.0));
    ASSERT_EQ(restored.variables_, original.variables_);
    ASSERT_EQ(restored.vectors_, original.vectors_);
    ASSERT_EQ(captured, observations);
    restored.variables_[0] = 55.0;
    restored.vectors_[0][0] = 99.0;
    ASSERT_DOUBLE_EQ(original.variables_[0], 10.0);
    ASSERT_DOUBLE_EQ(original.vectors_[0][0], 3.0);
}

TEST(SegmentedScriptStateTest, TestBoundaryPreservesIndependentNativeDerivative) {
    const auto prepared = VectorStateFixture();
    const Script::Detail::SegmentedEvalLayout_ layout(prepared.Product(), 1);
    const auto mode = AAD::SetNumResultsForAAD(false, 1);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 3.0);
    recording.StartRecording();
    auto original = prepared.BuildEvalState<AAD::Number_>();
    original.variables_[0] = input;
    original.vectors_[0] = {AAD::Number_(2.0 * input), AAD::Number_(input + 1.0)};
    Vector_<AAD::Number_> observations{AAD::Number_(input * input)};
    const AAD::Number_ initialLog = Dal::log(input);
    const auto packed = layout.Pack(initialLog, original, observations);
    auto restored = prepared.BuildEvalState<AAD::Number_>();
    AAD::Number_ logSpot;
    Vector_<AAD::Number_> captured(1);
    layout.Restore(packed, &logSpot, &restored, &captured);
    AAD::Number_ root = restored.variables_[0] * restored.vectors_[0][1] + captured[0] + Dal::exp(logSpot);
    recording.FinishRecording();
    AAD::NativeOperations_::AddSeed(root, 1.0);
    recording.Reverse();
    ASSERT_NEAR(AAD::Value(root), 24.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(input), 14.0, 1e-10);
    recording.Close();
}

TEST(SegmentedScriptStateTest, TestMalformedVectorLengthsRejectBeforeStateMutation) {
    const auto prepared = VectorStateFixture();
    const Script::Detail::SegmentedEvalLayout_ layout(prepared.Product(), 0);
    ASSERT_EQ(layout.VectorBounds()[0], 2);
    auto original = prepared.BuildEvalState<double>();
    original.variables_[0] = 10.0;
    original.vectors_[0] = {1.0, 2.0};
    const auto packed = layout.Pack(3.0, original, Vector_<>{});
    const size_t lengthOffset = 1 + original.variables_.size();
    auto restored = prepared.BuildEvalState<double>();
    restored.variables_[0] = 17.0;
    restored.vectors_[0] = {19.0};
    double logSpot = 23.0;
    Vector_<> observations;
    for (double length : {-1.0, 0.5, 3.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid = packed;
        invalid[lengthOffset] = length;
        ASSERT_THROW(layout.Restore(invalid, &logSpot, &restored, &observations), Dal::Exception_);
        ASSERT_DOUBLE_EQ(logSpot, 23.0);
        ASSERT_DOUBLE_EQ(restored.variables_[0], 17.0);
        ASSERT_EQ(restored.vectors_[0], Vector_<>{19.0});
    }
    ASSERT_THROW(layout.Restore(Vector_<>{}, &logSpot, &restored, &observations), Dal::Exception_);
    original.vectors_[0].push_back(3.0);
    ASSERT_THROW((void)layout.Pack(3.0, original, Vector_<>{}), Dal::Exception_);
    layout.Restore(packed, &logSpot, &restored, &observations);
    ASSERT_DOUBLE_EQ(logSpot, 3.0);
    ASSERT_DOUBLE_EQ(restored.variables_[0], 10.0);
    ASSERT_EQ(restored.vectors_[0], (Vector_<>{1.0, 2.0}));
}

namespace {
    struct CompactTestPolicy_ : Script::Detail::DefaultCompiledPolicy_ {
        const Script::Detail::SegmentedObservationPlan_* plan_;
        const Vector_<>* values_;

        CompactTestPolicy_(const Script::Detail::SegmentedObservationPlan_* plan, const Vector_<>* values) : plan_(plan), values_(values) {}
        double Read(size_t requestId, const Script::EvalState_<double>&) const { return plan_->Read(requestId, *values_); }
    };
} // namespace

TEST(SegmentedScriptStateTest, TestCompactReadPolicySurvivesNestedCompiledBranches) {
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    AAD::BlackScholes_<> model(100.0, 0.2);
    const Script::ScriptProductData_ product(
        "", {Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
        {"first = FIX(EQ[DAL196_TEST])",
         "IF FIX(EQ[DAL196_TEST]) > 0 THEN IF FIX(EQ[DAL196_TEST]) > 0 THEN pay PAYS FIX(EQ[DAL196_TEST], 2026-10-02) + FIX(EQ[DAL196_TEST]) "
         "ELSE pay PAYS -1 END ELSE pay PAYS -2 END"});
    const auto prepared = Script::PrepareScript(product, &model, valuation, simulation);
    const Script::Detail::SegmentedObservationPlan_ compact(prepared.PlanHandle());
    Vector_<> values(compact.Slots(), 0.0);
    AAD::Sample_<> sample{};
    sample.spot_ = 101.0;
    sample.numeraire_ = 1.0;
    sample.observations_ = {101.0};
    compact.Capture(0, sample, &values);
    sample.spot_ = 104.0;
    sample.observations_ = {104.0};
    compact.Capture(1, sample, &values);
    auto state = prepared.BuildEvalState<double>();
    const auto& program = prepared.CompiledProgram(true);
    ASSERT_EQ(state.scenario_, nullptr);
    const CompactTestPolicy_ policy(&compact, &values);
    Script::Detail::EvalCompiledEvents<true>(
        1,
        [&](size_t) {
            return Script::Detail::CompiledEventView_<double, CompactTestPolicy_>{
                program.NodeStreams()[1], program.ConstStreams()[1], sample, 0, 0, true, policy};
        },
        &state);
    ASSERT_NEAR(state.VarVals()[prepared.PayOffIdx()], 205.0, 1e-10);
}

namespace {
    struct TestTrace_ {
        Vector_<size_t> extrema_;
        Vector_<> fuzzyInputs_;
        Vector_<> degrees_;
    };

    struct TraceTestPolicy_ : Script::Detail::DefaultCompiledPolicy_ {
        static constexpr bool trace_ = true;
        TestTrace_* result_;

        explicit TraceTestPolicy_(TestTrace_* result) : result_(result) {}
        template <class T_, class C_> void Extremum(size_t, const T_& left, const T_& right, C_ compare) const {
            const double a = AAD::Value(left);
            const double b = AAD::Value(right);
            result_->extrema_.push_back(a == b ? 3 : (compare(b, a) ? 2 : 1));
        }
        template <class T_> void FuzzyComparison(size_t, const T_& value, double lower, double upper, bool) const {
            result_->fuzzyInputs_.push_back(AAD::Value(value));
            result_->fuzzyInputs_.push_back(lower);
            result_->fuzzyInputs_.push_back(upper);
        }
        template <class T_> void FuzzyBranch(size_t, const T_& degree) const { result_->degrees_.push_back(AAD::Value(degree)); }
        void Branch(size_t, bool) const {}
        template <class T_> void VectorReduction(size_t, Script::NodeVectorReduce_::Kind_, const Vector_<T_>&) const {}
    };
} // namespace

TEST(SegmentedScriptStateTest, TestTraceObservesExtremumTiesAndFuzzyBoundaryInputs) {
    Script::ScriptProduct_ product({Cell_(Date_(2027, 1, 1))},
                                   {"x = MAX(spot(), 100) y = MIN(spot(), 100) IF spot() > 100:2 THEN pay PAYS x ELSE pay PAYS y END"});
    const auto nested = product.PreProcess(true, false);
    const auto program = product.Compile(true);
    const Vector_<Vector_<size_t>> expected{{2, 1}, {3, 3}, {1, 2}};
    for (size_t i = 0; i < expected.size(); ++i) {
        AAD::Sample_<> sample{};
        sample.spot_ = 99.0 + static_cast<double>(i);
        sample.numeraire_ = 1.0;
        auto state = product.BuildEvalState<double>(nested, 2.0);
        TestTrace_ trace;
        const TraceTestPolicy_ policy(&trace);
        Script::Detail::EvalCompiledEvents<false>(
            1,
            [&](size_t) {
                return Script::Detail::CompiledEventView_<double, TraceTestPolicy_>{
                    program.NodeStreams()[0], program.ConstStreams()[0], sample, 0, 0, true, policy};
            },
            &state);
        ASSERT_EQ(trace.extrema_, expected[i]);
        ASSERT_EQ(trace.fuzzyInputs_, (Vector_<>{sample.spot_ - 100.0, -1.0, 1.0}));
        ASSERT_EQ(trace.degrees_, (Vector_<>{0.5 * static_cast<double>(i)}));
    }
}

TEST(SegmentedScriptStateTest, TestBoundedTraceStorageHasExactChoices) {
    Script::ScriptProduct_ product({Cell_(Date_(2027, 1, 1))},
                                   {"x = MAX(spot(), 100) y = MIN(spot(), 100) IF spot() > 100:2 THEN pay PAYS x ELSE pay PAYS y END"});
    const auto nested = product.PreProcess(true, false);
    const auto program = product.Compile(true);
    const Script::Detail::SegmentedTracePlan_ tracePlan(program, {}, {0}, 1);
    ASSERT_EQ(tracePlan.MaxWords(), 4);
    const Vector_<Vector_<std::uint64_t>> expected{{2, 1, 2, 1}, {3, 3, 3, 3}, {1, 2, 4, 2}};
    for (size_t i = 0; i < expected.size(); ++i) {
        AAD::Sample_<> sample{};
        sample.spot_ = 99.0 + static_cast<double>(i);
        sample.numeraire_ = 1.0;
        auto state = product.BuildEvalState<double>(nested, 2.0);
        Vector_<std::uint64_t> trace(tracePlan.MaxWords(), 0);
        const Vector_<> observations;
        const Script::Detail::SegmentedCompiledPolicy_<double> policy(nullptr, observations, tracePlan.View(0, &trace));
        Script::Detail::EvalCompiledEvents<false>(
            1,
            [&](size_t) {
                return Script::Detail::CompiledEventView_<double, Script::Detail::SegmentedCompiledPolicy_<double>>{
                    program.NodeStreams()[0], program.ConstStreams()[0], sample, 0, 0, true, policy};
            },
            &state);
        ASSERT_EQ(trace, expected[i]);
    }
}

namespace {
    Vector_<std::uint64_t> EvaluateTrace(Script::ScriptProduct_* product,
                                         const Script::ScriptCompiled_& program,
                                         const Script::Detail::SegmentedTracePlan_& plan,
                                         size_t nested,
                                         double spot) {
        AAD::Sample_<> sample{};
        sample.spot_ = spot;
        sample.numeraire_ = 1.0;
        auto state = product->BuildEvalState<double>(nested, 2.0);
        Vector_<std::uint64_t> trace(plan.MaxWords(), 0);
        const Vector_<> observations;
        const Script::Detail::SegmentedCompiledPolicy_<double> policy(nullptr, observations, plan.View(0, &trace));
        Script::Detail::EvalCompiledEvents<false>(
            1,
            [&](size_t) {
                return Script::Detail::CompiledEventView_<double, Script::Detail::SegmentedCompiledPolicy_<double>>{
                    program.NodeStreams()[0], program.ConstStreams()[0], sample, 0, 0, true, policy};
            },
            &state);
        return trace;
    }
} // namespace

TEST(SegmentedScriptStateTest, TestSkippedBranchesKeepSentinelsAndInteriorVisitsBoth) {
    Script::ScriptProduct_ product({Cell_(Date_(2027, 1, 1))},
                                   {"IF spot() > 100:2 THEN pay PAYS MAX(spot(), 100) ELSE pay PAYS MIN(spot(), 100) END"});
    const auto nested = product.PreProcess(true, false);
    const auto program = product.Compile(true);
    const Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
    ASSERT_EQ(plan.MaxWords(), 4);
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 98.0), (Vector_<std::uint64_t>{1, 1, 0, 1}));
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 100.0), (Vector_<std::uint64_t>{3, 3, 3, 3}));
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 102.0), (Vector_<std::uint64_t>{5, 2, 1, 0}));
}

TEST(SegmentedScriptStateTest, TestEqualWeightsOnOppositeEqualitySlopesHaveDifferentTraces) {
    Script::ScriptProduct_ product({Cell_(Date_(2027, 1, 1))}, {"IF spot() = 100:2 THEN pay PAYS 1 ELSE pay PAYS 0 END"});
    const auto nested = product.PreProcess(true, false);
    const auto program = product.Compile(true);
    const Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
    ASSERT_EQ(plan.MaxWords(), 2);
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 99.5), (Vector_<std::uint64_t>{3, 3}));
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 100.5), (Vector_<std::uint64_t>{5, 3}));
    ASSERT_EQ(EvaluateTrace(&product, program, plan, nested, 100.0), (Vector_<std::uint64_t>{4, 2}));
}

TEST(SegmentedScriptStateTest, TestVectorTraceKeepsLengthsIndividualChoicesAndSampleOffsets) {
    Script::ScriptProduct_ product({Cell_(Date_(2027, 1, 1)), Cell_(Date_(2027, 1, 2))},
                                   {"APPEND(v, 2) APPEND(v, 1) APPEND(v, 2) x = MAX(v) y = MIN(v) z = SUM(v) w = AVERAGE(v)",
                                    "IF spot() > 100:2 THEN pay PAYS x ELSE pay PAYS y END"});
    const auto nested = product.PreProcess(true, false);
    const auto program = product.Compile(true);
    const Script::Detail::SegmentedTracePlan_ plan(program, {4}, {0, 0}, 1);
    ASSERT_EQ(plan.MaxWords(), 14);
    ASSERT_EQ(plan.Events(0), (Vector_<size_t>{0, 1}));
    const auto first = EvaluateTrace(&product, program, plan, nested, 100.0);
    ASSERT_EQ(first, (Vector_<std::uint64_t>{4, 1, 1, 3, 0, 4, 1, 2, 1, 0, 4, 4, 0, 0}));
    AAD::Sample_<> sample{};
    sample.spot_ = 100.0;
    sample.numeraire_ = 1.0;
    auto state = product.BuildEvalState<double>(nested, 2.0);
    auto trace = first;
    const Vector_<> observations;
    const Script::Detail::SegmentedCompiledPolicy_<double> policy(nullptr, observations, plan.View(1, &trace));
    Script::Detail::EvalCompiledEvents<false>(
        1,
        [&](size_t) {
            return Script::Detail::CompiledEventView_<double, Script::Detail::SegmentedCompiledPolicy_<double>>{
                program.NodeStreams()[1], program.ConstStreams()[1], sample, 0, 0, true, policy};
        },
        &state);
    ASSERT_EQ(trace[12], 3);
    ASSERT_EQ(trace[13], 3);
    ASSERT_TRUE(std::equal(first.begin(), first.begin() + 12, trace.begin()));
}

TEST(SegmentedScriptStateTest, TestTraceRejectsMalformedProgramsAndStorage) {
    using namespace Script;
    using Dal::ScriptError_;
    for (const Vector_<int>& stream :
         Vector_<Vector_<int>>{{999}, {LsmcPays}, {Max2Const}, {FuzzyIf, 5, 5, -1, 0}, {FuzzyIf, 0, 5, 0, 0}, {VectorReduce, 1, 0, 0, 0, 0}}) {
        const ScriptCompiled_ malformed(Vector_<Vector_<int>>{stream}, Vector_<Vector_<>>{{}});
        ASSERT_THROW((void)Detail::SegmentedTracePlan_(malformed, {1}, {0}, 1), ScriptError_);
    }
    const ScriptCompiled_ program(Vector_<Vector_<int>>{{Max2}}, Vector_<Vector_<>>{{}});
    ASSERT_THROW((void)Detail::SegmentedTracePlan_(program, {}, {}, 1), ScriptError_);
    ASSERT_THROW((void)Detail::SegmentedTracePlan_(program, {}, {1}, 1), ScriptError_);
    const Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
    Vector_<std::uint64_t> wrong;
    ASSERT_THROW((void)plan.View(0, &wrong), ScriptError_);
    ASSERT_THROW((void)plan.View(0, nullptr), ScriptError_);
    wrong.Resize(1);
    ASSERT_NO_THROW((void)plan.View(0, &wrong));
    ASSERT_THROW((void)plan.View(1, &wrong), ScriptError_);
    ASSERT_THROW((void)plan.Events(1), ScriptError_);
}
