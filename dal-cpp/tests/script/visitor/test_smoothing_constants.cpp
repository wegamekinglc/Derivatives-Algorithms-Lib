//
// Created by Codex on 2026/10/12.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <utility>

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;

namespace {
    Dal::Script::ScriptProduct_ WeeklyBarrier() {
        Vector_<Cell_> dates{Cell_("B"), Cell_("K")};
        Vector_<String_> events{"120", "100"};
        for (int week = 1; week <= 53; ++week) {
            dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(7 * week)));
            events.push_back((week == 1 ? "alive = 1 " : "") + String_("IF SPOT() > B THEN alive = 0 END"));
        }
        events.back() += " pay PAYS alive * MAX(SPOT() - K, 0)";
        Dal::Script::ScriptProduct_ product(dates, events);
        product.PreProcess(true, true);
        return product;
    }
    template <class T_> T_ Kernel(bool spread, bool asymmetric, const T_& x, const Dal::Script::SmoothingConstants_<T_>& constants) {
        if (asymmetric)
            return spread ? Dal::Script::CSpr(x, -2.0, 1.0, constants) : Dal::Script::BFly(x, -2.0, 1.0, constants);
        return spread ? Dal::Script::CSpr(x, 2.0, constants) : Dal::Script::BFly(x, 2.0, constants);
    }

    struct BoundaryPoint_ {
        double x_, spreadSlope_, butterflySlope_;
    };

    struct PriceCase_ {
        double spot_, price_, delta_;
    };

    Vector_<BoundaryPoint_> BoundaryPoints(bool asymmetric) {
        const double lower = asymmetric ? -2.0 : -1.0;
        const double slope = asymmetric ? 1.0 / 3.0 : 0.5;
        const double leftSlope = asymmetric ? 0.5 : 1.0;
        const double infinity = std::numeric_limits<double>::infinity();
        return {{-3.0, 0.0, 0.0},
                {std::nextafter(lower, -infinity), 0.0, 0.0},
                {lower, slope, leftSlope},
                {std::nextafter(lower, 0.0), slope, leftSlope},
                {-0.25, slope, leftSlope},
                {0.0, slope, asymmetric ? -1.0 : 1.0},
                {0.25, slope, -1.0},
                {std::nextafter(1.0, 0.0), slope, -1.0},
                {1.0, slope, -1.0},
                {std::nextafter(1.0, infinity), 0.0, 0.0},
                {2.0, 0.0, 0.0}};
    }
} // namespace

TEST(SmoothingConstantsTest, TestWeeklyBarrierPayoffFitsLeafBudget) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const auto product = WeeklyBarrier();
    const auto compiled = product.Compile(true);
    for (const bool useCompiled : {false, true}) {
        Dal::AAD::BlackScholes_<Dal::AAD::Number_> model(100.0, 0.2, 0.05, 0.02);
        model.Allocate(product.TimeLine(), product.DefLine());
        Dal::AAD::Scenario_<Dal::AAD::Number_> path;
        Dal::AAD::AllocatePath(product.DefLine(), path);
        auto check = [&](auto& evaluator, auto evaluate) {
            for (int generation = 0; generation < 2; ++generation) {
                Dal::AAD::RecordingScope_ recording;
                const auto checkpoint = Dal::Script::InitModel4ParallelAAD(product, model, path, evaluator, recording);
                recording.FinishRecording();
                for (const double normal : {0.0, 0.1}) {
                    recording.Restore(checkpoint);
                    model.GeneratePath(Vector_<>(model.SimDim(), normal), &path);
                    auto first = Dal::AAD::Tape()->nodes_.End();
                    evaluate(evaluator);
                    size_t leaves = 0;
                    for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                        leaves += first->NumArguments() == 0;
                    ASSERT_LE(leaves, 10U) << "compiled=" << useCompiled;
                    const auto& value = evaluator.VarVals()[product.PayOffIdx()];
                    ASSERT_GT(Dal::AAD::Value(value), 0.0);
                    Dal::AAD::Adjoint(value) = 1.0;
                    recording.FinishRecording();
                    recording.ReverseSuffix(checkpoint);
                }
                recording.ReversePrefix(checkpoint);
                ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(evaluator.ConstVarVals()[0]), 0.0);
            }
        };
        if (useCompiled) {
            auto evaluator = product.BuildEvalState<Dal::AAD::Number_>(1, 0.01);
            check(evaluator, [&](auto& state) { compiled.Evaluate(path, state); });
        } else {
            auto evaluator = product.BuildFuzzyEvaluator<Dal::AAD::Number_>(1, 0.01);
            check(evaluator, [&](auto& state) { product.Evaluate(path, state); });
        }
    }
}

TEST(SmoothingConstantsTest, TestCachedKernelBoundariesPreserveInputAndThresholdRisks) {
    for (const bool asymmetric : {false, true}) {
        for (const bool spread : {false, true}) {
            for (const auto& point : BoundaryPoints(asymmetric)) {
                Dal::Script::SmoothingConstants_<Dal::AAD::Number_> constants;
                for (int generation = 0; generation < 2; ++generation) {
                    Dal::AAD::RecordingScope_ recording;
                    Dal::AAD::Number_ input, threshold;
                    recording.RegisterInput(input, point.x_);
                    recording.RegisterInput(threshold, 0.0);
                    recording.StartRecording();
                    constants.Initialize();
                    const auto checkpoint = recording.MakeCheckpoint();
                    recording.FinishRecording();
                    for (int path = 0; path < 2; ++path) {
                        recording.Restore(checkpoint);
                        const Dal::AAD::Number_ gap = input - threshold;
                        const Dal::AAD::Number_ degree = Kernel(spread, asymmetric, gap, constants);
                        const Dal::Script::SmoothingConstants_<double> passive;
                        ASSERT_DOUBLE_EQ(Dal::AAD::Value(degree), Kernel(spread, asymmetric, point.x_, passive));
                        const Dal::AAD::Number_ payoff = degree + 0.0 * input;
                        Dal::AAD::Adjoint(payoff) = 1.0;
                        recording.FinishRecording();
                        recording.ReverseSuffix(checkpoint);
                    }
                    recording.ReversePrefix(checkpoint);
                    const double slope = spread ? point.spreadSlope_ : point.butterflySlope_;
                    ASSERT_NEAR(Dal::AAD::Adjoint(input), 2.0 * slope, 1e-12) << point.x_;
                    ASSERT_NEAR(Dal::AAD::Adjoint(threshold), -2.0 * slope, 1e-12) << point.x_;
                }
            }
        }
    }
}

TEST(SmoothingConstantsTest, TestCacheCopiesFallBackAndOwnerMovesReusePrefix) {
    Dal::AAD::RecordingScope_ recording;
    recording.StartRecording();
    Dal::Script::SmoothingConstants_<Dal::AAD::Number_> original;
    original.Initialize();
    const auto copied = original;
    Dal::Script::SmoothingConstants_<Dal::AAD::Number_> assigned;
    assigned.Initialize();
    assigned = original;
    auto moved = std::move(original);
    Dal::Script::SmoothingConstants_<Dal::AAD::Number_> moveAssigned;
    moveAssigned = std::move(moved);
    auto check = [](const auto& constants, size_t expectedNodes) {
        const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
        const auto zero = constants.Zero(), one = constants.One();
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(zero), 0.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Value(one), 1.0);
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, expectedNodes);
    };
    ASSERT_NO_FATAL_FAILURE(check(copied, 2));
    ASSERT_NO_FATAL_FAILURE(check(assigned, 2));
    ASSERT_NO_FATAL_FAILURE(check(moveAssigned, 0));
    const Dal::AAD::Number_ x = -2.0;
    const size_t before = Dal::AAD::Tape()->nodes_.OccupiedSlots();
    const auto direct = Dal::Script::CSpr(x, 2.0);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(direct), 0.0);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.OccupiedSlots() - before, 1U);
    recording.FinishRecording();
}

TEST(SmoothingConstantsTest, TestCompoundConditionsPreserveActiveBarrierAndModelRisks) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const Dal::Script::ScriptProductData_ product(
        "", {Cell_("B"), Cell_(Date_(2024, 1, 1).AddDays(365))},
        {"100", "IF (SPOT() > B:2 AND SPOT() != B:2) OR (0 > 1:0.01) THEN pay PAYS SPOT() ELSE pay PAYS B END"});
    for (const auto& [spot, price, delta] : {PriceCase_{90.0, 100.0, 0.0}, PriceCase_{100.25, 100.0390625, 0.34375}, PriceCase_{110.0, 110.0, 1.0}}) {
        const Dal::Handle_<Dal::ModelData_> model(new Dal::BSModelData_("", spot, 0.0, 0.0, 0.0));
        Vector_<String_> treeNames;
        Vector_<> treeRisks;
        for (const bool compiled : {false, true}) {
            Dal::Script::MonteCarloSettings_ settings;
            settings.compiled_ = compiled;
            settings.enableAad_ = true;
            settings.smooth_ = 2.0;
            const auto result = Dal::Script::MCSimulation<Dal::AAD::Number_>(product, model, 16, {}, settings);
            ASSERT_NEAR(result.aggregated_ / 16.0, price, 1e-12);
            ASSERT_NEAR(result["spot"], delta, 1e-12);
            ASSERT_NEAR(result["B"], 1.0 - delta, 1e-12);
            ASSERT_NEAR(result["rate"], spot * delta - price, 1e-12);
            ASSERT_NEAR(result["div"], -spot * delta, 1e-12);
            if (compiled) {
                ASSERT_EQ(result.names_, treeNames);
                ASSERT_EQ(result.risks_.size(), treeRisks.size());
                for (size_t input = 0; input < treeRisks.size(); ++input)
                    ASSERT_NEAR(result.risks_[input], treeRisks[input], 1e-12);
            } else {
                treeNames = result.names_;
                treeRisks = result.risks_;
            }
        }
    }
}
