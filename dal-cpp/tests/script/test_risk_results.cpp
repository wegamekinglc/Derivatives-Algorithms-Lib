//
// Created by Codex on 2026/10/4.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/script/riskresults.hpp>
#include <dal/script/simulation.hpp>

using Dal::String_;
using Dal::Vector_;
using namespace Dal::Script;

namespace {
    Vector_<RiskCoordinate_> Coordinates() {
        return {{"model:0", "spot", "model", 0, 100.0, "asset_price", "asset_price", 1.0},
                {"model:1", "vol", "model", 1, 0.2, "decimal_volatility", "decimal_volatility", 1.0},
                {"constant:0", "strike", "constant", 0, 90.0, "script_numeric", std::nullopt, 1.0}};
    }

    SimResults_ Source() {
        SimResults_ source({"spot", "vol", "strike"});
        source.aggregated_ = 120.0;
        source.risks_ = {2.0, 7.0, -3.0};
        return source;
    }

    RiskResultProvenance_ Provenance() {
        RiskResultProvenance_ provenance;
        provenance.method_ = "NativeAAD";
        provenance.modelType_ = "BSModelData_";
        return provenance;
    }
} // namespace

TEST(RiskResultTest, TestMeanProjectionDoesNotRenormalizeGradient) {
    const auto result = ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, Provenance());
    ASSERT_EQ(result.OutputIds(), Vector_<String_>({"payoff"}));
    ASSERT_EQ(result.Values(), Vector_<>({12.0}));
    ASSERT_EQ(result.Jacobian().Rows(), 1);
    ASSERT_EQ(result.Jacobian().Cols(), 3);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 2.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 7.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 2), -3.0);
    ASSERT_EQ(result.CompleteInputAxis().size(), 3);
    ASSERT_DOUBLE_EQ(result.CompleteInputAxis()[1].value_, 0.2);
    ASSERT_EQ(result.Provenance().method_, "NativeAAD");
    ASSERT_EQ(result.Provenance().normalization_, "mean");
}

TEST(RiskResultTest, TestSelectedOrderAndReportingScale) {
    RiskRequest_ request;
    request.inputs_ = Vector_<String_>{"constant:0", "MODEL:1"};
    request.reportFactors_ = Vector_<>{0.5, 0.01};
    const auto result = ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance());
    ASSERT_EQ(result.InputAxis()[0].id_, "constant:0");
    ASSERT_EQ(result.InputAxis()[1].id_, "model:1");
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), -3.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 7.0);
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto reported = result.ReportedJacobian();
        ASSERT_DOUBLE_EQ(reported(0, 0), -1.5);
        ASSERT_DOUBLE_EQ(reported(0, 1), 0.07);
        ASSERT_DOUBLE_EQ(result.LegacyValues().at("d_vol"), 7.0);
    }
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), 7.0);
}

TEST(RiskResultTest, TestEmptyInputSelectionKeepsTwoDimensionalPrice) {
    RiskRequest_ request;
    request.inputs_ = Vector_<String_>{};
    const auto result = ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance());
    ASSERT_EQ(result.Jacobian().Rows(), 1);
    ASSERT_EQ(result.Jacobian().Cols(), 0);
    ASSERT_TRUE(result.InputAxis().empty());
    ASSERT_EQ(result.ReportedJacobian().Rows(), 1);
    ASSERT_EQ(result.ReportedJacobian().Cols(), 0);
    ASSERT_EQ(result.LegacyValues().size(), 1);
    ASSERT_DOUBLE_EQ(result.LegacyValues().at("PV"), 12.0);
}

TEST(RiskResultTest, TestDisplayCollisionPreservesDistinctNumericColumns) {
    auto coordinates = Coordinates();
    coordinates[2].label_ = "SPOT";
    auto source = Source();
    source.names_[2] = "SPOT";
    const auto result = ProjectMonteCarloRiskResult(source, 10, coordinates, {}, Provenance());
    ASSERT_EQ(result.InputAxis()[0].id_, "model:0");
    ASSERT_EQ(result.InputAxis()[2].id_, "constant:0");
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 2.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 2), -3.0);
    ASSERT_THROW(static_cast<void>(result.LegacyValues()), Dal::ScriptError_);
}

TEST(RiskResultTest, TestRequestAndScaleErrors) {
    for (const Vector_<String_>& ids : {Vector_<String_>{"unknown"}, Vector_<String_>{"model:0", "MODEL:0"}}) {
        RiskRequest_ request;
        request.inputs_ = ids;
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())), Dal::ScriptError_);
    }
    for (const Vector_<String_>& ids : {Vector_<String_>{}, Vector_<String_>{"unknown"}, Vector_<String_>{"payoff", "PAYOFF"}}) {
        RiskRequest_ request;
        request.outputs_ = ids;
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())), Dal::ScriptError_);
    }
    for (const double factor : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        RiskRequest_ request;
        request.reportFactors_ = Vector_<>{factor, 1.0, 1.0};
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())), Dal::ScriptError_);
    }
    RiskRequest_ wrongFactors;
    wrongFactors.reportFactors_ = Vector_<>{1.0};
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), wrongFactors, Provenance())), Dal::ScriptError_);
}

TEST(RiskResultTest, TestAxisAndDimensionErrors) {
    {
        auto axis = Coordinates();
        axis[1].id_ = "model:0";
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, axis, {}, Provenance())), Dal::ScriptError_);
    }
    {
        auto axis = Coordinates();
        axis[1].ordinal_ = 4;
        axis[1].id_ = "model:4";
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, axis, {}, Provenance())), Dal::ScriptError_);
    }
    {
        auto source = Source();
        source.risks_.pop_back();
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, Coordinates(), {}, Provenance())), Dal::ScriptError_);
    }
    {
        auto axis = Coordinates();
        axis[1].label_ = "wrong";
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, axis, {}, Provenance())), Dal::ScriptError_);
    }
}

TEST(RiskResultTest, TestPayloadBudgetBoundariesAndOverflow) {
    ASSERT_EQ(RiskResultPayloadBytes(1, 3), 4 * sizeof(double));
    ASSERT_EQ(RiskResultPayloadBytes(1, 0), sizeof(double));
    ASSERT_THROW(static_cast<void>(RiskResultPayloadBytes(1, std::numeric_limits<size_t>::max())), Dal::ScriptError_);
    ASSERT_THROW(static_cast<void>(RiskResultPayloadBytes(std::numeric_limits<size_t>::max(), 1)), Dal::ScriptError_);
    RiskRequest_ request;
    request.numericPayloadBudgetBytes_ = 4 * sizeof(double);
    ASSERT_NO_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())));
    request.numericPayloadBudgetBytes_ = 4 * sizeof(double) - 1;
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())), Dal::ScriptError_);
    request.inputs_ = Vector_<String_>{};
    request.numericPayloadBudgetBytes_ = sizeof(double);
    ASSERT_NO_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())));
    request.numericPayloadBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), request, Provenance())), Dal::ScriptError_);
}

TEST(RiskResultTest, TestNumericFailuresAndSelectedRiskOnly) {
    for (const int paths : {0, -1})
        ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), paths, Coordinates(), {}, Provenance())), Dal::ScriptError_);
    auto source = Source();
    source.aggregated_ = std::numeric_limits<double>::infinity();
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, Coordinates(), {}, Provenance())), Dal::ScriptError_);
    source = Source();
    source.risks_[1] = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, Coordinates(), {}, Provenance())), Dal::ScriptError_);
    RiskRequest_ selected;
    selected.inputs_ = Vector_<String_>{"constant:0"};
    ASSERT_NO_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, Coordinates(), selected, Provenance())));
    source = Source();
    source.risks_[1] = std::numeric_limits<double>::max();
    selected.inputs_ = Vector_<String_>{"model:1"};
    selected.reportFactors_ = Vector_<>{2.0};
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, Coordinates(), selected, Provenance())), Dal::ScriptError_);
}

TEST(RiskResultTest, TestResultSnapshotSurvivesFailureAndLaterInputs) {
    auto source = Source();
    auto axis = Coordinates();
    auto provenance = Provenance();
    const auto first = ProjectMonteCarloRiskResult(source, 10, axis, {}, provenance);
    source.risks_[1] = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(source, 10, axis, {}, provenance)), Dal::ScriptError_);
    source.risks_[1] = 11.0;
    axis[1].value_ = 0.3;
    provenance.method_ = "NativeAAD+RetrainedBump";
    const auto last = ProjectMonteCarloRiskResult(source, 10, axis, {}, provenance);
    ASSERT_DOUBLE_EQ(first.Jacobian()(0, 1), 7.0);
    ASSERT_DOUBLE_EQ(first.CompleteInputAxis()[1].value_, 0.2);
    ASSERT_EQ(first.Provenance().method_, "NativeAAD");
    ASSERT_DOUBLE_EQ(last.Jacobian()(0, 1), 11.0);
    ASSERT_DOUBLE_EQ(last.CompleteInputAxis()[1].value_, 0.3);
}

TEST(RiskResultTest, TestCallerProvidedExecutionSnapshotMustBeStructurallyValid) {
    auto provenance = Provenance();
    provenance.execution_.emplace();
    auto& execution = *provenance.execution_;
    execution.pathsPerReplicate_ = 10;
    execution.productDates_ = {Dal::Cell_("STRIKE")};
    execution.productEvents_ = {"90"};
    ASSERT_NO_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)));
    execution.productEvents_.clear();
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)), Dal::ScriptError_);
    execution.productEvents_ = {"90"};
    execution.pathsPerReplicate_ = 9;
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)), Dal::ScriptError_);
    execution.pathsPerReplicate_ = 10;
    execution.pricingReplicates_ = 0;
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)), Dal::ScriptError_);
    execution.pricingReplicates_ = 1;
    execution.observations_.push_back({"EQ[A]", Dal::DateTime_(Dal::Date_(2026, 9, 12)), true, std::numeric_limits<double>::quiet_NaN()});
    ASSERT_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)), Dal::ScriptError_);
    execution.observations_[0].value_ = 100.0;
    ASSERT_NO_THROW(static_cast<void>(ProjectMonteCarloRiskResult(Source(), 10, Coordinates(), {}, provenance)));
}
