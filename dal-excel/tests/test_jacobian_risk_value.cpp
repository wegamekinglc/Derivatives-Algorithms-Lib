//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__models_test_api.hpp>
#include <dal-excel/src/__risk.hpp>
#include <dal-excel/src/__script_test_api.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct JacobianFixture_ {
        Handle_<ScriptProductData_> product_;
        Handle_<ModelData_> model_;
        Handle_<StorableScriptValuationSettings_> valuation_;
        explicit JacobianFixture_(const String_& event = "pay PAYS 5") {
            Excel::ScriptTestInitialize(1);
            Product_New("jacobian", {Cell_(double(Date::ToExcel(Date_(2026, 1, 1))))}, {event}, &product_);
            CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.0}, {0.0}, 0.0, Matrix_<>(1, 1, 1.0), &model_);
            ScriptValuationSettings_ settings;
            settings.evaluationDate_ = Date_(2026, 1, 1);
            valuation_.reset(new StorableScriptValuationSettings_("date", settings));
        }
    };

    Matrix_<Cell_> JacobianSetting(const String_& key, const Cell_& value) {
        Matrix_<Cell_> settings(1, 2);
        settings(0, 0) = key;
        settings(0, 1) = value;
        return settings;
    }

    struct RejectJacobianGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectJacobianGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectJacobianGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };
} // namespace

TEST(ExcelJacobianRiskTest, TestOrderedAnalyticMatrixAndDetachedTables) {
    Excel::ScriptTestInitialize(1);
    Handle_<ScriptProductData_> product;
    Product_New("jacobian", {Cell_("X"), Cell_("Y"), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))},
                {"2", "3", "a = X * Y b = X + Y alias = a pay PAYS 5"}, &product);
    Handle_<ModelData_> model;
    CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.2}, {0.0}, 0.0, Matrix_<>(1, 1, 1.0), &model);
    ScriptValuationSettings_ settings;
    settings.evaluationDate_ = Date_(2026, 1, 1);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("date", settings));
    Matrix_<Cell_> requestSettings(5, 2);
    const Vector_<String_> keys{"outputs", "inputs", "report_factors", "max_block_width", "numeric_payload_budget_bytes"};
    const Vector_<Cell_> cells{Cell_("output:2;output:1;payoff;output:0"), Cell_("constant:1;constant:0"), Cell_("0.5;2"), Cell_(3.0), Cell_(96.0)};
    for (int row = 0; row < requestSettings.Rows(); ++row) {
        requestSettings(row, 0) = keys[row];
        requestSettings(row, 1) = cells[row];
    }
    Handle_<StorableJacobianRiskRequest_> request;
    JacobianRiskRequest_New("request", requestSettings, &request);
    requestSettings.Fill(Cell_());
    for (const bool compiled : {false, true}) {
        auto execution = DefaultRiskMonteCarloSettings();
        execution.compiled_ = compiled;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("native", execution));
        Handle_<StorableJacobianRiskResult_> result;
        MonteCarlo_ValueWithJacobianRisk(product, model, 17, request, valuation, simulation, &result);
        const RejectJacobianGetterWork_ reject;
        Matrix_<Cell_> values, raw, reported, shape, outputs, inputs, diagnostics;
        JacobianRiskResult_Get_Values(result, &values);
        JacobianRiskResult_Get_Jacobian(result, false, &raw);
        JacobianRiskResult_Get_Jacobian(result, true, &reported);
        JacobianRiskResult_Get_Shape(result, &shape);
        JacobianRiskResult_Get_Outputs(result, false, &outputs);
        JacobianRiskResult_Get_Inputs(result, false, &inputs);
        JacobianRiskResult_Get_Execution(result, &diagnostics);
        ASSERT_EQ(values.Rows(), 5);
        ASSERT_EQ(values.Cols(), 4);
        ASSERT_EQ(Cell::ToString(values(1, 0)), "output:2");
        ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 6.0);
        ASSERT_EQ(raw.Rows(), 4);
        ASSERT_EQ(raw.Cols(), 2);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 0)), 2.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 1)), 3.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(2, 0)), 0.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(0, 0)), 1.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(0, 1)), 6.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 4.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), 2.0);
        ASSERT_EQ(Cell::ToString(inputs(1, 0)), "constant:1");
        ASSERT_EQ(Cell::ToString(outputs(1, 0)), "output:2");
        ASSERT_EQ(diagnostics.Cols(), 2);
        ASSERT_EQ(Cell::ToString(diagnostics(0, 1)), "3;3");
        ASSERT_DOUBLE_EQ(Cell::ToDouble(diagnostics(1, 1)), 2.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(diagnostics(2, 1)), 34.0);
        raw.Fill(Cell_(-999.0));
        values.Fill(Cell_(-999.0));
        JacobianRiskResult_Get_Jacobian(result, false, &raw);
        JacobianRiskResult_Get_Values(result, &values);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 0)), 2.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 6.0);
    }
}

TEST(ExcelJacobianRiskTest, TestDefaultPayoffSnapshotsAndGettersPerformNoWork) {
    const JacobianFixture_ fixture;
    Handle_<StorableJacobianRiskResult_> result;
    MonteCarlo_ValueWithJacobianRisk(fixture.product_, fixture.model_, 17, {}, fixture.valuation_, {}, &result);
    const RejectJacobianGetterWork_ reject;
    Matrix_<Cell_> values, outputs, inputs, provenance, product, history;
    Vector_<String_> json;
    JacobianRiskResult_Get_Values(result, &values);
    JacobianRiskResult_Get_Outputs(result, true, &outputs);
    JacobianRiskResult_Get_Inputs(result, true, &inputs);
    JacobianRiskResult_Get_Provenance(result, &provenance);
    JacobianRiskResult_Get_Product(result, &product);
    JacobianRiskResult_Get_History(result, &history);
    JacobianRiskResult_Get_ModelSnapshot(result, &json);
    ASSERT_EQ(values.Rows(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 5.0);
    ASSERT_EQ(Cell::ToString(outputs(1, 0)), "payoff");
    ASSERT_EQ(inputs.Rows(), 5);
    ASSERT_EQ(provenance.Cols(), 2);
    ASSERT_EQ(product.Rows(), 1);
    ASSERT_EQ(history.Cols(), 4);
    ASSERT_FALSE(json.empty());
    ASSERT_EQ(reject.history_.historyCalls_, 0);
    ASSERT_EQ(reject.history_.fixingCalls_, 0);
    ASSERT_EQ(reject.workers_.calls_, 0);
}

TEST(ExcelJacobianRiskTest, TestEmptyColumnsSpillBlankAndPreserveNativeEstimator) {
    const JacobianFixture_ fixture("IF SPOT() > 100 THEN a = 1 ELSE a = 0 END b = a pay PAYS 0");
    Matrix_<Cell_> settings(2, 2);
    settings(0, 0) = "outputs";
    settings(0, 1) = "output:1;output:0";
    settings(1, 0) = "inputs";
    Handle_<StorableJacobianRiskRequest_> request;
    JacobianRiskRequest_New("empty", settings, &request);
    for (const bool native : {false, true}) {
        auto execution = DefaultRiskMonteCarloSettings();
        execution.enableAad_ = native;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("simulation", execution));
        Handle_<StorableJacobianRiskResult_> result;
        MonteCarlo_ValueWithJacobianRisk(fixture.product_, fixture.model_, 17, request, fixture.valuation_, simulation, &result);
        Matrix_<Cell_> values, raw, shape;
        JacobianRiskResult_Get_Values(result, &values);
        JacobianRiskResult_Get_Jacobian(result, false, &raw);
        JacobianRiskResult_Get_Shape(result, &shape);
        ASSERT_EQ(values.Rows(), 3);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), native ? 0.5 : 0.0);
        ASSERT_EQ(raw.Rows(), 1);
        ASSERT_EQ(raw.Cols(), 1);
        ASSERT_TRUE(Cell::IsEmpty(raw(0, 0)));
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 2.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), 0.0);
    }
}

TEST(ExcelJacobianRiskTest, TestStrictBudgetsWidthsAndRowsPreservePreviousRequest) {
    Handle_<StorableJacobianRiskRequest_> request;
    JacobianRiskRequest_New("valid", {}, &request);
    const auto original = request;
    for (const auto& key :
         Vector_<String_>{"numeric_payload_budget_bytes", "recording_capacity_budget_bytes", "scratch_capacity_budget_bytes", "max_block_width"}) {
        for (const auto& invalid : Vector_<Cell_>{Cell_(true), Cell_("1"), Cell_(1.5), Cell_(-1.0), Cell_(9007199254740992.0),
                                                  Cell_(std::numeric_limits<double>::infinity())}) {
            ASSERT_THROW(JacobianRiskRequest_New("bad", JacobianSetting(key, invalid), &request), Exception_);
            ASSERT_EQ(request.get(), original.get());
        }
    }
    ASSERT_THROW(JacobianRiskRequest_New("bad", JacobianSetting("max_block_width", Cell_(0.0)), &request), Exception_);
    ASSERT_THROW(JacobianRiskRequest_New("bad", JacobianSetting("max_block_width", Cell_(double(AAD::ADJ_SIZE) + 1.0)), &request), Exception_);
    ASSERT_THROW(JacobianRiskRequest_New("bad", JacobianSetting("typo", Cell_(1.0)), &request), Exception_);
    ASSERT_THROW(JacobianRiskRequest_New("bad", Matrix_<Cell_>(1, 3), &request), Exception_);
    Matrix_<Cell_> duplicate(2, 2);
    duplicate(0, 0) = duplicate(1, 0) = "max_block_width";
    duplicate(0, 1) = duplicate(1, 1) = 1.0;
    ASSERT_THROW(JacobianRiskRequest_New("bad", duplicate, &request), Exception_);
    ASSERT_EQ(request.get(), original.get());
}

TEST(ExcelJacobianRiskTest, TestFailedValuationAndNullGettersPreserveCompletedResult) {
    const JacobianFixture_ fixture;
    Handle_<StorableJacobianRiskResult_> result;
    MonteCarlo_ValueWithJacobianRisk(fixture.product_, fixture.model_, 17, {}, fixture.valuation_, {}, &result);
    const auto original = result;
    Handle_<StorableJacobianRiskRequest_> invalid;
    JacobianRiskRequest_New("invalid", JacobianSetting("scratch_capacity_budget_bytes", Cell_(0.0)), &invalid);
    const RejectJacobianGetterWork_ reject;
    ASSERT_THROW(MonteCarlo_ValueWithJacobianRisk(fixture.product_, fixture.model_, 17, invalid, fixture.valuation_, {}, &result), Exception_);
    ASSERT_EQ(result.get(), original.get());
    for (const double paths : {0.0, 0.5, -1.0, double((std::numeric_limits<int>::max)()) + 1.0}) {
        ASSERT_THROW(MonteCarlo_ValueWithJacobianRisk({}, {}, paths, {}, {}, {}, &result), Exception_);
        ASSERT_EQ(result.get(), original.get());
    }
    Matrix_<Cell_> cells;
    Vector_<String_> json;
    ASSERT_THROW(JacobianRiskResult_Get_Values({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Jacobian({}, false, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Shape({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Inputs({}, false, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Outputs({}, false, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Execution({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Provenance({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_History({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_Product({}, &cells), Exception_);
    ASSERT_THROW(JacobianRiskResult_Get_ModelSnapshot({}, &json), Exception_);
    ASSERT_EQ(reject.workers_.calls_, 0);
}
