//
// Created by Codex on 2026-10-06.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__models_test_api.hpp>
#include <dal-excel/src/__risk.hpp>
#include <dal-excel/src/__riskrequestinput.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal/storage/globals.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct WeightedFixture_ {
        Handle_<ScriptProductData_> product_;
        Handle_<ModelData_> model_;
        Handle_<StorableScriptValuationSettings_> valuation_;
        WeightedFixture_(const Vector_<Cell_>& dates, const Vector_<String_>& events) {
            Excel::ScriptTestInitialize(1);
            Product_New("weighted", dates, events, &product_);
            CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.2}, {0.0}, 0.0, Matrix_<>(1, 1, 1.0), &model_);
            ScriptValuationSettings_ settings;
            settings.evaluationDate_ = Date_(2026, 1, 1);
            valuation_.reset(new StorableScriptValuationSettings_("date", settings));
        }
    };

    Matrix_<Cell_> WeightedSetting(const String_& key, const Cell_& value) {
        Matrix_<Cell_> settings(1, 2);
        settings(0, 0) = key;
        settings(0, 1) = value;
        return settings;
    }

    struct RejectWeightedGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectWeightedGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectWeightedGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };

    template <class F_> void ExpectWeightedError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected error for " << field;
        } catch (const std::exception& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(ExcelWeightedRiskTest, TestIndependentAnalyticValueComponentsAndReportedGradient) {
    Excel::ScriptTestInitialize(1);
    Handle_<ScriptProductData_> product;
    Product_New("weighted", {Cell_("X"), Cell_("Y"), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"2", "3", "a = X * Y b = X + Y pay PAYS 5"},
                &product);
    Handle_<ModelData_> model;
    CorrelatedBSModelData_New("model", {"EQ[A]"}, {100.0}, {0.2}, {0.0}, 0.0, Matrix_<>(1, 1, 1.0), &model);
    ScriptValuationSettings_ valueSettings;
    valueSettings.evaluationDate_ = Date_(2026, 1, 1);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("date", valueSettings));
    Matrix_<Cell_> settings(5, 2);
    const Vector_<String_> keys{"outputs", "weights", "inputs", "report_factors", "numeric_payload_budget_bytes"};
    const Vector_<Cell_> values{Cell_("output:0;output:1;payoff"), Cell_("2;-1;0.5"), Cell_("constant:0;constant:1"), Cell_("0.5;2"), Cell_(72.0)};
    for (int row = 0; row < settings.Rows(); ++row) {
        settings(row, 0) = keys[row];
        settings(row, 1) = values[row];
    }
    Handle_<StorableWeightedRiskRequest_> request;
    WeightedRiskRequest_New("objective", settings, &request);
    settings.Fill(Cell_());
    for (const bool compiled : {false, true}) {
        auto simulationSettings = DefaultRiskMonteCarloSettings();
        simulationSettings.compiled_ = compiled;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("native", simulationSettings));
        Handle_<StorableWeightedRiskResult_> result;
        MonteCarlo_ValueWithWeightedRisk(product, model, 257, request, valuation, simulation, &result);
        const RejectWeightedGetterWork_ reject;
        double objective;
        WeightedRiskResult_Get_WeightedValue(result, &objective);
        ASSERT_DOUBLE_EQ(objective, 9.5);
        Matrix_<Cell_> components, raw, reported, shape;
        WeightedRiskResult_Get_Components(result, &components);
        WeightedRiskResult_Get_Jacobian(result, false, &raw);
        WeightedRiskResult_Get_Jacobian(result, true, &reported);
        WeightedRiskResult_Get_Shape(result, &shape);
        ASSERT_EQ(components.Rows(), 4);
        ASSERT_EQ(components.Cols(), 5);
        ASSERT_EQ(Cell::ToString(components(0, 0)), String_("id"));
        ASSERT_EQ(Cell::ToString(components(3, 0)), String_("payoff"));
        ASSERT_DOUBLE_EQ(Cell::ToDouble(components(1, 3)), 2.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(components(1, 4)), 6.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(components(2, 4)), 5.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(components(3, 4)), 5.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 1.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), 2.0);
        ASSERT_NEAR(Cell::ToDouble(raw(0, 0)), 5.0, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(raw(0, 1)), 3.0, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(reported(0, 0)), 2.5, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(reported(0, 1)), 6.0, 1.0e-10);
        raw(0, 0) = -999.0;
        components(1, 4) = -999.0;
        WeightedRiskResult_Get_Jacobian(result, false, &raw);
        WeightedRiskResult_Get_Components(result, &components);
        ASSERT_NEAR(Cell::ToDouble(raw(0, 0)), 5.0, 1.0e-10);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(components(1, 4)), 6.0);
    }
}

TEST(ExcelWeightedRiskTest, TestOutputQueryAndDefaultPayoffOwnIndependentTables) {
    const WeightedFixture_ fixture({Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"a = 2 pay PAYS 5"});
    Matrix_<Cell_> axis;
    {
        const RejectWeightedGetterWork_ reject;
        Product_Get_RiskOutputs(fixture.product_, &axis);
        ASSERT_EQ(axis.Rows(), 3);
        ASSERT_EQ(axis.Cols(), 3);
        ASSERT_EQ(Cell::ToString(axis(1, 0)), String_("output:0"));
        ASSERT_EQ(Cell::ToString(axis(2, 0)), String_("payoff"));
        ASSERT_EQ(Cell::ToString(axis(2, 1)), String_("pay"));
        axis(2, 0) = "changed";
        Product_Get_RiskOutputs(fixture.product_, &axis);
        ASSERT_EQ(Cell::ToString(axis(2, 0)), String_("payoff"));
        Handle_<ScriptProductData_> vectorProduct;
        Product_New("vector", {Cell_("V"), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"[0.25, 0.75]", "a = 3 pay PAYS V[0] * a"},
                    &vectorProduct);
        Product_Get_RiskOutputs(vectorProduct, &axis);
        ASSERT_EQ(axis.Rows(), 3);
        ASSERT_EQ(Cell::ToString(axis(1, 1)), String_("a"));
        ASSERT_EQ(Cell::ToString(axis(2, 0)), String_("payoff"));
    }
    Handle_<StorableWeightedRiskResult_> result;
    MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 17, {}, fixture.valuation_, {}, &result);
    double value;
    WeightedRiskResult_Get_WeightedValue(result, &value);
    ASSERT_DOUBLE_EQ(value, 5.0);
    WeightedRiskResult_Get_Components(result, &axis);
    ASSERT_EQ(axis.Rows(), 2);
    ASSERT_EQ(Cell::ToString(axis(1, 0)), String_("payoff"));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(axis(1, 3)), 1.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(axis(1, 4)), 5.0);
}

TEST(ExcelWeightedRiskTest, TestRequestParsingRejectsInvalidWeightsRowsAndBudgetsWithoutReplacingHandle) {
    Handle_<StorableWeightedRiskRequest_> request;
    WeightedRiskRequest_New("valid", WeightedSetting("weights", Cell_("-1;0;2")), &request);
    const auto original = request;
    for (const Cell_ invalid :
         {Cell_(true), Cell_(1.0), Cell_("1x"), Cell_("1;;2"), Cell_("nan"), Cell_("inf"), Cell_(String_(std::string("1\0;2", 4)))}) {
        ASSERT_NO_FATAL_FAILURE(
            ExpectWeightedError([&] { WeightedRiskRequest_New("bad", WeightedSetting("weights", invalid), &request); }, "weights"));
        ASSERT_EQ(request, original);
    }
    for (const Cell_ invalid : {Cell_(true), Cell_(-1.0), Cell_(0.5), Cell_(9007199254740992.0)}) {
        ASSERT_NO_FATAL_FAILURE(
            ExpectWeightedError([&] { WeightedRiskRequest_New("bad", WeightedSetting("numeric_payload_budget_bytes", invalid), &request); },
                                "numeric_payload_budget_bytes"));
        ASSERT_EQ(request, original);
    }
    ASSERT_NO_THROW(WeightedRiskRequest_New("budget0", WeightedSetting("numeric_payload_budget_bytes", Cell_(0.0)), &request));
    Matrix_<Cell_> duplicate(2, 2);
    duplicate(0, 0) = "weights";
    duplicate(0, 1) = "1";
    duplicate(1, 0) = "WEIGHTS";
    duplicate(1, 1) = "2";
    ASSERT_THROW(WeightedRiskRequest_New("bad", duplicate, &request), Exception_);
    ASSERT_THROW(WeightedRiskRequest_New("bad", Matrix_<Cell_>(1, 3), &request), Exception_);
    ASSERT_THROW(WeightedRiskRequest_New("bad", WeightedSetting("typo", Cell_(1.0)), &request), Exception_);
    ASSERT_THROW(WeightedRiskRequest_New(String_(std::string("a\0b", 3)), {}, &request), Exception_);
}

TEST(ExcelWeightedRiskTest, TestSelectionAndBudgetErrorsPrecedeHistoryAndRetainCompletedResult) {
    const WeightedFixture_ valid({Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"pay PAYS 5"});
    Handle_<StorableWeightedRiskResult_> result;
    MonteCarlo_ValueWithWeightedRisk(valid.product_, valid.model_, 17, {}, valid.valuation_, {}, &result);
    const auto original = result;
    const WeightedFixture_ missing({Cell_(double(Date::ToExcel(Date_(2025, 1, 1)))), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))},
                                   {"a = FIX(EQ[EXCEL_WEIGHTED_MISSING])", "pay PAYS a"});
    const RejectWeightedGetterWork_ reject;
    for (const auto& option :
         Vector_<std::pair<Matrix_<Cell_>, String_>>{{WeightedSetting("outputs", Cell_()), "nonempty"},
                                                     {WeightedSetting("outputs", Cell_("payoff;PAYOFF")), "repeated output"},
                                                     {WeightedSetting("outputs", Cell_("unknown")), "unknown scalar output"},
                                                     {WeightedSetting("weights", Cell_("1;2")), "weight count"},
                                                     {WeightedSetting("inputs", Cell_("weight:0")), "unknown input"},
                                                     {WeightedSetting("numeric_payload_budget_bytes", Cell_(0.0)), "RiskResultBudgetExceeded"}}) {
        Handle_<StorableWeightedRiskRequest_> request;
        WeightedRiskRequest_New("invalid", option.first, &request);
        ASSERT_NO_FATAL_FAILURE(ExpectWeightedError(
            [&] { MonteCarlo_ValueWithWeightedRisk(missing.product_, missing.model_, 257, request, missing.valuation_, {}, &result); },
            option.second.c_str()));
        ASSERT_EQ(result, original);
    }
    double value;
    WeightedRiskResult_Get_WeightedValue(result, &value);
    ASSERT_DOUBLE_EQ(value, 5.0);
}

TEST(ExcelWeightedRiskTest, TestNativeEmptyAndPriceOnlySpillBlankButKeepDistinctEstimators) {
    const WeightedFixture_ fixture({Cell_(double(Date::ToExcel(Date_(2026, 1, 1))))}, {"IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"});
    Handle_<StorableWeightedRiskRequest_> request;
    Matrix_<Cell_> settings(2, 2);
    settings(0, 0) = "inputs";
    settings(1, 0) = "numeric_payload_budget_bytes";
    settings(1, 1) = 24.0;
    WeightedRiskRequest_New("empty", settings, &request);
    for (const bool compiled : {false, true}) {
        for (const bool aad : {false, true}) {
            auto execution = DefaultRiskMonteCarloSettings();
            execution.compiled_ = compiled;
            execution.enableAad_ = aad;
            const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("mode", execution));
            Handle_<StorableWeightedRiskResult_> result;
            MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 17, request, fixture.valuation_, simulation, &result);
            const RejectWeightedGetterWork_ reject;
            double value;
            Matrix_<Cell_> raw, shape, provenance;
            WeightedRiskResult_Get_WeightedValue(result, &value);
            ASSERT_NEAR(value, aad ? 0.5 : 0.0, 1.0e-10);
            WeightedRiskResult_Get_Jacobian(result, false, &raw);
            WeightedRiskResult_Get_Shape(result, &shape);
            WeightedRiskResult_Get_Provenance(result, &provenance);
            ASSERT_EQ(raw.Rows(), 1);
            ASSERT_EQ(raw.Cols(), 1);
            ASSERT_TRUE(Cell::IsEmpty(raw(0, 0)));
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 1.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), 0.0);
            ASSERT_EQ(Cell::ToString(provenance(0, 1)), String_(aad ? "NativeAAD" : "PriceOnly"));
        }
    }
}

TEST(ExcelWeightedRiskTest, TestHistoricalComponentsAndSnapshotsRemainFrozenAfterCallersDie) {
    Excel::ScriptTestInitialize(1);
    const String_ index("EQ[EXCEL_WEIGHTED_HISTORY]");
    struct Cleanup_ {
        const String_ index_;
        ~Cleanup_() { Excel::ScriptTestStoreFixings(index_, {}); }
    } cleanup{index};
    FixHistory_ history;
    history.vals_ = {{DateTime_(Date_(2025, 1, 1), 0.0), 80.0}};
    Excel::ScriptTestStoreFixings(index, history);
    WeightedFixture_ fixture({Cell_("SCALE"), Cell_(double(Date::ToExcel(Date_(2025, 1, 1)))), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))},
                             {"2", "a = SCALE * FIX(EQ[EXCEL_WEIGHTED_HISTORY])", "pay PAYS a"});
    Matrix_<Cell_> settings(3, 2);
    settings(0, 0) = "outputs";
    settings(0, 1) = "output:0;payoff";
    settings(1, 0) = "weights";
    settings(1, 1) = "2;-1";
    settings(2, 0) = "inputs";
    settings(2, 1) = "constant:0";
    Handle_<StorableWeightedRiskRequest_> request;
    WeightedRiskRequest_New("history", settings, &request);
    Handle_<StorableWeightedRiskResult_> result;
    MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 257, request, fixture.valuation_, {}, &result);
    history.vals_[0].second = 90.0;
    Excel::ScriptTestStoreFixings(index, history);
    fixture.product_.reset();
    fixture.model_.reset();
    fixture.valuation_.reset();
    request.reset();
    settings.Fill(Cell_());
    const RejectWeightedGetterWork_ reject;
    Matrix_<Cell_> components, raw, provenance, observations, product, inputs;
    Vector_<String_> model;
    double value;
    WeightedRiskResult_Get_WeightedValue(result, &value);
    WeightedRiskResult_Get_Components(result, &components);
    WeightedRiskResult_Get_Jacobian(result, false, &raw);
    WeightedRiskResult_Get_Provenance(result, &provenance);
    WeightedRiskResult_Get_History(result, &observations);
    WeightedRiskResult_Get_Product(result, &product);
    WeightedRiskResult_Get_ModelSnapshot(result, &model);
    WeightedRiskResult_Get_Inputs(result, false, &inputs);
    ASSERT_DOUBLE_EQ(value, 160.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(components(1, 4)), 160.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(components(2, 4)), 160.0);
    ASSERT_NEAR(Cell::ToDouble(raw(0, 0)), 80.0, 1.0e-10);
    ASSERT_EQ(Cell::ToString(provenance(0, 1)), String_("NativeAAD"));
    ASSERT_EQ(observations.Rows(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(observations(1, 3)), 80.0);
    ASSERT_EQ(Cell::ToString(product(2, 1)), String_("pay PAYS a"));
    ASSERT_EQ(Cell::ToString(inputs(1, 0)), String_("constant:0"));
    ASSERT_FALSE(model.empty());
    WeightedRiskResult_Get_Inputs(result, true, &inputs);
    ASSERT_GT(inputs.Rows(), 2);
}

TEST(ExcelWeightedRiskTest, TestNonfiniteZeroWeightFailurePreservesResultAndNextCallRecovers) {
    const WeightedFixture_ good({Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"a = 100 pay PAYS 1"});
    const WeightedFixture_ bad({Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"a = EXP(1000) pay PAYS 1"});
    Matrix_<Cell_> settings(2, 2);
    settings(0, 0) = "outputs";
    settings(0, 1) = "output:0;payoff";
    settings(1, 0) = "weights";
    settings(1, 1) = "0;1";
    Handle_<StorableWeightedRiskRequest_> request;
    WeightedRiskRequest_New("zero", settings, &request);
    for (const bool aad : {false, true}) {
        auto execution = DefaultRiskMonteCarloSettings();
        execution.enableAad_ = aad;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("mode", execution));
        Handle_<StorableWeightedRiskResult_> result;
        MonteCarlo_ValueWithWeightedRisk(good.product_, good.model_, 257, request, good.valuation_, simulation, &result);
        const auto original = result;
        ASSERT_NO_FATAL_FAILURE(ExpectWeightedError(
            [&] { MonteCarlo_ValueWithWeightedRisk(bad.product_, bad.model_, 257, request, bad.valuation_, simulation, &result); }, "finite"));
        ASSERT_EQ(result, original);
        MonteCarlo_ValueWithWeightedRisk(good.product_, good.model_, 257, request, good.valuation_, simulation, &result);
        double value;
        WeightedRiskResult_Get_WeightedValue(result, &value);
        ASSERT_DOUBLE_EQ(value, 1.0);
    }
}

TEST(ExcelWeightedRiskTest, TestHistoricalAliasesSignedWeightsInputPermutationAndExactBudget) {
    const WeightedFixture_ fixture(
        {Cell_("X"), Cell_("Y"), Cell_(double(Date::ToExcel(Date_(2025, 1, 1)))), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))},
        {"2", "3", "a = X * Y b = a direct = X literal = 5", "pay PAYS 0"});
    struct Reference_ {
        String_ weights_;
        double value_;
        double first_;
        double second_;
    };
    Matrix_<Cell_> settings(4, 2);
    settings(0, 0) = "outputs";
    settings(0, 1) = "output:0;output:1;output:2;output:3";
    settings(1, 0) = "weights";
    settings(2, 0) = "inputs";
    settings(2, 1) = "constant:1;constant:0";
    settings(3, 0) = "numeric_payload_budget_bytes";
    settings(3, 1) = 88.0;
    for (const bool compiled : {false, true}) {
        auto execution = DefaultRiskMonteCarloSettings();
        execution.compiled_ = compiled;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("native", execution));
        for (const auto& reference : Vector_<Reference_>{
                 {"2;3;0;0", 30.0, 10.0, 15.0}, {"-1;-1;0;0", -12.0, -4.0, -6.0}, {"0;0;4;-2", -2.0, 0.0, 4.0}, {"0;0;0;0", 0.0, 0.0, 0.0}}) {
            settings(1, 1) = reference.weights_;
            Handle_<StorableWeightedRiskRequest_> request;
            WeightedRiskRequest_New("aliases", settings, &request);
            Handle_<StorableWeightedRiskResult_> result;
            MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 257, request, fixture.valuation_, simulation, &result);
            double value;
            Matrix_<Cell_> raw, components, inputs;
            WeightedRiskResult_Get_WeightedValue(result, &value);
            WeightedRiskResult_Get_Jacobian(result, false, &raw);
            WeightedRiskResult_Get_Components(result, &components);
            WeightedRiskResult_Get_Inputs(result, false, &inputs);
            ASSERT_DOUBLE_EQ(value, reference.value_);
            ASSERT_NEAR(Cell::ToDouble(raw(0, 0)), reference.first_, 1.0e-10);
            ASSERT_NEAR(Cell::ToDouble(raw(0, 1)), reference.second_, 1.0e-10);
            ASSERT_EQ(Cell::ToString(inputs(1, 0)), String_("constant:1"));
            ASSERT_EQ(Cell::ToString(inputs(2, 0)), String_("constant:0"));
            ASSERT_DOUBLE_EQ(Cell::ToDouble(components(1, 4)), 6.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(components(2, 4)), 6.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(components(3, 4)), 2.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(components(4, 4)), 5.0);
        }
    }
    settings(3, 1) = 87.0;
    Handle_<StorableWeightedRiskRequest_> request;
    WeightedRiskRequest_New("short", settings, &request);
    Handle_<StorableWeightedRiskResult_> result;
    ASSERT_NO_FATAL_FAILURE(ExpectWeightedError(
        [&] { MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 257, request, fixture.valuation_, {}, &result); },
        "RiskResultBudgetExceeded"));
    ASSERT_FALSE(result);
}

TEST(ExcelWeightedRiskTest, TestUnsupportedExerciseAndExpiredProductsRejectWithoutWork) {
    const WeightedFixture_ exercise({Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))}, {"EXERCISE MAX(100 - SPOT(), 0)"});
    const WeightedFixture_ expired({Cell_(double(Date::ToExcel(Date_(2025, 1, 1))))}, {"pay PAYS 1"});
    const RejectWeightedGetterWork_ reject;
    for (const auto& fixture : {exercise, expired}) {
        Handle_<StorableWeightedRiskResult_> result;
        ASSERT_NO_FATAL_FAILURE(
            ExpectWeightedError([&] { MonteCarlo_ValueWithWeightedRisk(fixture.product_, fixture.model_, 257, {}, fixture.valuation_, {}, &result); },
                                "UnsupportedWeightedRisk"));
        ASSERT_FALSE(result);
    }
}

TEST(ExcelWeightedRiskTest, TestPathsAndNullGettersIdentifyFailureBeforeReplacingOutput) {
    const RejectWeightedGetterWork_ reject;
    Handle_<StorableWeightedRiskResult_> result;
    for (const double count : {0.0, 0.5, -1.0, std::numeric_limits<double>::infinity(), double((std::numeric_limits<int>::max)()) + 1.0}) {
        ASSERT_NO_FATAL_FAILURE(
            ExpectWeightedError([&] { MonteCarlo_ValueWithWeightedRisk({}, {}, count, {}, {}, {}, &result); }, "InvalidPathCount"));
        ASSERT_FALSE(result);
    }
    Matrix_<Cell_> cells;
    Vector_<String_> json;
    double value;
    ASSERT_THROW(Product_Get_RiskOutputs({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_WeightedValue({}, &value), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Components({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Jacobian({}, false, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Shape({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Inputs({}, false, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Provenance({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_History({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_Product({}, &cells), Exception_);
    ASSERT_THROW(WeightedRiskResult_Get_ModelSnapshot({}, &json), Exception_);
}

#ifdef _WIN32
TEST(ExcelWeightedRiskTest, TestRawSettingsFlagsAndPathsRejectCoercionAndMalformedRanges) {
    wchar_t key[]{7, L'w', L'e', L'i', L'g', L'h', L't', L's'};
    wchar_t value[]{2, L'-', L'1'};
    OPER_ cells[2]{};
    cells[0].xltype = xltypeStr;
    cells[0].val.str = key;
    cells[1].xltype = xltypeStr;
    cells[1].val.str = value;
    OPER_ input{};
    input.xltype = xltypeMulti;
    input.val.array = {cells, 1, 2};
    ASSERT_NO_THROW(Excel::ValidateRiskRequestSettings(&input, "WeightedRiskRequest_New"));
    value[1] = L'\0';
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "WeightedRiskRequest_New"), Exception_);
    cells[1].xltype = xltypeErr;
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "WeightedRiskRequest_New"), Exception_);
    input.val.array = {nullptr, (std::numeric_limits<int>::max)(), 3};
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "WeightedRiskRequest_New"), Exception_);
    OPER_ flag{};
    flag.xltype = xltypeBool;
    flag.val.xbool = 1;
    ASSERT_NO_THROW(Excel::ValidateRiskRequestBoolean(&flag, "WeightedRiskResult_Get_Jacobian; reported"));
    ASSERT_THROW(Excel::ValidateRiskRequestPaths(&flag, "MonteCarlo_ValueWithWeightedRisk"), Exception_);
    flag.xltype = xltypeNum;
    flag.val.num = 1.0;
    ASSERT_THROW(Excel::ValidateRiskRequestBoolean(&flag, "WeightedRiskResult_Get_Jacobian; reported"), Exception_);
    ASSERT_NO_THROW(Excel::ValidateRiskRequestPaths(&flag, "MonteCarlo_ValueWithWeightedRisk"));
    flag.val.num = 0.5;
    ASSERT_THROW(Excel::ValidateRiskRequestPaths(&flag, "MonteCarlo_ValueWithWeightedRisk"), Exception_);
}
#endif
