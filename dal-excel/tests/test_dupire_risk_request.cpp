//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-excel/src/__riskrequest.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/initall.hpp>

#include <script_test_observers.hpp>

#include <dal-excel/src/__riskrequestinput.hpp>

using namespace Dal;

namespace {
    struct SingleWorker_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        std::pair<size_t, bool> excelState_;
        SingleWorker_() {
            RegisterAll_::Init();
            Excel::ScriptTestInitialize(1);
            excelState_ = Excel::ScriptTestStartWorkers(1);
            pool_->Start(1, true);
        }
        ~SingleWorker_() {
            Excel::ScriptTestRestoreWorkers(excelState_);
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    Handle_<StorableDupireCalibration_> Calibration(bool merton = false) {
        const DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
        const auto value =
            merton ? CalibrateDupireWithRisk(NewMertonIVS(100, 0.2, 0.08, -0.1, 0.15), inputs) : CalibrateDupireWithRisk(FlatIVS_(), inputs);
        return Handle_<StorableDupireCalibration_>(new StorableDupireCalibration_("source", value));
    }

    Handle_<ScriptProductData_> Product() {
        Handle_<ScriptProductData_> product;
        Product_New("smooth", {Cell_("QUOTE"), Cell_(double(Date::ToExcel(Date_(2027, 9, 12))))},
                    {"0", "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 3 * QUOTE"}, &product);
        return product;
    }

    double NumericValue(double value) { return value; }
    double NumericValue(const Cell_& value) { return Cell::ToDouble(value); }

    template <class T_> void AssertCells(const Matrix_<T_>& cells, const Matrix_<>& expected) {
        ASSERT_EQ(cells.Rows(), expected.Rows());
        ASSERT_EQ(cells.Cols(), expected.Cols());
        for (int row = 0; row < cells.Rows(); ++row)
            for (int column = 0; column < cells.Cols(); ++column)
                ASSERT_NEAR(NumericValue(cells(row, column)), expected(row, column), 1e-10);
    }

    Matrix_<Cell_> QuoteSettings(int selection, double budget = 304) {
        Matrix_<Cell_> cells(selection == 0 ? 1 : 3, 2);
        cells(0, 0) = "numeric_payload_budget_bytes";
        cells(0, 1) = budget;
        if (selection) {
            cells(1, 0) = "inputs";
            cells(1, 1) = selection == 1 ? "quote:3;quote:0" : "";
            cells(2, 0) = "report_factors";
            cells(2, 1) = selection == 1 ? "0.01;0.5" : "";
        }
        return cells;
    }

    auto Settings(bool compiled = false) {
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 9, 12);
        MonteCarloSettings_ simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const Handle_<StorableScriptValuationSettings_> val(new StorableScriptValuationSettings_("valuation", valuation));
        const Handle_<StorableMonteCarloSettings_> sim(new StorableMonteCarloSettings_("simulation", simulation));
        Handle_<StorableDupireScriptRiskSettings_> settings;
        DupireScriptRiskSettings_New("settings", 257, val, sim, &settings);
        return settings;
    }

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
} // namespace

TEST(ExcelDupireRequestTest, TestAutomaticBoundQuotesMatchIndependentNativeRequest) {
    const SingleWorker_ workers;
    const auto source = Calibration();
    Handle_<ModelData_> model;
    DupireModelData_New("model", source, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
    const auto product = Product();
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    const Handle_<StorableScriptValuationSettings_> valuationHandle(new StorableScriptValuationSettings_("valuation", valuation));
    Handle_<StorableDupireScriptRiskSettings_> settings;
    DupireScriptRiskSettings_New("execution", 257, valuationHandle, {}, &settings);
    Matrix_<Cell_> bindings(1, 2);
    bindings(0, 0) = 0.0;
    bindings(0, 1) = "quote:0";
    Handle_<StorableDupireScriptRiskRequest_> request;
    DupireScriptRiskRequest_New("request", settings, {}, bindings, {}, &request);
    Handle_<StorableDupireScriptRiskPlan_> plan;
    DupireScriptRiskPlan_New("plan", product, model, source, "equity", request, &plan);
    DupireScriptRiskRequest_ native;
    native.numPaths_ = 257;
    native.valuation_ = valuation;
    native.directBindings_ = {{0, "quote:0"}};
    const auto reference = ValueByMonteCarloWithDupireRisk(PlanDupireScriptRisk(product, model, source->val_, "equity", native));
    Handle_<StorableDupireScriptRiskResult_> result;
    DupireScriptRiskResult_New("result", plan, &result);
    Handle_<StorableRiskResult_> price;
    DupireScriptRiskResult_Get_Valuation(result, &price);
    ASSERT_NEAR(price->val_.Values()[0], reference.Valuation().Values()[0], 1e-10);
    Handle_<StorableCalibrationRiskResult_> quoteRisk;
    DupireScriptRiskResult_Get_QuoteRisk(result, &quoteRisk);
    Matrix_<Cell_> cells;
    CalibrationRiskResult_Get_Jacobian(quoteRisk, "Total", &cells);
    ASSERT_NO_FATAL_FAILURE(AssertCells(cells, reference.QuoteRisk().Jacobian()));
    ASSERT_EQ(plan->val_.NumericPayloadBytes(), 304);
    ASSERT_NEAR(quoteRisk->val_.QuoteRisk().DirectAdjoints()(0, 0), 3.0 * std::exp(-0.05), 1e-10);
}

namespace {
    void CheckQuoteProjections(const Handle_<StorableCalibrationRiskResult_>& actual, const CalibrationRiskResult_& expected, bool empty) {
        using Projection_ = Matrix_<> (CalibrationRiskResult_::*)() const;
        const Vector_<std::pair<String_, Projection_>> projections{{"Total", &CalibrationRiskResult_::Jacobian},
                                                                   {"Calibration", &CalibrationRiskResult_::CalibrationJacobian},
                                                                   {"Direct", &CalibrationRiskResult_::DirectJacobian},
                                                                   {"Reported", &CalibrationRiskResult_::ReportedJacobian}};
        for (const auto& projection : projections) {
            Matrix_<Cell_> cells;
            CalibrationRiskResult_Get_Jacobian(actual, projection.first, &cells);
            if (empty) {
                ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
                ASSERT_EQ(actual->val_.QuoteRisk().TotalAdjoints().Rows(), 3);
            } else {
                ASSERT_NO_FATAL_FAILURE(AssertCells(cells, (expected.*projection.second)()));
            }
        }
        const auto& raw = actual->val_.QuoteRisk();
        const auto& reference = expected.QuoteRisk();
        ASSERT_NO_FATAL_FAILURE(AssertCells(raw.CalibrationAdjoints(), reference.CalibrationAdjoints()));
        ASSERT_NO_FATAL_FAILURE(AssertCells(raw.DirectAdjoints(), reference.DirectAdjoints()));
        ASSERT_NO_FATAL_FAILURE(AssertCells(raw.TotalAdjoints(), reference.TotalAdjoints()));
    }

    void CheckAutomaticCase(bool merton, bool compiled, int selection, bool external) {
        const auto calibration = Calibration(merton);
        Handle_<ModelData_> model;
        DupireModelData_New("model", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
        const auto product = Product();
        const auto boundary = NewCalibrationPullback(calibration->val_);
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct(
            new StorableCalibrationDirectQuoteAdjoints_("direct", NewCalibrationDirectQuoteAdjoints(boundary, Matrix_<>(3, 2, -0.125))));
        const auto settings = Settings(compiled);
        const double bytes = external ? 296 : 304;
        Handle_<StorableCalibrationRiskRequest_> quotes;
        CalibrationRiskRequest_New("quotes", QuoteSettings(selection, bytes), &quotes);
        Matrix_<Cell_> bindings(1, 2);
        bindings(0, 0) = 0.0;
        bindings(0, 1) = "quote:0";
        Handle_<StorableDupireScriptRiskRequest_> request;
        DupireScriptRiskRequest_New("request", settings, quotes, external ? Matrix_<Cell_>() : bindings,
                                    external ? direct : Handle_<StorableCalibrationDirectQuoteAdjoints_>(), &request);
        DupireScriptRiskRequest_ native;
        native.numPaths_ = 257;
        native.valuation_ = settings->val_.valuation_;
        native.simulation_ = settings->val_.simulation_;
        native.quotes_ = quotes->val_;
        if (external)
            native.direct_ = direct->val_;
        else
            native.directBindings_ = {{0, "quote:0"}};
        if (external) {
            Handle_<StorableCalibrationDirectQuoteAdjoints_> copy;
            DupireScriptRiskRequest_Get_Direct(request, &copy);
            ASSERT_NO_FATAL_FAILURE(AssertCells(copy->val_.Adjoints(), direct->val_.Adjoints()));
        }
        const auto reference = ValueByMonteCarloWithDupireRisk(PlanDupireScriptRisk(product, model, calibration->val_, "equity", native));
        Handle_<StorableDupireScriptRiskPlan_> plan;
        DupireScriptRiskPlan_New("plan", product, model, calibration, "equity", request, &plan);
        Handle_<StorableDupireScriptRiskResult_> result;
        DupireScriptRiskResult_New("result", plan, &result);
        ASSERT_NEAR(result->val_.Valuation().Values()[0], reference.Valuation().Values()[0], 1e-10);
        ASSERT_EQ(result->val_.Method(), reference.Method());
        ASSERT_EQ(result->val_.NumericPayloadBytes(), bytes);
        Handle_<StorableRiskResult_> valuation;
        DupireScriptRiskResult_Get_Valuation(result, &valuation);
        Matrix_<Cell_> cells;
        RiskResult_Get_Jacobian(valuation, false, &cells);
        ASSERT_NO_FATAL_FAILURE(AssertCells(cells, reference.Valuation().Jacobian()));
        Handle_<StorableCalibrationRiskResult_> quoteRisk;
        DupireScriptRiskResult_Get_QuoteRisk(result, &quoteRisk);
        ASSERT_NO_FATAL_FAILURE(CheckQuoteProjections(quoteRisk, reference.QuoteRisk(), selection == 2));
    }
} // namespace

TEST(ExcelDupireRequestTest, TestFlatMertonCompiledSelectionsAndExternalDirectMatchNative) {
    const SingleWorker_ workers;
    for (bool merton : {false, true})
        for (bool compiled : {false, true})
            for (int selection : {0, 1, 2})
                for (bool external : {false, true})
                    ASSERT_NO_FATAL_FAILURE(CheckAutomaticCase(merton, compiled, selection, external));
}

TEST(ExcelDupireRequestTest, TestStrictOrdinalsCountsAndNullFactoriesPreserveOutputs) {
    const SingleWorker_ workers;
    auto settings = Settings();
    const auto savedSettings = settings;
    for (double count : {0.0, -1.0, 1.5, std::numeric_limits<double>::infinity(), double((std::numeric_limits<int>::max)()) + 1}) {
        ASSERT_THROW(DupireScriptRiskSettings_New("bad", count, {}, {}, &settings), Exception_);
        ASSERT_EQ(settings.get(), savedSettings.get());
    }
    Handle_<StorableDupireScriptRiskRequest_> request;
    DupireScriptRiskRequest_New("valid", settings, {}, {}, {}, &request);
    const auto saved = request;
    Matrix_<Cell_> bindings(1, 2);
    bindings(0, 1) = "quote:0";
    for (const auto& bad : {Cell_(true), Cell_("0"), Cell_(-1.0), Cell_(0.25), Cell_(9007199254740992.0),
                            Cell_(std::numeric_limits<double>::quiet_NaN()), Cell_(std::numeric_limits<double>::infinity()), Cell_()}) {
        bindings(0, 0) = bad;
        ASSERT_THROW(DupireScriptRiskRequest_New("bad", settings, {}, bindings, {}, &request), Exception_);
        ASSERT_EQ(request.get(), saved.get());
    }
    bindings(0, 0) = 0.0;
    bindings(0, 1) = String_(std::string("quote:0\0hidden", 14));
    ASSERT_THROW(DupireScriptRiskRequest_New("bad", settings, {}, bindings, {}, &request), Exception_);
    ASSERT_THROW(DupireScriptRiskRequest_New("bad", {}, {}, {}, {}, &request), Exception_);
    ASSERT_EQ(request.get(), saved.get());
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
    ASSERT_THROW(DupireScriptRiskRequest_Get_Direct(request, &direct), Exception_);
    ASSERT_FALSE(direct);
    DupireScriptRiskRequest_New("recovered", settings, {}, {}, {}, &request);
    ASSERT_EQ(request->val_.numPaths_, 257);
}

TEST(ExcelDupireRequestTest, TestPlanningBudgetAndBindingErrorsPrecedeHistoryWorkersAndRecover) {
    const SingleWorker_ workers;
    const auto calibration = Calibration();
    Handle_<ModelData_> model;
    DupireModelData_New("model", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
    const auto product = Product();
    const auto settings = Settings();
    Handle_<StorableDupireScriptRiskRequest_> request;
    DupireScriptRiskRequest_New("valid", settings, {}, {}, {}, &request);
    Handle_<StorableDupireScriptRiskPlan_> plan;
    DupireScriptRiskPlan_New("valid", product, model, calibration, "equity", request, &plan);
    const auto saved = plan;
    const RejectGetterWork_ reject;
    for (int selection : {0, 1, 2}) {
        Handle_<StorableCalibrationRiskRequest_> quotes;
        CalibrationRiskRequest_New("too-small", QuoteSettings(selection, 295), &quotes);
        DupireScriptRiskRequest_New("bad", settings, quotes, {}, {}, &request);
        ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "equity", request, &plan), Exception_);
        ASSERT_EQ(plan.get(), saved.get());
    }
    Matrix_<Cell_> bindings(1, 2);
    for (double ordinal : {1.0, 99.0}) {
        bindings(0, 0) = ordinal;
        bindings(0, 1) = "quote:0";
        DupireScriptRiskRequest_New("bad", settings, {}, bindings, {}, &request);
        ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "equity", request, &plan), Exception_);
    }
    DupireScriptRiskRequest_New("valid", settings, {}, {}, {}, &request);
    const auto boundary = NewCalibrationPullback(calibration->val_);
    Handle_<StorableCalibrationDirectQuoteAdjoints_> direct(
        new StorableCalibrationDirectQuoteAdjoints_("direct", NewCalibrationDirectQuoteAdjoints(boundary, Matrix_<>(3, 2, -0.25))));
    const auto savedDirect = direct;
    ASSERT_THROW(DupireScriptRiskRequest_Get_Direct(request, &direct), Exception_);
    ASSERT_EQ(direct.get(), savedDirect.get());
    bindings(0, 0) = 0.0;
    bindings(0, 1) = "quote:0";
    DupireScriptRiskRequest_New("exclusive", settings, {}, bindings, direct, &request);
    ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "equity", request, &plan), Exception_);
    auto disabled = DefaultRiskMonteCarloSettings();
    disabled.enableAad_ = false;
    const Handle_<StorableMonteCarloSettings_> passive(new StorableMonteCarloSettings_("passive", disabled));
    Handle_<StorableDupireScriptRiskSettings_> passiveSettings;
    DupireScriptRiskSettings_New(
        "passive", 257, Handle_<StorableScriptValuationSettings_>(new StorableScriptValuationSettings_("valuation", settings->val_.valuation_)),
        passive, &passiveSettings);
    DupireScriptRiskRequest_New("passive", passiveSettings, {}, {}, {}, &request);
    ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "equity", request, &plan), Exception_);
    DupireScriptRiskRequest_New("valid", settings, {}, {}, {}, &request);
    ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "EQ[LOCAL]", request, &plan), Exception_);
    ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, {}, "equity", request, &plan), Exception_);
    ASSERT_THROW(DupireScriptRiskPlan_New("bad", product, model, calibration, "equity", {}, &plan), Exception_);
    DupireScriptRiskPlan_New("recovered", product, model, calibration, "equity", request, &plan);
    ASSERT_EQ(plan->val_.NumericPayloadBytes(), 296);
}

TEST(ExcelDupireRequestTest, TestOwningConfigurationAndAllPassiveGettersPreserveRecording) {
    const SingleWorker_ workers;
    const auto calibration = Calibration();
    Handle_<ModelData_> model;
    DupireModelData_New("model", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
    auto settings = Settings();
    Handle_<StorableDupireScriptRiskRequest_> request;
    DupireScriptRiskRequest_New("request", settings, {}, {}, {}, &request);
    Handle_<StorableDupireScriptRiskPlan_> plan;
    DupireScriptRiskPlan_New("plan", Product(), model, calibration, "equity", request, &plan);
    Handle_<StorableDupireScriptRiskResult_> result;
    DupireScriptRiskResult_New("result", plan, &result);
    const auto saved = result;
    ASSERT_THROW(DupireScriptRiskResult_New("bad", {}, &result), Exception_);
    ASSERT_EQ(result.get(), saved.get());
    const RejectGetterWork_ reject;
    double adjoint = 0;
    Excel::ScriptTestWithRecording(
        [&](size_t before) {
            Handle_<StorableCalibrationRiskRequest_> quotes;
            Matrix_<Cell_> cells;
            bool hasDirect = true;
            DupireScriptRiskRequest_Get_Configuration(request, &settings, &quotes, &cells, &hasDirect);
            ASSERT_FALSE(hasDirect);
            ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
            double paths = 0;
            Handle_<StorableScriptValuationSettings_> valuationSettings;
            Handle_<StorableMonteCarloSettings_> simulation;
            DupireScriptRiskSettings_Get_Configuration(settings, &paths, &valuationSettings, &simulation);
            ASSERT_DOUBLE_EQ(paths, 257);
            ASSERT_TRUE(simulation->val_.enableAad_);
            DupireScriptRiskPlan_Get_Configuration(plan, &settings, &cells);
            ASSERT_EQ(*settings->val_.valuation_.evaluationDate_, Date_(2026, 9, 12));
            DupireScriptRiskPlan_Get_Inputs(plan, false, &cells);
            ASSERT_EQ(cells.Rows(), 19);
            ASSERT_EQ(cells.Cols(), 8);
            cells(1, 0) = "changed";
            DupireScriptRiskPlan_Get_Inputs(plan, true, &cells);
            ASSERT_NE(Cell::ToString(cells(1, 0)), "changed");
            Handle_<StorableCalibrationRiskPlan_> quotePlan;
            DupireScriptRiskPlan_Get_QuotePlan(plan, &quotePlan);
            ASSERT_EQ(quotePlan->val_.InputAxis().size(), 6);
            DupireScriptRiskPlan_Get_Provenance(plan, &cells);
            ASSERT_EQ(Cell::ToString(cells(0, 1)), "equity");
            DupireScriptRiskResult_Get_Provenance(result, &cells);
            ASSERT_EQ(Cell::ToString(cells(1, 1)), result->val_.Method());
            Handle_<StorableRiskResult_> valuation;
            DupireScriptRiskResult_Get_Valuation(result, &valuation);
            ASSERT_EQ(valuation->val_.Provenance().calibration_, "fixed");
            Handle_<StorableCalibrationRiskResult_> risk;
            DupireScriptRiskResult_Get_QuoteRisk(result, &risk);
            CalibrationRiskResult_Get_Jacobian(risk, "Reported", &cells);
            const auto previous = Cell::ToDouble(cells(0, 0));
            cells(0, 0) = 999.0;
            CalibrationRiskResult_Get_Jacobian(risk, "Reported", &cells);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), previous);
            ASSERT_THROW(DupireScriptRiskResult_Get_Provenance({}, &cells), Exception_);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), previous);
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), before);
        },
        false, &adjoint);
    ASSERT_DOUBLE_EQ(adjoint, 4.0);
}

#ifdef _WIN32
TEST(ExcelDupireRequestTest, TestWorksheetOrdinalGuardsRejectCoercionBeforeConversion) {
    OPER_ cells[2]{};
    wchar_t quote[]{7, L'q', L'u', L'o', L't', L'e', L':', L'0'};
    cells[0].xltype = xltypeInt;
    cells[0].val.w = 0;
    cells[1].xltype = xltypeStr;
    cells[1].val.str = quote;
    OPER_ input{};
    input.xltype = xltypeMulti;
    input.val.array = {cells, 1, 2};
    ASSERT_NO_THROW(Excel::ValidateRiskRequestBindings(&input));
    for (const auto type : {xltypeBool, xltypeStr, xltypeErr, xltypeMissing, xltypeNil}) {
        cells[0].xltype = type;
        if (type == xltypeStr)
            cells[0].val.str = quote;
        ASSERT_THROW(Excel::ValidateRiskRequestBindings(&input), Exception_);
    }
    cells[0].xltype = xltypeNum;
    for (double ordinal : {-1.0, 0.5, 9007199254740992.0, std::numeric_limits<double>::infinity()}) {
        cells[0].val.num = ordinal;
        ASSERT_THROW(Excel::ValidateRiskRequestBindings(&input), Exception_);
    }
    cells[0].val.num = 0;
    quote[3] = L'\0';
    ASSERT_THROW(Excel::ValidateRiskRequestBindings(&input), Exception_);
    input.val.array = {nullptr, (std::numeric_limits<int>::max)(), 3};
    ASSERT_THROW(Excel::ValidateRiskRequestBindings(&input), Exception_);
}

TEST(ExcelDupireRequestTest, TestWorksheetPathsTextAndBooleanGuardsAreStrict) {
    OPER_ cell{};
    OPER_ input{};
    input.xltype = xltypeMulti;
    input.val.array = {&cell, 1, 1};
    for (const auto type : {xltypeBool, xltypeStr, xltypeErr, xltypeNil, xltypeMissing}) {
        cell.xltype = type;
        ASSERT_THROW(Excel::ValidateRiskRequestPaths(&input), Exception_);
    }
    cell.xltype = xltypeInt;
    cell.val.w = 7;
    ASSERT_NO_THROW(Excel::ValidateRiskRequestPaths(&input));
    ASSERT_THROW(Excel::ValidateRiskRequestText(&input, "component"), Exception_);
    ASSERT_THROW(Excel::ValidateRiskRequestBoolean(&input, "complete"), Exception_);
    wchar_t text[]{3, L'a', L'\0', L'b'};
    cell.xltype = xltypeStr;
    cell.val.str = text;
    ASSERT_THROW(Excel::ValidateRiskRequestText(&input, "component"), Exception_);
    for (const auto type : {xltypeBool, xltypeMissing, xltypeNil}) {
        cell.xltype = type;
        ASSERT_NO_THROW(Excel::ValidateRiskRequestBoolean(&input, "complete"));
    }
}
#endif
