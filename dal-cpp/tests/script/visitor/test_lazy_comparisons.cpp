#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/detail/segmentedtrace.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;

TEST(LazyComparisonsTest, TestPlateauComparisonRecordsNoActiveDifference) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    for (const String_ op : {" > ", " >= ", " < ", " <= ", " = "}) {
        Dal::Script::ScriptProduct_ product({Cell_("B"), Cell_(Date_(2024, 1, 2))}, {"100", "IF SPOT()" + op + "B:2 THEN pay = 1 ELSE pay = 0 END"});
        product.PreProcess(true, false);
        const auto& condition = *product.Events()[0][0]->arguments_[0];
        Dal::Script::Compiler_ compiler(true);
        condition.Accept(compiler);
        for (const bool compiled : {false, true}) {
            for (const double spot : {90.0, 110.0}) {
                auto check = [&](auto& evaluator, auto evaluate) {
                    Dal::AAD::Scenario_<Dal::AAD::Number_> path(1);
                    Dal::AAD::RecordingScope_ recording;
                    recording.RegisterInput(path[0].spot_, spot);
                    recording.RegisterInput(evaluator.ConstVarVals()[0], 100.0);
                    recording.StartRecording();
                    evaluator.InitRecordingConstants();
                    const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
                    evaluate(evaluator, path);
                    ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, 0U) << op << spot << " compiled=" << compiled;
                    recording.FinishRecording();
                };
                if (compiled) {
                    auto evaluator = product.BuildEvalState<Dal::AAD::Number_>(1, 2.0);
                    check(evaluator, [&](auto& state, const auto& path) {
                        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state);
                    });
                } else {
                    auto evaluator = product.BuildFuzzyEvaluator<Dal::AAD::Number_>(1, 2.0);
                    check(evaluator, [&](auto& state, const auto& path) {
                        state.SetScenario(&path);
                        state.SetCurEvt(0);
                        condition.Accept(state);
                    });
                }
            }
        }
    }
}

namespace {
    struct BoundaryPoint_ {
        double value_, spreadSlope_, butterflySlope_;
    };

    Dal::Vector_<BoundaryPoint_> BoundaryPoints(bool discrete) {
        const double lower = discrete ? -2.0 : -1.0;
        const double spread = discrete ? 1.0 / 3.0 : 0.5;
        const double left = discrete ? 0.5 : 1.0;
        const double infinity = std::numeric_limits<double>::infinity();
        return {
            {-3.0, 0.0, 0.0},      {std::nextafter(lower, -infinity), 0.0, 0.0}, {lower, spread, left}, {std::nextafter(lower, 0.0), spread, left},
            {-0.25, spread, left}, {0.0, spread, discrete ? -1.0 : 1.0},         {0.25, spread, -1.0},  {std::nextafter(1.0, 0.0), spread, -1.0},
            {1.0, spread, -1.0},   {std::nextafter(1.0, infinity), 0.0, 0.0},    {2.0, 0.0, 0.0}};
    }

    double ReferenceKernel(bool equality, bool discrete, double value) {
        if (discrete)
            return equality ? Dal::Script::BFly(value, -2.0, 1.0) : Dal::Script::CSpr(value, -2.0, 1.0);
        return equality ? Dal::Script::BFly(value, 2.0) : Dal::Script::CSpr(value, 2.0);
    }

    Dal::AAD::Number_ GuardedDegree(const Dal::Script::SmoothingComparison_& comparison,
                                    int operands,
                                    const Dal::AAD::Number_& input,
                                    const Dal::AAD::Number_& threshold,
                                    const Dal::Script::SmoothingConstants_<Dal::AAD::Number_>& constants) {
        if (operands == 0)
            return comparison.Difference(input, threshold, 2.0, constants);
        if (operands == 1)
            return comparison.Difference(input, 0.0, 2.0, constants);
        return comparison.Difference(0.0, input, 2.0, constants);
    }

    void CheckKernelPoint(const Dal::Script::SmoothingComparison_& comparison, const BoundaryPoint_& point, int operands) {
        const double slope = comparison.equality_ ? point.butterflySlope_ : point.spreadSlope_;
        const double expected = ReferenceKernel(comparison.equality_, comparison.discrete_, point.value_);
        for (int generation = 0; generation < 2; ++generation) {
            Dal::Script::SmoothingConstants_<Dal::AAD::Number_> constants;
            Dal::AAD::RecordingScope_ recording;
            Dal::AAD::Number_ input, threshold;
            recording.RegisterInput(input, operands == 2 ? -point.value_ : point.value_);
            recording.RegisterInput(threshold, 0.0);
            recording.StartRecording();
            constants.Initialize();
            const auto checkpoint = recording.MakeCheckpoint();
            recording.FinishRecording();
            for (int path = 0; path < 2; ++path) {
                recording.Restore(checkpoint);
                const auto degree = GuardedDegree(comparison, operands, input, threshold, constants);
                ASSERT_DOUBLE_EQ(Dal::AAD::Value(degree), expected);
                const Dal::AAD::Number_ payoff = degree + 0.0 * input;
                Dal::AAD::Adjoint(payoff) = 1.0;
                recording.FinishRecording();
                recording.ReverseSuffix(checkpoint);
            }
            recording.ReversePrefix(checkpoint);
            ASSERT_NEAR(Dal::AAD::Adjoint(input), (operands == 2 ? -2.0 : 2.0) * slope, 1e-12);
            ASSERT_NEAR(Dal::AAD::Adjoint(threshold), operands == 0 ? -2.0 * slope : 0.0, 1e-12);
        }
    }
} // namespace

TEST(LazyComparisonsTest, TestKernelsKeepInclusiveBoundaryAndThresholdDerivatives) {
    for (int mode = 0; mode < 3; ++mode) {
        for (const bool equality : {false, true}) {
            const Dal::Script::SmoothingComparison_ comparison{equality, mode == 2, mode == 2 ? -2.0 : (mode == 0 ? -1.0 : 2.0), 1.0};
            for (const auto& point : BoundaryPoints(mode == 2))
                for (int operands = 0; operands < 3; ++operands)
                    ASSERT_NO_FATAL_FAILURE(CheckKernelPoint(comparison, point, operands));
        }
    }
}

TEST(LazyComparisonsTest, TestTreeAndCompiledRecordInclusiveBoundaries) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    for (const String_ op : {" > ", " = "}) {
        Dal::Script::ScriptProduct_ product({Cell_("B"), Cell_(Date_(2024, 1, 2))}, {"0", "IF SPOT()" + op + "B:2 THEN pay = 1 ELSE pay = 0 END"});
        product.PreProcess(true, false);
        const auto& condition = *product.Events()[0][0]->arguments_[0];
        Dal::Script::Compiler_ compiler(true);
        condition.Accept(compiler);
        for (const bool compiled : {false, true}) {
            for (const auto& point : BoundaryPoints(false)) {
                auto check = [&](auto& evaluator, auto evaluate) {
                    Dal::AAD::Scenario_<Dal::AAD::Number_> path(1);
                    Dal::AAD::RecordingScope_ recording;
                    recording.RegisterInput(path[0].spot_, point.value_);
                    recording.RegisterInput(evaluator.ConstVarVals()[0], 0.0);
                    recording.StartRecording();
                    evaluator.InitRecordingConstants();
                    const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
                    evaluate(evaluator, path);
                    const size_t nodes = Dal::AAD::Tape()->nodes_.OccupiedSlots() - before;
                    if (point.value_ < -1.0 || point.value_ > 1.0)
                        ASSERT_EQ(nodes, 0U);
                    else
                        ASSERT_GT(nodes, 0U);
                    recording.FinishRecording();
                };
                if (compiled) {
                    auto evaluator = product.BuildEvalState<Dal::AAD::Number_>(1, 2.0);
                    check(evaluator, [&](auto& state, const auto& path) {
                        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state);
                    });
                } else {
                    auto evaluator = product.BuildFuzzyEvaluator<Dal::AAD::Number_>(1, 2.0);
                    check(evaluator, [&](auto& state, const auto& path) {
                        state.SetScenario(&path);
                        state.SetCurEvt(0);
                        condition.Accept(state);
                    });
                }
            }
        }
    }
}

namespace {
    void CheckTracePoint(const String_& expression, double spot, bool equality, double gap, std::uint64_t expected) {
        Dal::Script::ScriptProduct_ product({Cell_("B"), Cell_(Date_(2024, 1, 2))}, {"0", "IF " + expression + ":2 THEN pay = 1 ELSE pay = 0 END"});
        product.PreProcess(true, false);
        const auto& condition = *product.Events()[0][0]->arguments_[0];
        Dal::Script::Compiler_ compiler(true);
        condition.Accept(compiler);
        const Dal::Script::ScriptCompiled_ program({compiler.NodeStream()}, {compiler.ConstStream()});
        const Dal::Script::Detail::SegmentedTracePlan_ plan(program, {}, {0}, 1);
        ASSERT_EQ(plan.MaxWords(), 1U);
        Dal::Vector_<std::uint64_t> trace(1, 0);
        const Dal::Vector_<Dal::AAD::Number_> observations;
        using TracePolicy_ = Dal::Script::Detail::SegmentedCompiledPolicy_<Dal::AAD::Number_>;
        const TracePolicy_ policy(nullptr, observations, plan.View(0, &trace));
        auto state = product.BuildEvalState<Dal::AAD::Number_>(1, 2.0);
        Dal::AAD::Sample_<Dal::AAD::Number_> sample;
        Dal::AAD::Number_ threshold;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, spot);
        recording.RegisterInput(threshold, 0.0);
        if (!state.ConstVarVals().empty())
            state.ConstVarVals()[0] = threshold;
        recording.StartRecording();
        state.InitRecordingConstants();
        const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
        Dal::Script::Detail::EvalCompiledEvents<false>(
            1,
            [&](size_t) {
                return Dal::Script::Detail::CompiledEventView_<Dal::AAD::Number_, TracePolicy_>{
                    compiler.NodeStream(), compiler.ConstStream(), sample, 0, 0, true, policy};
            },
            &state);
        ASSERT_EQ(trace[0], expected);
        if (gap < -1.0 || gap > 1.0)
            ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, 0U);
        const double degree = equality ? Dal::Script::BFly(gap, 2.0) : Dal::Script::CSpr(gap, 2.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.dStack_.Top()), degree);
        recording.FinishRecording();
    }
} // namespace

TEST(LazyComparisonsTest, TestSegmentedTraceReadsPassiveGapWithoutPlateauNodes) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const Dal::Vector_<> gaps{-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0};
    const Dal::Vector_<std::uint64_t> spread{1, 2, 3, 3, 3, 4, 5}, butterfly{1, 2, 3, 4, 5, 6, 7};
    for (const bool equality : {false, true}) {
        const String_ op = equality ? " = " : " > ";
        for (size_t point = 0; point < gaps.size(); ++point) {
            const auto expected = equality ? butterfly[point] : spread[point];
            ASSERT_NO_FATAL_FAILURE(CheckTracePoint("SPOT()" + op + "B", gaps[point], equality, gaps[point], expected));
            ASSERT_NO_FATAL_FAILURE(CheckTracePoint("SPOT()" + op + "0", gaps[point], equality, gaps[point], expected));
            ASSERT_NO_FATAL_FAILURE(CheckTracePoint("0" + op + "SPOT()", -gaps[point], equality, gaps[point], expected));
        }
    }
}

TEST(LazyComparisonsTest, TestActiveOperandsRemainEager) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    Dal::Script::ScriptProduct_ product({Cell_(Date_(2024, 1, 2))}, {"APPEND(v, 100) IF SPOT() > v[0]:2 THEN pay = 1 END"});
    product.PreProcess(true, false);
    const auto& condition = *product.Events()[0][1]->arguments_[0];
    Dal::AAD::Scenario_<double> path(1);
    path[0].spot_ = 1000.0;
    auto tree = product.BuildFuzzyEvaluator<double>(1, 2.0);
    tree.SetScenario(&path);
    tree.SetCurEvt(0);
    ASSERT_THROW(condition.Accept(tree), Dal::Exception_);
    Dal::Script::Compiler_ compiler(true);
    condition.Accept(compiler);
    auto state = product.BuildEvalState<double>(1, 2.0);
    ASSERT_THROW(Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state), Dal::Exception_);
}

TEST(LazyComparisonsTest, TestNonfiniteGapsPreserveKernelFallback) {
    Dal::Script::SmoothingConstants_<double> constants;
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const bool equality : {false, true}) {
        const Dal::Script::SmoothingComparison_ comparison{equality, false, 2.0, 0.0};
        ASSERT_DOUBLE_EQ(comparison.Difference(infinity, 0.0, 2.0, constants), equality ? 0.0 : 1.0);
        ASSERT_DOUBLE_EQ(comparison.Difference(-infinity, 0.0, 2.0, constants), 0.0);
        ASSERT_TRUE(std::isnan(comparison.Difference(infinity, infinity, 2.0, constants)));
        ASSERT_TRUE(std::isnan(comparison.Difference(nan, 0.0, 2.0, constants)));
    }
}

TEST(LazyComparisonsTest, TestUnprocessedVariableMetadataKeepsRuntimeValue) {
    auto variable = std::make_unique<Dal::Script::NodeVar_>("x");
    variable->index_ = 0;
    ASSERT_TRUE(variable->isConst_);
    auto difference = std::make_unique<Dal::Script::NodeSub_>();
    difference->arguments_.push_back(std::move(variable));
    difference->arguments_.push_back(std::make_unique<Dal::Script::NodeConst_>(0.0));
    auto condition = std::make_unique<Dal::Script::NodeSup_>();
    condition->eps_ = 2.0;
    condition->arguments_.push_back(std::move(difference));
    auto assignment = [](double value) {
        auto target = std::make_unique<Dal::Script::NodeVar_>("pay");
        target->index_ = 1;
        auto result = std::make_unique<Dal::Script::NodeAssign_>();
        result->arguments_.push_back(std::move(target));
        result->arguments_.push_back(std::make_unique<Dal::Script::NodeConst_>(value));
        return result;
    };
    Dal::Script::NodeIf_ branch;
    branch.firstElse_ = 2;
    branch.affectedVars_ = {1};
    branch.arguments_.push_back(std::move(condition));
    branch.arguments_.push_back(assignment(1.0));
    branch.arguments_.push_back(assignment(0.0));
    Dal::Script::FuzzyEvaluator_<double> evaluator({2.0, 0.0}, {}, 1, 2.0);
    evaluator.Init();
    branch.Accept(evaluator);
    ASSERT_DOUBLE_EQ(evaluator.VarVals()[1], 1.0);
}

TEST(LazyComparisonsTest, TestDirectComparisonAstRetainsGenericFallback) {
    for (const bool equality : {false, true}) {
        for (const bool discrete : {false, true}) {
            std::unique_ptr<Dal::Script::CompNode_> condition;
            if (equality)
                condition = std::make_unique<Dal::Script::NodeEqual_>();
            else
                condition = std::make_unique<Dal::Script::NodeSup_>();
            condition->arguments_.push_back(std::make_unique<Dal::Script::NodeSpot_>());
            condition->eps_ = -1.0;
            condition->isDiscrete_ = discrete;
            condition->lb_ = -2.0;
            condition->rb_ = 1.0;
            Dal::Script::Compiler_ compiler(true);
            condition->Accept(compiler);
            for (const double spot : {-3.0, 0.25, 2.0}) {
                Dal::AAD::Scenario_<double> path(1);
                path[0].spot_ = spot;
                Dal::Script::FuzzyEvaluator_<double> tree({}, {}, 0, 2.0);
                tree.SetScenario(&path);
                tree.SetCurEvt(0);
                if (equality)
                    ASSERT_NO_THROW(condition->Accept(tree));
                else
                    ASSERT_NO_THROW(tree.VisitComp(*condition));
                Dal::Script::EvalState_<double> state({}, {}, 0, 2.0);
                Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), path[0], state);
                ASSERT_DOUBLE_EQ(state.dStack_.Top(), ReferenceKernel(equality, discrete, spot));
            }
        }
    }
}

namespace {
    struct OperandVariant_ {
        String_ left_, right_;
        double spotSign_, thresholdSign_;
    };

    void CheckEvaluatorPoint(const OperandVariant_& operands, bool equality, int mode, const BoundaryPoint_& point) {
        const String_ conditionText = operands.left_ + (equality ? " = " : " > ") + operands.right_ + (mode == 0 ? "" : ":2");
        SCOPED_TRACE(::testing::Message() << conditionText << " mode=" << mode << " gap=" << point.value_);
        Dal::Script::ScriptProduct_ product({Cell_("B"), Cell_(Date_(2024, 1, 2))}, {"0", "IF " + conditionText + " THEN pay = 1 ELSE pay = 0 END"});
        product.PreProcess(true, false);
        auto& condition = *product.Events()[0][0]->arguments_[0];
        if (mode == 2) {
            auto& comparison = dynamic_cast<Dal::Script::CompNode_&>(condition);
            comparison.isDiscrete_ = true;
            comparison.lb_ = -2.0;
            comparison.rb_ = 1.0;
        }
        const double expected = ReferenceKernel(equality, mode == 2, point.value_);
        Dal::AAD::Scenario_<double> passive(1);
        passive[0].spot_ = operands.spotSign_ * point.value_;
        auto tree = product.BuildFuzzyEvaluator<double>(1, 2.0);
        product.Evaluate(passive, tree);
        const double branchValue = expected < Dal::EPSILON ? 0.0 : (expected > 1.0 - Dal::EPSILON ? 1.0 : expected);
        ASSERT_DOUBLE_EQ(tree.VarVals()[product.PayOffIdx()], branchValue);
        Dal::Script::Compiler_ compiler(true);
        condition.Accept(compiler);
        auto state = product.BuildEvalState<Dal::AAD::Number_>(1, 2.0);
        Dal::AAD::Sample_<Dal::AAD::Number_> sample;
        Dal::AAD::Number_ threshold;
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(sample.spot_, passive[0].spot_);
        recording.RegisterInput(threshold, 0.0);
        if (!state.ConstVarVals().empty())
            state.ConstVarVals()[0] = threshold;
        recording.StartRecording();
        state.InitRecordingConstants();
        Dal::Script::EvalCompiled(compiler.NodeStream(), compiler.ConstStream(), sample, state);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.dStack_.Top()), expected);
        const Dal::AAD::Number_ payoff = state.dStack_.Top() + 0.0 * sample.spot_;
        Dal::AAD::Adjoint(payoff) = 1.0;
        recording.FinishRecording();
        recording.Reverse();
        const double slope = equality ? point.butterflySlope_ : point.spreadSlope_;
        ASSERT_NEAR(Dal::AAD::Adjoint(sample.spot_), operands.spotSign_ * slope, 1e-12);
        ASSERT_NEAR(Dal::AAD::Adjoint(threshold), operands.thresholdSign_ * slope, 1e-12);
    }
} // namespace

TEST(LazyComparisonsTest, TestEvaluatorOperandVariantsAndWidthsMatchAnalyticKernels) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const Dal::Vector_<OperandVariant_> variants{{"SPOT()", "B", 1.0, -1.0}, {"SPOT()", "0", 1.0, 0.0}, {"0", "SPOT()", -1.0, 0.0}};
    for (int mode = 0; mode < 3; ++mode)
        for (const bool equality : {false, true})
            for (const auto& operands : variants)
                for (const auto& point : BoundaryPoints(mode == 2))
                    ASSERT_NO_FATAL_FAILURE(CheckEvaluatorPoint(operands, equality, mode, point));
}
