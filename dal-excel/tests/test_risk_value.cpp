//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__models_test_api.hpp>
#include <dal-excel/src/__risk.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccy_test_api.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct RejectGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };

    Matrix_<Cell_> Setting(const String_& key, const Cell_& value) {
        Matrix_<Cell_> cells(1, 2);
        cells(0, 0) = key;
        cells(0, 1) = value;
        return cells;
    }

    template <class F_> void AssertError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected error for " << field;
        } catch (const std::exception& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ExcelRiskTest, TestRequestAndResultGettersMatchNativeRiskWithoutRevaluation) {
    Excel::ScriptTestInitialize(1);
    Handle_<ScriptProductData_> product;
    Product_New("risk", {Cell_("STRIKE"), Cell_(double(Date::ToExcel(Date_(2023, 9, 25))))}, {"100", "call PAYS MAX(SPOT() - STRIKE, 0)"}, &product);
    Handle_<ModelData_> model;
    CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.2}, {0.02}, 0.05, Matrix_<>(1, 1, 1.0), &model);
    ScriptValuationSettings_ settings;
    settings.evaluationDate_ = Date_(2022, 9, 25);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", settings));
    Matrix_<Cell_> options(2, 2);
    options(0, 0) = "inputs";
    options(0, 1) = "constant:0;model:1";
    options(1, 0) = "report_factors";
    options(1, 1) = "0.5;0.01";
    Handle_<StorableRiskRequest_> request;
    RiskRequest_New("selection", options, &request);
    Handle_<StorableRiskResult_> result;
    MonteCarlo_ValueWithRisk(product, model, 257, request, valuation, {}, &result);
    MonteCarloSettings_ execution;
    execution.enableAad_ = true;
    const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("native", execution));
    Matrix_<Cell_> legacy;
    MonteCarlo_ValueWithSettings(product, model, 257, valuation, simulation, &legacy);
    auto legacyValue = [&](const String_& key) {
        for (int row = 0; row < legacy.Rows(); ++row)
            if (Cell::ToString(legacy(row, 0)) == key)
                return Cell::ToDouble(legacy(row, 1));
        THROW("missing reference key " + key);
    };
    RejectGetterWork_ reject;
    Matrix_<Cell_> values, raw, reported, shape, axis;
    RiskResult_Get_Values(result, &values);
    RiskResult_Get_Jacobian(result, false, &raw);
    RiskResult_Get_Jacobian(result, true, &reported);
    RiskResult_Get_Shape(result, &shape);
    RiskResult_Get_Inputs(result, false, &axis);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(values(0, 0)), legacyValue("PV"));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 1)), legacyValue("d_vol:EQ[A]"));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(0, 1)), 0.01 * legacyValue("d_vol:EQ[A]"));
    ASSERT_EQ(Cell::ToDouble(shape(0, 0)), 1.0);
    ASSERT_EQ(Cell::ToDouble(shape(0, 1)), 2.0);
    ASSERT_EQ(Cell::ToString(axis(1, 0)), String_("constant:0"));
    ASSERT_EQ(Cell::ToString(axis(2, 0)), String_("model:1"));
    raw(0, 1) = -999;
    RiskResult_Get_Jacobian(result, false, &raw);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 1)), legacyValue("d_vol:EQ[A]"));
    Matrix_<Cell_> provenance, productCopy, history, compatibility;
    Vector_<String_> outputs, json;
    RiskResult_Get_Provenance(result, &provenance);
    RiskResult_Get_Product(result, &productCopy);
    RiskResult_Get_History(result, &history);
    RiskResult_Get_Outputs(result, &outputs);
    RiskResult_Get_ModelSnapshot(result, &json);
    RiskResult_Get_LegacyValues(result, &compatibility);
    ASSERT_EQ(Cell::ToString(provenance(0, 1)), String_("NativeAAD"));
    ASSERT_EQ(Cell::ToString(productCopy(1, 1)), String_("call PAYS MAX(SPOT() - STRIKE, 0)"));
    ASSERT_EQ(outputs, Vector_<String_>{"payoff"});
    ASSERT_FALSE(json.empty());
    ASSERT_EQ(compatibility.Rows(), 3);
    ASSERT_EQ(reject.history_.historyCalls_, 0);
    ASSERT_EQ(reject.workers_.calls_, 0);
}

TEST(ExcelRiskTest, TestSelectionOmissionEmptyBudgetsAndNullResultErrors) {
    Handle_<StorableRiskRequest_> defaults, empty;
    RiskRequest_New("default", {}, &defaults);
    RiskRequest_New("empty", Setting("inputs", Cell_()), &empty);
    ASSERT_FALSE(defaults->val_.inputs_);
    ASSERT_TRUE(empty->val_.inputs_);
    ASSERT_TRUE(empty->val_.inputs_->empty());
    ASSERT_NO_THROW(RiskRequest_New("zero", Setting("numeric_payload_budget_bytes", Cell_(0.0)), &defaults));
    for (const double budget : {-1.0, 0.5, 9007199254740992.0, std::numeric_limits<double>::infinity()})
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { RiskRequest_New("invalid", Setting("numeric_payload_budget_bytes", Cell_(budget)), &defaults); },
                                            "numeric_payload_budget_bytes"));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { RiskRequest_New("invalid", Setting("report_factors", Cell_("abc")), &defaults); }, "report_factors"));
    ASSERT_THROW(RiskRequest_New("invalid", Setting("typo", Cell_(1.0)), &defaults), Exception_);
    Matrix_<Cell_> duplicate(2, 2);
    duplicate(0, 0) = "inputs";
    duplicate(1, 0) = "INPUTS";
    ASSERT_THROW(RiskRequest_New("invalid", duplicate, &defaults), Exception_);
    Matrix_<Cell_> output;
    ASSERT_THROW(RiskResult_Get_Values({}, &output), Exception_);
    ASSERT_THROW(RiskResult_Get_Jacobian({}, false, &output), Exception_);
    ASSERT_THROW(RiskResult_Get_Provenance({}, &output), Exception_);
}

TEST(ExcelRiskTest, TestEmptyColumnsSpillBlankAndFailureRetainsPreviousHandle) {
    Excel::ScriptTestInitialize(1);
    Handle_<ScriptProductData_> product;
    Product_New("price", {Cell_(double(Date::ToExcel(Date_(2023, 9, 25))))}, {"pay PAYS 100"}, &product);
    Handle_<ModelData_> model;
    CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.0}, {0.0}, 0.0, Matrix_<>(1, 1, 1.0), &model);
    ScriptValuationSettings_ settings;
    settings.evaluationDate_ = Date_(2022, 9, 25);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", settings));
    Handle_<StorableRiskRequest_> request;
    RiskRequest_New("empty", Setting("inputs", Cell_()), &request);
    Handle_<StorableRiskResult_> result;
    MonteCarlo_ValueWithRisk(product, model, 32, request, valuation, {}, &result);
    const auto previous = result;
    Matrix_<Cell_> jacobian, shape;
    RiskResult_Get_Jacobian(result, false, &jacobian);
    RiskResult_Get_Shape(result, &shape);
    ASSERT_EQ(jacobian.Rows(), 1);
    ASSERT_EQ(jacobian.Cols(), 1);
    ASSERT_TRUE(Cell::IsEmpty(jacobian(0, 0)));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), 0.0);
    RiskRequest_New("invalid", Setting("numeric_payload_budget_bytes", Cell_(7.0)), &request);
    RejectGetterWork_ reject;
    ASSERT_THROW(MonteCarlo_ValueWithRisk(product, model, 32, request, valuation, {}, &result), Exception_);
    ASSERT_EQ(result, previous);
    Matrix_<Cell_> values;
    RiskResult_Get_Values(result, &values);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(values(0, 0)), 100.0);
    ASSERT_EQ(reject.history_.historyCalls_, 0);
    ASSERT_EQ(reject.workers_.calls_, 0);
}

TEST(ExcelRiskTest, TestPathCountErrorsIdentifyFunctionAndFieldBeforeOtherWork) {
    Handle_<StorableRiskResult_> result;
    RejectGetterWork_ reject;
    for (const double paths : {0.0, -1.0, 1.5, double(std::numeric_limits<int>::max()) + 1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { MonteCarlo_ValueWithRisk({}, {}, paths, {}, {}, {}, &result); }, "InvalidPathCount"));
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { MonteCarlo_ValueWithRisk({}, {}, paths, {}, {}, {}, &result); }, "MonteCarlo_ValueWithRisk"));
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { MonteCarlo_ValueWithRisk({}, {}, paths, {}, {}, {}, &result); }, "n_paths"));
    }
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_EQ(reject.history_.historyCalls_, 0);
    ASSERT_EQ(reject.workers_.calls_, 0);
}
