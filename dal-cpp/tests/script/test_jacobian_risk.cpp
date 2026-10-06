//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/script/blockedreplay.hpp>
#include <dal/script/jacobianrisk.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ JacobianProduct() {
        ScriptProduct_ product({Cell_(Date_(2027, 1, 1))}, {"a = 1 b = 2 pay = a + b"}, "b");
        product.IndexVariables();
        return product;
    }

    Vector_<RiskCoordinate_> JacobianInputs() {
        return {{"model:0", "spot", "model", 0, 2.0, "model-coordinate", std::nullopt, 1.0},
                {"model:1", "vol", "model", 1, 0.2, "model-coordinate", std::nullopt, 1.0},
                {"constant:0", "strike", "constant", 0, 3.0, "script-number", std::nullopt, 1.0}};
    }
} // namespace

TEST(JacobianRiskPlanTest, TestOrderedAxesExactPayloadAndOwningSelections) {
    JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"OUTPUT:2", "output:0"};
    request.selection_.inputs_ = Vector_<String_>{"constant:0", "MODEL:1"};
    request.selection_.reportFactors_ = Vector_<>{0.5, 2.0};
    request.selection_.numericPayloadBudgetBytes_ = 6 * sizeof(double);
    request.maxBlockWidth_ = 4;
    const auto plan = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), request, true);
    ASSERT_EQ(plan.OutputAxis().size(), 2);
    ASSERT_EQ(plan.OutputAxis()[0].id_, "output:2");
    ASSERT_EQ(plan.OutputAxis()[1].id_, "output:0");
    ASSERT_EQ(plan.CompleteOutputAxis().size(), 3);
    ASSERT_EQ(plan.InputPositions(), Vector_<size_t>({2, 1}));
    ASSERT_EQ(plan.InputAxis()[0].id_, "constant:0");
    ASSERT_EQ(plan.InputAxis()[1].id_, "model:1");
    ASSERT_DOUBLE_EQ(plan.InputAxis()[0].reportScale_, 0.5);
    ASSERT_DOUBLE_EQ(plan.InputAxis()[1].reportScale_, 2.0);
    ASSERT_EQ(plan.CompleteInputAxis().size(), 3);
    ASSERT_EQ(plan.NumericPayloadBytes(), 6 * sizeof(double));
    ASSERT_EQ(plan.Request().maxBlockWidth_, 4);
    request.selection_.outputs_->clear();
    request.selection_.reportFactors_->front() = 99.0;
    ASSERT_EQ(plan.OutputAxis().size(), 2);
    ASSERT_DOUBLE_EQ(plan.InputAxis()[0].reportScale_, 0.5);
    request = plan.Request();
    request.selection_.numericPayloadBudgetBytes_ = 6 * sizeof(double) - 1;
    ASSERT_THROW(static_cast<void>(PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), request, true)), ScriptError_);
}

TEST(JacobianRiskPlanTest, TestDefaultAndNativePassiveEmptyColumns) {
    const auto native = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), {}, true);
    ASSERT_EQ(native.OutputAxis().size(), 1);
    ASSERT_EQ(native.OutputAxis()[0].id_, "payoff");
    ASSERT_EQ(native.InputAxis().size(), 3);
    ASSERT_TRUE(native.EnableAad());
    JacobianRiskRequest_ empty;
    empty.selection_.inputs_ = Vector_<String_>{};
    for (const bool enableAad : {false, true}) {
        const auto plan = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), empty, enableAad);
        ASSERT_TRUE(plan.InputAxis().empty());
        ASSERT_TRUE(plan.InputPositions().empty());
        ASSERT_EQ(plan.NumericPayloadBytes(), sizeof(double));
        ASSERT_EQ(plan.EnableAad(), enableAad);
    }
    empty.selection_.inputs_ = Vector_<String_>{"model:0"};
    ASSERT_THROW(static_cast<void>(PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), empty, false)), ScriptError_);
}

TEST(JacobianRiskResultTest, TestOwningMatricesScalingAndExecutionEvidence) {
    JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:2", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"constant:0", "model:0"};
    request.selection_.reportFactors_ = Vector_<>{0.5, 2.0};
    const auto plan = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), request, true);
    Script::Detail::AADBlockReplayResult_ source(2, 2, Vector_<size_t>{1, 1});
    source.values_ = {6.0, 2.0};
    source.jacobian_(0, 0) = 3.0;
    source.jacobian_(0, 1) = 4.0;
    source.jacobian_(1, 0) = 5.0;
    source.jacobian_(1, 1) = 6.0;
    source.replayAttempts_ = 2;
    source.executedPaths_ = 34;
    source.peakScratchBytes_ = 1024;
    source.peakTapeBytes_ = 4096;
    RiskResultProvenance_ provenance;
    provenance.method_ = "NativeAAD";
    provenance.modelType_ = "BS";
    const auto result = ProjectJacobianRiskResult(std::move(source), 17, plan, provenance);
    ASSERT_EQ(result.Values(), Vector_<>({6.0, 2.0}));
    ASSERT_EQ(result.Jacobian().Rows(), 2);
    ASSERT_EQ(result.Jacobian().Cols(), 2);
    auto reported = result.ReportedJacobian();
    ASSERT_DOUBLE_EQ(reported(0, 0), 1.5);
    ASSERT_DOUBLE_EQ(reported(0, 1), 8.0);
    ASSERT_DOUBLE_EQ(reported(1, 0), 2.5);
    ASSERT_DOUBLE_EQ(reported(1, 1), 12.0);
    reported(0, 0) = 99.0;
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 3.0);
    ASSERT_EQ(result.Execution().actualWidths_, Vector_<size_t>({1, 1}));
    ASSERT_EQ(result.Execution().replayAttempts_, 2);
    ASSERT_EQ(result.Execution().executedPaths_, 34);
    ASSERT_EQ(result.Execution().peakScratchBytes_, 1024);
    ASSERT_EQ(result.Execution().peakRecordingBytes_, 4096);
    ASSERT_EQ(result.Provenance().engine_, "native");
}

TEST(JacobianRiskPlanTest, TestMalformedSelectionsWidthsAndFactorsAreRejected) {
    const auto plan = [&](const JacobianRiskRequest_& request) {
        return PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), request, true);
    };
    JacobianRiskRequest_ request;
    for (const size_t width : {size_t{0}, AAD::ADJ_SIZE + 1}) {
        request.maxBlockWidth_ = width;
        ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    }
    request.maxBlockWidth_ = AAD::ADJ_SIZE;
    ASSERT_NO_THROW(plan(request));
    for (const Vector_<String_>& outputs : {Vector_<String_>{}, Vector_<String_>{"output:0", "OUTPUT:0"}, Vector_<String_>{"output:99"}}) {
        request.selection_.outputs_ = outputs;
        ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    }
    request.selection_.outputs_.reset();
    request.selection_.inputs_ = Vector_<String_>{"model:0", "MODEL:0"};
    ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    request.selection_.inputs_ = Vector_<String_>{"model:99"};
    ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    request.selection_.inputs_ = Vector_<String_>{"model:0"};
    request.selection_.reportFactors_ = Vector_<>{};
    ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    for (const double factor : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        request.selection_.reportFactors_ = Vector_<>{factor};
        ASSERT_THROW(static_cast<void>(plan(request)), ScriptError_);
    }
}

TEST(JacobianRiskResultTest, TestNonfiniteNumbersRetainOutputAndInputIdentity) {
    JacobianRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:2", "payoff"};
    request.selection_.inputs_ = Vector_<String_>{"model:0"};
    request.selection_.reportFactors_ = Vector_<>{2.0};
    const auto plan = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), request, true);
    RiskResultProvenance_ provenance;
    provenance.method_ = "NativeAAD";
    provenance.modelType_ = "BS";
    for (int failure = 0; failure < 3; ++failure) {
        Script::Detail::AADBlockReplayResult_ source(2, 1, Vector_<size_t>{1, 1});
        source.executedPaths_ = 34;
        source.replayAttempts_ = 2;
        source.values_[0] = failure == 0 ? std::numeric_limits<double>::infinity() : 0.0;
        source.jacobian_(0, 0) = failure == 1 ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::max();
        try {
            static_cast<void>(ProjectJacobianRiskResult(std::move(source), 17, plan, provenance));
            FAIL() << "expected a non-finite Jacobian projection error";
        } catch (const ScriptError_& error) {
            const std::string message = error.what();
            ASSERT_NE(message.find("output=output:2"), std::string::npos);
            if (failure > 0)
                ASSERT_NE(message.find("input=model:0"), std::string::npos);
        }
    }
}

TEST(JacobianRiskResultTest, TestExecutionCountsAndBlockWidthsCannotBeForged) {
    const auto plan = PlanJacobianRiskRequest(JacobianProduct(), JacobianInputs(), Date_(2026, 1, 1), {}, true);
    RiskResultProvenance_ provenance;
    provenance.method_ = "NativeAAD";
    provenance.modelType_ = "BS";
    for (int failure = 0; failure < 3; ++failure) {
        Script::Detail::AADBlockReplayResult_ source(1, 3, Vector_<size_t>{1});
        source.executedPaths_ = failure == 0 ? 16 : 17;
        source.replayAttempts_ = failure == 1 ? 2 : 1;
        source.actualWidths_[0] = failure == 2 ? 0 : 1;
        ASSERT_THROW(static_cast<void>(ProjectJacobianRiskResult(std::move(source), 17, plan, provenance)), ScriptError_);
    }
}
