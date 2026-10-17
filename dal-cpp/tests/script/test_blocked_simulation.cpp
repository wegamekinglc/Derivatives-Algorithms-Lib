//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/script/blockedsimulation.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    SimResults_ ScalarBatch(const PreparedScript_& product,
                            const Handle_<ModelData_>& model,
                            const Script::Detail::AADBatchSettings_& settings,
                            const std::optional<ScriptCompiled_>& program,
                            const PathBatch_& batch,
                            size_t slot) {
        auto mode = AAD::SetNumResultsForAAD(false, 1);
        auto scalarSettings = settings;
        scalarSettings.nPaths_ = 1;
        scalarSettings.payoffIndex_ = slot;
        SimResults_ result({"spot", "vol", "rate", "div"});
        Script::Detail::EvaluateAADBatch(product, model, scalarSettings, program, batch, &result);
        return result;
    }

    void AssertCommonPathRows(const PreparedScript_& product,
                              const Handle_<ModelData_>& model,
                              const Script::Detail::AADBatchSettings_& settings,
                              const std::optional<ScriptCompiled_>& program,
                              const Vector_<RiskOutputCoordinate_>& outputs,
                              size_t width) {
        const PathBatch_ batch{37, 13};
        const auto plan = AAD::PlanAdjointBlocks(outputs.size(), 2, {width});
        for (size_t index = 0; index < plan.BlockCount(); ++index) {
            const auto block = plan.Block(index);
            Script::Detail::AADBlockBatchResult_ result(block.width_, 2);
            Script::Detail::EvaluateAADBlockBatch(product, model, settings, program, batch, block, outputs, {1, 0}, &result);
            for (size_t lane = 0; lane < block.outputs_; ++lane) {
                const auto scalar = ScalarBatch(product, model, settings, program, batch, outputs[block.firstOutput_ + lane].slot_);
                ASSERT_NEAR(result.valueSums_[lane], scalar.aggregated_, 1.0e-10);
                ASSERT_NEAR(result.gradientSums_(static_cast<int>(lane), 0), scalar.risks_[1], 1.0e-10);
                ASSERT_NEAR(result.gradientSums_(static_cast<int>(lane), 1), scalar.risks_[0], 1.0e-10);
            }
        }
    }
} // namespace

TEST(BlockedSimulationTest, TestPreparedPrefixAliasesDirectInputsAndPaddingUseDistinctLanes) {
    const ScriptProductData_ data("blocked", {Cell_("X"), Cell_("Y"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))},
                                  {"2", "3", "a = X * Y b = a direct = X literal = 5", "pay PAYS 0"});
    const Handle_<ModelData_> modelData(new BSModelData_("model", 100.0, 0.2));
    AAD::BlackScholes_<double> metadata(100.0, 0.2);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    const Vector_<double> expected{6.0, 6.0, 2.0, 5.0, 0.0};
    const Vector_<double> expectedDy{2.0, 2.0, 0.0, 0.0, 0.0};
    const Vector_<double> expectedDx{3.0, 3.0, 1.0, 0.0, 0.0};
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto prepared = PrepareScript(data, &metadata, valuation, simulation);
        const auto outputs = ScriptRiskOutputAxis(prepared.Product());
        const std::optional<ScriptCompiled_> program = compiled ? std::optional<ScriptCompiled_>(prepared.Compile(true)) : std::nullopt;
        const Script::Detail::AADBatchSettings_ settings{simulation.rsg_, simulation.useBb_, -1, simulation.smooth_, 17, 4, 2, prepared.PayOffIdx()};
        for (const size_t width : {1, 2, 4}) {
            const auto plan = AAD::PlanAdjointBlocks(outputs.size(), 2, {width});
            for (size_t index = 0; index < plan.BlockCount(); ++index) {
                const auto block = plan.Block(index);
                Script::Detail::AADBlockBatchResult_ result(block.width_, 2);
                Script::Detail::EvaluateAADBlockBatch(prepared, modelData, settings, program, {5, 3}, block, outputs, {5, 4}, &result);
                for (size_t lane = 0; lane < block.outputs_; ++lane) {
                    const auto row = block.firstOutput_ + lane;
                    ASSERT_DOUBLE_EQ(result.valueSums_[lane], 3.0 * expected[row]);
                    ASSERT_NEAR(result.gradientSums_(static_cast<int>(lane), 0), 3.0 * expectedDy[row], 1.0e-10);
                    ASSERT_NEAR(result.gradientSums_(static_cast<int>(lane), 1), 3.0 * expectedDx[row], 1.0e-10);
                }
                for (size_t lane = block.outputs_; lane < block.width_; ++lane) {
                    ASSERT_DOUBLE_EQ(result.valueSums_[lane], 0.0);
                    ASSERT_DOUBLE_EQ(result.gradientSums_(static_cast<int>(lane), 0), 0.0);
                    ASSERT_DOUBLE_EQ(result.gradientSums_(static_cast<int>(lane), 1), 0.0);
                }
            }
        }
    }
}

TEST(BlockedSimulationTest, TestCommonAbsolutePathRangeMatchesIndependentScalarRows) {
    String_ event;
    for (size_t row = 0; row < 63; ++row)
        event += "o" + String_(std::to_string(row)) + " = " + String_(std::to_string(row + 1)) + " * SPOT() ";
    event += "pay = 64 * SPOT() pay PAYS 0";
    const ScriptProductData_ data("stochastic", {Cell_(Date_(2027, 1, 1))}, {event});
    const Handle_<ModelData_> modelData(new BSModelData_("model", 100.0, 0.2));
    AAD::BlackScholes_<double> metadata(100.0, 0.2);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        const auto product = PrepareScript(data, &metadata, valuation, simulation);
        const auto axis = ScriptRiskOutputAxis(product.Product());
        ASSERT_EQ(axis.size(), 64);
        const std::optional<ScriptCompiled_> program = compiled ? std::optional<ScriptCompiled_>(product.Compile(true)) : std::nullopt;
        const Script::Detail::AADBatchSettings_ settings{simulation.rsg_, simulation.useBb_, -1, simulation.smooth_, 13, 4, 0, product.PayOffIdx()};
        for (const size_t count : {1, 4, 16, 64}) {
            Vector_<RiskOutputCoordinate_> outputs(axis.begin(), axis.begin() + count);
            std::reverse(outputs.begin(), outputs.end());
            ASSERT_NO_FATAL_FAILURE(AssertCommonPathRows(product, modelData, settings, program, outputs, 3));
        }
    }
}

TEST(BlockedSimulationTest, TestCallerWorkerScratchGuardPreservesCoordinatorAndRecovers) {
    const ScriptProductData_ data("budget", {Cell_(Date_(2027, 1, 1))}, {"a = SPOT() pay PAYS a"});
    const Handle_<ModelData_> modelData(new BSModelData_("model", 100.0, 0.2));
    AAD::BlackScholes_<double> metadata(100.0, 0.2);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto product = PrepareScript(data, &metadata, valuation, simulation);
    const auto outputs = ScriptRiskOutputAxis(product.Product());
    const Script::Detail::AADBatchSettings_ settings{simulation.rsg_, simulation.useBb_, -1, simulation.smooth_, 3, 4, 0, product.PayOffIdx()};
    const AAD::AdjointBlock_ block{0, 2, 2};
    BufferCapacityBudget_ budget(1024 * 1024);
    AAD::TapeCapacityBudget_ tapeBudget(128 * 1024 * 1024);
    {
        BufferCapacityScope_ coordinator(&budget);
        Script::Detail::AADBlockBatchResult_ result(block.width_, 2);
        const auto retained = budget.CapacityBytes();
        Script::Detail::EvaluateAADBlockBatch(product, modelData, settings, {}, {37, 3}, block, outputs, {1, 0}, &result, &budget, &tapeBudget);
        ASSERT_GT(budget.PeakCapacityBytes(), retained);
        ASSERT_EQ(budget.CapacityBytes(), retained);
        ASSERT_GT(tapeBudget.CapacityBytes(), 0);
        ASSERT_GT(result.valueSums_[0], 0.0);
        ASSERT_NEAR(result.valueSums_[0], result.valueSums_[1], 1.0e-10);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
    BufferCapacityBudget_ insufficient(Script::Detail::AADBlockBatchFixedPayloadBytes(false) - 1);
    Script::Detail::AADBlockBatchResult_ untouched(block.width_, 2);
    ASSERT_THROW(Script::Detail::EvaluateAADBlockBatch(product, modelData, settings, {}, {37, 3}, block, outputs, {1, 0}, &untouched, &insufficient),
                 Exception_);
    ASSERT_EQ(insufficient.CapacityBytes(), 0);
    ASSERT_EQ(untouched.valueSums_, Vector_<>({0.0, 0.0}));
    Script::Detail::EvaluateAADBlockBatch(product, modelData, settings, {}, {37, 3}, block, outputs, {1, 0}, &untouched, &budget, &tapeBudget);
    ASSERT_GT(untouched.valueSums_[0], 0.0);
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BlockedSimulationTest, TestUnsupportedPreparationRejectsBeforeBudgetAttachment) {
    const ScriptProductData_ data("expired", {Cell_(Date_(2025, 1, 1))}, {"pay PAYS 1"});
    const Handle_<ModelData_> modelData(new BSModelData_("model", 100.0, 0.2));
    AAD::BlackScholes_<double> metadata(100.0, 0.2);
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 1, 1);
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto product = PrepareScript(data, &metadata, valuation, simulation);
    ASSERT_TRUE(product.AllExpired());
    const auto outputs = ScriptRiskOutputAxis(product.Product());
    const Script::Detail::AADBatchSettings_ settings{simulation.rsg_, simulation.useBb_, -1, simulation.smooth_, 1, 4, 0, product.PayOffIdx()};
    Script::Detail::AADBlockBatchResult_ result(1, 0);
    BufferCapacityBudget_ budget(0);
    try {
        Script::Detail::EvaluateAADBlockBatch(product, modelData, settings, {}, {0, 1}, {0, 1, 1}, outputs, {}, &result, &budget);
        FAIL() << "expired preparation must fail before budget admission";
    } catch (const ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("UnsupportedJacobianBatch"), std::string::npos);
    }
    ASSERT_EQ(budget.CapacityBytes(), 0);
}
