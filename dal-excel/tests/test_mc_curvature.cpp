//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include <dal-excel/src/__mccurvature.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/script.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    void InitializeRuntime() {
        RegisterAll_::Init();
#ifdef _WIN32
        Excel::ScriptTestInitialize(1);
#endif
    }

    Matrix_<Cell_> Cells(std::initializer_list<std::pair<String_, Cell_>> fields) {
        Matrix_<Cell_> cells(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& field : fields) {
            cells(row, 0) = field.first;
            cells(row++, 1) = field.second;
        }
        return cells;
    }

    Matrix_<Cell_> Point(const Vector_<>& values = {10, 0.2, 0.03, 0.01, 2}, bool column = false) {
        Matrix_<Cell_> cells(column ? static_cast<int>(values.size()) : 1, column ? 1 : static_cast<int>(values.size()));
        for (int index = 0; index < static_cast<int>(values.size()); ++index)
            cells(column ? index : 0, column ? 0 : index) = values[index];
        return cells;
    }

    auto Valuation() {
        Script::ScriptValuationSettings_ value;
        value.evaluationDate_ = Date_(2026, 1, 2);
        return value;
    }

    auto Plan(bool today = true) {
        InitializeRuntime();
        const auto product =
            NewScriptProduct("sealed", {Cell_("SCALE"), Cell_(Date_(today ? 2026 : 2027, 1, 2))},
                             {"2", today ? "pay PAYS SCALE * FIX(EQ[EXCEL_SEGMENTED]) ^ 2" : "pay PAYS SCALE * FIX(EQ[EXCEL_SEGMENTED])"});
        const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", Valuation()));
        Handle_<StorableBlackScholesMonteCarloPlan_> plan;
        BlackScholesMonteCarloPlan_New("plan", product, valuation, 0.01, &plan);
        return plan;
    }

    auto Bumps(int rows = 2) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(rows, 5, 0.0);
        if (rows > 0) {
            request.directions_(0, 0) = 1.0;
            request.directions_(0, 4) = 0.5;
        }
        if (rows > 1) {
            request.directions_(1, 0) = -1.0;
            request.directions_(1, 4) = 0.25;
        }
        request.steps_ = Vector_<>(rows, 0.01);
        return Handle_<StorableBumpOverAADRequest_>(new StorableBumpOverAADRequest_("bumps", request));
    }

    auto Curvature(const Handle_<StorableBumpOverAADRequest_>& bumps = Bumps()) {
        Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result;
        BlackScholesMonteCarloCurvatureResult_New("result", Plan(), Point(), 1, bumps, {}, &result);
        return result;
    }

    const Cell_& Field(const Matrix_<Cell_>& cells, const char* name) {
        for (int row = 0; row < cells.Rows(); ++row)
            if (Cell::ToString(cells(row, 0)) == name)
                return cells(row, 1);
        THROW(String_("Missing field ") + name);
    }

    template <class F_> void CheckError(F_&& function, std::initializer_list<const char*> fields) {
        try {
            function();
            FAIL() << "Expected input rejection";
        } catch (const Exception_& error) {
            for (const auto* field : fields)
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }

    struct ObserveWork_ {
#ifdef _WIN32
        Detail::FixingReadObserver_*& history_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_*& workers_ = Excel::ScriptTestSimulationObserver();
#else
        Detail::FixingReadObserver_*& history_ = Detail::FixingReadObserver();
        Script::Detail::SimulationObserver_*& workers_ = Script::Detail::SimulationObserver();
#endif
        Detail::FixingReadObserver_* previousHistory_ = history_;
        Script::Detail::SimulationObserver_* previousWorkers_ = workers_;
        ObserveWork_(Detail::FixingReadObserver_* history, Script::Detail::SimulationObserver_* workers) {
            history_ = history;
            workers_ = workers;
        }
        ~ObserveWork_() {
            history_ = previousHistory_;
            workers_ = previousWorkers_;
        }
    };
} // namespace

TEST(ExcelMonteCarloCurvatureTest, TestIndependentTimeZeroMixedProductsAndStoredBase) {
    InitializeRuntime();
    const auto product =
        NewScriptProduct("today", {Cell_("SCALE"), Cell_(Date_(2026, 1, 2))}, {"2", "pay PAYS SCALE * FIX(EQ[EXCEL_SEGMENTED]) ^ 2"});
    Script::ScriptValuationSettings_ value;
    value.evaluationDate_ = Date_(2026, 1, 2);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", value));
    Handle_<StorableBlackScholesMonteCarloPlan_> plan;
    BlackScholesMonteCarloPlan_New("plan", product, valuation, 0.01, &plan);
    Matrix_<Cell_> point(1, 5);
    const Vector_<> numbers{10.0, 0.2, 0.03, 0.01, 2.0};
    for (int column = 0; column < 5; ++column)
        point(0, column) = numbers[column];
    AAD::BumpOverAADRequest_ request;
    request.directions_ = Matrix_<>(1, 5, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(0, 4) = 0.5;
    request.steps_ = {0.01};
    const Handle_<StorableBumpOverAADRequest_> bumps(new StorableBumpOverAADRequest_("bumps", request));
    Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result;
    BlackScholesMonteCarloCurvatureResult_New("result", plan, point, 1.0, bumps, {}, &result);
    ASSERT_NEAR(result->val_.Curvature().Base().MeanValue(), 200.0, 1.0e-10);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, 0), 14.0, 1.0e-9);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, 4), 20.0, 1.0e-9);
    Handle_<StorableBlackScholesMonteCarloResult_> base;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &base);
    double price = 0.0;
    BlackScholesMonteCarloResult_Get_Value(base, &price);
    ASSERT_NEAR(price, 200.0, 1.0e-10);
    ASSERT_EQ(base->val_.point_, numbers);
    ASSERT_EQ(result->val_.Curvature().Execution().gradientEvaluations_, 3U);
}

TEST(ExcelMonteCarloCurvatureTest, TestCompleteSettingsAndExactFullWidthScrambleRoundTrip) {
    auto cells = Cells({{"rsg", Cell_("sobol")},
                        {"use_bb", Cell_(true)},
                        {"first_path", Cell_(7.0)},
                        {"scramble_key", Cell_("18446744073709551615")},
                        {"normal_precision", Cell_("Precise")},
                        {"segment_steps", Cell_(1.0)},
                        {"checkpoint_capacity_budget_bytes", Cell_(0.0)},
                        {"recording_capacity_budget_bytes", Cell_()}});
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("settings", cells, &settings);
    cells(2, 1) = 99.0;
    ASSERT_EQ(settings->val_.firstPath_, 7U);
    ASSERT_EQ(settings->val_.scrambleKey_, (std::numeric_limits<std::uint64_t>::max)());
    ASSERT_TRUE(settings->val_.useBb_);
    ASSERT_EQ(settings->val_.path_.checkpointCapacityBudgetBytes_, 0U);
    ASSERT_FALSE(settings->val_.path_.recordingCapacityBudgetBytes_);
    Matrix_<Cell_> copy;
    SegmentedMonteCarloSettings_Get_Configuration(settings, &copy);
    ASSERT_EQ(copy.Rows(), 8);
    ASSERT_EQ(Cell::ToString(Field(copy, "scramble_key")), "18446744073709551615");
    ASSERT_EQ(Cell::ToDouble(Field(copy, "first_path")), 7.0);
    copy(2, 1) = 123.0;
    ASSERT_EQ(settings->val_.firstPath_, 7U);
    SegmentedMonteCarloSettings_New("numeric-key", Cells({{"scramble_key", Cell_(9007199254740991.0)}}), &settings);
    SegmentedMonteCarloSettings_Get_Configuration(settings, &copy);
    ASSERT_EQ(Cell::ToString(Field(copy, "scramble_key")), "9007199254740991");
    SegmentedMonteCarloSettings_New("defaults", {}, &settings);
    ASSERT_EQ(settings->val_.path_.segmentSteps_, 64U);
    ASSERT_FALSE(settings->val_.scrambleKey_);
    ASSERT_EQ(settings->val_.normalPrecision_, "Default");
    SegmentedMonteCarloSettings_New("full-key", Cells({{"scramble_key", Cell_("18446744073709551615")}}), &settings);
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloResult_New("mean", Plan(), Point(), 1, settings, &mean);
    BlackScholesMonteCarloResult_Get_Execution(mean, &copy);
    ASSERT_EQ(Cell::ToString(Field(copy, "scramble_key")), "18446744073709551615");
}

TEST(ExcelMonteCarloCurvatureTest, TestFirstOrderIndependentFutureValueAndPartialBatch) {
    const auto plan = Plan(false);
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("settings", Cells({{"first_path", Cell_(7.0)}, {"segment_steps", Cell_(1.0)}}), &settings);
    const auto point = Point({100, 0, 0.03, 0.01, 2}, true);
    for (const double paths : {1.0, 33.0}) {
        Handle_<StorableBlackScholesMonteCarloResult_> result;
        BlackScholesMonteCarloResult_New("mean", plan, point, paths, settings, &result);
        const double expected = 200.0 * std::exp(-0.01);
        double value = 0;
        BlackScholesMonteCarloResult_Get_Value(result, &value);
        ASSERT_NEAR(value, expected, 1.0e-10);
        Matrix_<Cell_> cells;
        BlackScholesMonteCarloResult_Get_Gradient(result, &cells);
        ASSERT_NEAR(Cell::ToDouble(cells(0, 0)), expected / 100.0, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(cells(2, 0)), 0.0, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(cells(3, 0)), -expected, 1.0e-10);
        ASSERT_NEAR(Cell::ToDouble(cells(4, 0)), expected / 2.0, 1.0e-10);
        BlackScholesMonteCarloResult_Get_Execution(result, &cells);
        ASSERT_EQ(cells.Rows(), 13);
        ASSERT_EQ(Cell::ToDouble(Field(cells, "path_count")), paths);
        ASSERT_EQ(Cell::ToDouble(Field(cells, "first_path")), 7.0);
        ASSERT_EQ(Cell::ToDouble(Field(cells, "batches")), paths == 1 ? 1 : 2);
        ASSERT_EQ(Cell::ToDouble(Field(cells, "batch_size")), 32.0);
        ASSERT_EQ(Cell::ToDouble(Field(cells, "segment_steps")), 1.0);
    }
}

TEST(ExcelMonteCarloCurvatureTest, TestDetachedCompletePlanMeanAndCurvatureQueriesWithoutWork) {
    const auto result = Curvature();
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ work;
    const ObserveWork_ observer(&history, &work);
    Handle_<StorableBlackScholesMonteCarloResult_> base;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &base);
    Handle_<StorableBlackScholesMonteCarloPlan_> plan, meanPlan;
    BlackScholesMonteCarloCurvatureResult_Get_Plan(result, &plan);
    BlackScholesMonteCarloResult_Get_Plan(base, &meanPlan);
    Handle_<ScriptProductData_> product;
    BlackScholesMonteCarloPlan_Get_Product(plan, &product);
    ASSERT_NE(product.get(), &plan->val_.Contract());
    ASSERT_EQ(product->EventTexts(), meanPlan->val_.Contract().EventTexts());
    Handle_<StorableScriptValuationSettings_> valuation;
    BlackScholesMonteCarloPlan_Get_Valuation(plan, &valuation);
    ASSERT_EQ(valuation->val_.evaluationDate_, Date_(2026, 1, 2));
    double smoothing = 0;
    BlackScholesMonteCarloPlan_Get_Smoothing(plan, &smoothing);
    ASSERT_EQ(smoothing, 0.01);
    Vector_<String_> labels;
    BlackScholesMonteCarloPlan_Get_ParameterLabels(plan, &labels);
    ASSERT_EQ(labels, (Vector_<String_>{"spot", "vol", "rate", "div", "SCALE"}));
    labels[0] = "changed";
    BlackScholesMonteCarloResult_Get_ParameterLabels(base, &labels);
    ASSERT_EQ(labels[0], "spot");
    Matrix_<Cell_> cells;
    BlackScholesMonteCarloPlan_Get_ScriptConstants(plan, &cells);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 2.0);
    BlackScholesMonteCarloPlan_Get_Observations(plan, &cells);
    ASSERT_EQ(cells.Cols(), 4);
    ASSERT_EQ(cells.Rows(), 2);
    ASSERT_EQ(Cell::ToString(cells(1, 0)), "EQ[EXCEL_SEGMENTED]");
    ASSERT_FALSE(Cell::ToBool(cells(1, 2)));
    ASSERT_TRUE(Cell::IsEmpty(cells(1, 3)));
    BlackScholesMonteCarloResult_Get_Gradient(base, &cells);
    ASSERT_NEAR(Cell::ToDouble(cells(0, 0)), 40.0, 1.0e-10);
    cells(0, 0) = 999.0;
    ASSERT_NEAR(base->val_.mean_.MeanGradient()[0], 40.0, 1.0e-10);
    BlackScholesMonteCarloResult_Get_Point(base, &cells);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 10.0);
    for (const auto getter : {BlackScholesMonteCarloCurvatureResult_Get_Point, BlackScholesMonteCarloCurvatureResult_Get_Directions,
                              BlackScholesMonteCarloCurvatureResult_Get_Steps, BlackScholesMonteCarloCurvatureResult_Get_HessianProducts,
                              BlackScholesMonteCarloCurvatureResult_Get_Shape, BlackScholesMonteCarloCurvatureResult_Get_Execution})
        getter(result, &cells);
    ASSERT_EQ(cells.Rows(), 7);
    ASSERT_EQ(Cell::ToString(Field(cells, "method")), "BumpOverSegmentedNativeAAD");
    ASSERT_EQ(Cell::ToDouble(Field(cells, "gradient_evaluations")), 5.0);
    ASSERT_EQ(Cell::ToDouble(Field(cells, "numeric_payload_bytes")), 264.0);
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    BlackScholesMonteCarloResult_Get_Settings(base, &settings);
    ASSERT_EQ(settings->val_.path_.segmentSteps_, 64U);
    BlackScholesMonteCarloCurvatureResult_Get_Settings(result, &settings);
    ASSERT_EQ(settings->val_.path_.segmentSteps_, 64U);
    ASSERT_EQ(work.calls_, 0);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
}

TEST(ExcelMonteCarloCurvatureTest, TestStrictSettingsKindsKeysAndScrambleBoundsAreAtomic) {
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("original", {}, &settings);
    const auto original = settings;
    for (const auto& invalid : {Cell_(true), Cell_(Date_(2026, 1, 2)), Cell_("1"), Cell_(-1.0), Cell_(0.5), Cell_(9007199254740992.0),
                                Cell_(std::numeric_limits<double>::infinity())}) {
        ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"first_path", invalid}}), &settings), Exception_);
        ASSERT_EQ(settings, original);
    }
    for (const auto* key : {"00", "-1", "+1", " 1", "1 ", "1e3", "18446744073709551616"})
        ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"scramble_key", Cell_(key)}}), &settings), Exception_);
    ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"use_bb", Cell_(1.0)}}), &settings), Exception_);
    ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"segment_steps", Cell_(0.0)}}), &settings), Exception_);
    ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"unknown", Cell_()}}), &settings), Exception_);
    ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"rsg", Cell_("sobol")}, {"rsg", Cell_("sobol")}}), &settings), Exception_);
    ASSERT_THROW(SegmentedMonteCarloSettings_New("bad", Cells({{"rsg", Cell_(String_(std::string("so\0bol", 6)))}}), &settings), Exception_);
    ASSERT_EQ(settings, original);
}

TEST(ExcelMonteCarloCurvatureTest, TestZeroDirectionsAndSeparateExactNativeBudgetsRecover) {
    const auto plan = Plan();
    auto result = Curvature();
    const auto original = result;
    auto request = Bumps()->val_;
    request.numericPayloadBudgetBytes_ = 263;
    auto bumps = Handle_<StorableBumpOverAADRequest_>(new StorableBumpOverAADRequest_("small", request));
    ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, Point(), 1, bumps, {}, &result), Exception_);
    ASSERT_EQ(result, original);
    request.numericPayloadBudgetBytes_ = 264;
    request.recordingCapacityBudgetBytes_ = 0;
    bumps.reset(new StorableBumpOverAADRequest_("zero-recording", request));
    ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, Point(), 1, bumps, {}, &result), Exception_);
    ASSERT_EQ(result, original);
    request.recordingCapacityBudgetBytes_ = size_t(1) << 26;
    bumps.reset(new StorableBumpOverAADRequest_("exact", request));
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("path-cap", Cells({{"recording_capacity_budget_bytes", Cell_(double(size_t(1) << 27))}}), &settings);
    BlackScholesMonteCarloCurvatureResult_New("recovered", plan, Point(), 1, bumps, settings, &result);
    ASSERT_EQ(result->val_.Curvature().Execution().numericPayloadBytes_, 264U);
    ASSERT_EQ(result->val_.Curvature().Execution().recordingCapacityBudgetBytes_, size_t(1) << 26);
    ASSERT_EQ(result->val_.Curvature().Settings().path_.recordingCapacityBudgetBytes_, size_t(1) << 27);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(1, 0), 1.0, 1.0e-9);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(1, 4), -20.0, 1.0e-9);
    BlackScholesMonteCarloCurvatureResult_New("empty", plan, Point(), 1, Bumps(0), {}, &result);
    Matrix_<Cell_> cells;
    BlackScholesMonteCarloCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    BlackScholesMonteCarloCurvatureResult_Get_Shape(result, &cells);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_EQ(Cell::ToDouble(cells(0, 1)), 5.0);
    ASSERT_EQ(result->val_.Curvature().Execution().gradientEvaluations_, 1U);
    ASSERT_EQ(result->val_.Curvature().Execution().numericPayloadBytes_, 88U);
}

TEST(ExcelMonteCarloCurvatureTest, TestHistoricalPreparationSealsObservationsWithoutRereads) {
    InitializeRuntime();
    auto valuation = Valuation();
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[EXCEL_SEGMENTED_HISTORY]", {{DateTime_(Date_(2025, 1, 2), 0.0), 80.0}}}}));
    const Handle_<StorableScriptValuationSettings_> settings(new StorableScriptValuationSettings_("history", valuation));
    const auto product = NewScriptProduct("history", {Cell_("SCALE"), Cell_(Date_(2025, 1, 2))},
                                          {"2", "APPEND(v, SCALE * FIX(EQ[EXCEL_SEGMENTED_HISTORY])) pay PAYS 0 pay = SUM(v)"});
    Handle_<StorableBlackScholesMonteCarloPlan_> plan;
    BlackScholesMonteCarloPlan_New("sealed", product, settings, 0.01, &plan);
    valuation.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[EXCEL_SEGMENTED_HISTORY]", {{DateTime_(Date_(2025, 1, 2), 0.0), 999.0}}}}));
    Script::TestSupport::RejectFixingReads_ history;
    const ObserveWork_ observer(&history, nullptr);
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloResult_New("mean", plan, Point({100, 0.2, 0.03, 0.01, -3}), 33, {}, &mean);
    ASSERT_EQ(mean->val_.mean_.MeanValue(), -240.0);
    ASSERT_EQ(mean->val_.mean_.MeanGradient(), (Vector_<>{0, 0, 0, 0, 80}));
    Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result;
    BlackScholesMonteCarloCurvatureResult_New("curvature", plan, Point({100, 0.2, 0.03, 0.01, -3}), 1, Bumps(), {}, &result);
    ASSERT_EQ(result->val_.Curvature().Base().MeanValue(), -240.0);
    for (const auto value : result->val_.Curvature().HessianProducts())
        ASSERT_EQ(value, 0.0);
    Matrix_<Cell_> observations;
    BlackScholesMonteCarloPlan_Get_Observations(plan, &observations);
    ASSERT_TRUE(Cell::ToBool(observations(1, 2)));
    ASSERT_EQ(Cell::ToDouble(observations(1, 3)), 80.0);
    observations(1, 3) = 999.0;
    ASSERT_EQ(plan->val_.Observations()[0].value_, 80.0);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
}

TEST(ExcelMonteCarloCurvatureTest, TestCompletePointKindsPathsAndRequiredInputsRejectAtomically) {
    const auto plan = Plan();
    const auto bumps = Bumps();
    auto result = Curvature();
    const auto original = result;
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &mean);
    const auto originalMean = mean;
    for (const bool column : {false, true})
        for (const int index : {0, 2, 4}) {
            auto point = Point({10, 0.2, 0.03, 0.01, 2}, column);
            point(column ? index : 0, column ? 0 : index) = Cell_();
            ASSERT_THROW(BlackScholesMonteCarloResult_New("bad", plan, point, 1, {}, &mean), Exception_);
            ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, point, 1, bumps, {}, &result), Exception_);
        }
    for (const auto& invalid : {Cell_(true), Cell_("10"), Cell_(Date_(2026, 1, 2)), Cell_(std::numeric_limits<double>::infinity())}) {
        auto point = Point();
        point(0, 0) = invalid;
        ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, point, 1, bumps, {}, &result), Exception_);
    }
    for (const double paths : {0.0, -1.0, 1.5, 9007199254740992.0, std::numeric_limits<double>::infinity()}) {
        ASSERT_THROW(BlackScholesMonteCarloResult_New("bad", plan, Point(), paths, {}, &mean), Exception_);
        ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, Point(), paths, bumps, {}, &result), Exception_);
    }
    ASSERT_THROW(BlackScholesMonteCarloResult_New("bad", {}, Point(), 1, {}, &mean), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("bad", plan, Point(), 1, {}, {}, &result), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloResult_New("bad", plan, Matrix_<Cell_>(2, 3, Cell_(1.0)), 1, {}, &mean), Exception_);
    ASSERT_EQ(mean, originalMean);
    ASSERT_EQ(result, original);
    auto request = Bumps(0)->val_;
    request.numericPayloadBudgetBytes_ = 0;
    const Handle_<StorableBumpOverAADRequest_> zero(new StorableBumpOverAADRequest_("zero", request));
    CheckError([&] { BlackScholesMonteCarloCurvatureResult_New("large-path", plan, Point(), 2147483648.0, zero, {}, &result); },
               {"numeric payload budget"});
    ASSERT_EQ(result, original);
}

TEST(ExcelMonteCarloCurvatureTest, TestRequiredHandlesNamesAndArchivesRejectWithoutReplacingOutputs) {
    auto plan = Plan();
    const auto originalPlan = plan;
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", Valuation()));
    ASSERT_THROW(BlackScholesMonteCarloPlan_New("bad", {}, valuation, 0.01, &plan), Exception_);
    const auto& contract = plan->val_.Contract();
    const Handle_<ScriptProductData_> product(new ScriptProductData_(contract.Name(), contract.Dates(), contract.EventTexts(), contract.Settings()));
    ASSERT_THROW(BlackScholesMonteCarloPlan_New("bad", product, valuation, std::numeric_limits<double>::infinity(), &plan), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloPlan_New(String_(std::string("bad\0name", 8)), product, valuation, 0.01, &plan), Exception_);
    ASSERT_EQ(plan, originalPlan);
    auto result = Curvature();
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &mean);
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("settings", {}, &settings);
    ASSERT_THROW(JSON::WriteString(*settings), Exception_);
    ASSERT_THROW(JSON::WriteString(*plan), Exception_);
    ASSERT_THROW(JSON::WriteString(*mean), Exception_);
    ASSERT_THROW(JSON::WriteString(*result), Exception_);
    Matrix_<Cell_> cells(1, 1, Cell_(9.0));
    ASSERT_THROW(SegmentedMonteCarloSettings_Get_Configuration({}, &cells), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloPlan_Get_Observations({}, &cells), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloResult_Get_Gradient({}, &cells), Exception_);
    ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_Get_Execution({}, &cells), Exception_);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 9.0);
}

TEST(ExcelMonteCarloCurvatureTest, TestInvalidBumpedDomainAndCheckpointCapacityRecover) {
    const auto plan = Plan();
    auto result = Curvature();
    const auto original = result;
    auto request = Bumps(1)->val_;
    request.directions_(0, 1) = 1.0;
    request.steps_[0] = 0.3;
    const Handle_<StorableBumpOverAADRequest_> invalid(new StorableBumpOverAADRequest_("invalid", request));
    {
        Script::TestSupport::RejectSubmissions_ work;
        const ObserveWork_ observer(nullptr, &work);
        CheckError([&] { BlackScholesMonteCarloCurvatureResult_New("bad", plan, Point(), 1, invalid, {}, &result); }, {"direction=0", "minus"});
        ASSERT_EQ(work.calls_, 0);
    }
    ASSERT_EQ(result, original);
    Handle_<StorableSegmentedMonteCarloSettings_> settings;
    SegmentedMonteCarloSettings_New("no-checkpoints", Cells({{"checkpoint_capacity_budget_bytes", Cell_(0.0)}}), &settings);
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &mean);
    const auto originalMean = mean;
    const auto future = Plan(false);
    ASSERT_THROW(BlackScholesMonteCarloResult_New("bad", future, Point(), 1, settings, &mean), Exception_);
    ASSERT_EQ(mean, originalMean);
    BlackScholesMonteCarloResult_New("recovered", future, Point({100, 0, 0.03, 0.01, 2}), 1, {}, &mean);
    ASSERT_NEAR(mean->val_.mean_.MeanValue(), 200.0 * std::exp(-0.01), 1.0e-10);
}

TEST(ExcelMonteCarloCurvatureTest, TestPassiveQueriesAndNestedEntriesPreserveCallerGraphAndSeed) {
    const auto plan = Plan();
    const auto point = Point();
    const auto bumps = Bumps();
    auto result = Curvature();
    const auto original = result;
    Handle_<StorableBlackScholesMonteCarloResult_> mean;
    BlackScholesMonteCarloCurvatureResult_Get_Base(result, &mean);
    const auto originalMean = mean;
#ifdef _WIN32
    Excel::ScriptTestInitialize(1);
    double adjoint = 0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodes) {
#else
    AAD::Clear(*AAD::Tape());
    const auto mode = AAD::SetNumResultsForAAD(true, 3);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    AAD::Number_ square = input * input;
    AAD::NativeOperations_::SetSeed(square, 7.0, 2);
    const auto nodes = AAD::Tape()->nodes_.Size();
#endif
            Matrix_<Cell_> cells;
            BlackScholesMonteCarloPlan_Get_Observations(plan, &cells);
            BlackScholesMonteCarloResult_Get_Execution(mean, &cells);
            for (const auto getter : {BlackScholesMonteCarloCurvatureResult_Get_Point, BlackScholesMonteCarloCurvatureResult_Get_Directions,
                                      BlackScholesMonteCarloCurvatureResult_Get_Steps, BlackScholesMonteCarloCurvatureResult_Get_HessianProducts,
                                      BlackScholesMonteCarloCurvatureResult_Get_Shape, BlackScholesMonteCarloCurvatureResult_Get_Execution})
                getter(result, &cells);
            Handle_<StorableBlackScholesMonteCarloResult_> copied;
            BlackScholesMonteCarloCurvatureResult_Get_Base(result, &copied);
            ASSERT_THROW(BlackScholesMonteCarloResult_New("nested", plan, point, 1, {}, &mean), Exception_);
            ASSERT_THROW(BlackScholesMonteCarloCurvatureResult_New("nested", plan, point, 1, bumps, {}, &result), Exception_);
            ASSERT_EQ(result, original);
            ASSERT_EQ(mean, originalMean);
#ifdef _WIN32
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodes);
        },
        false, &adjoint);
    ASSERT_EQ(adjoint, 4.0);
#else
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3);
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(square, 2), 7.0);
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 28.0);
    recording.Close();
#endif
}
