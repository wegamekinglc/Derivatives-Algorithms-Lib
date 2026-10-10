//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <dal-cpp/test-support/script_test_observers.hpp>
#include <dal-excel/src/__dupirecurvature.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/script.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    struct SingleWorker_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
#ifdef _WIN32
        std::pair<size_t, bool> excelState_;
#endif
        SingleWorker_() {
            RegisterAll_::Init();
            pool_->Start(1, true);
#ifdef _WIN32
            Excel::ScriptTestInitialize(1);
            excelState_ = Excel::ScriptTestStartWorkers(1);
#endif
        }
        ~SingleWorker_() {
#ifdef _WIN32
            Excel::ScriptTestRestoreWorkers(excelState_);
#endif
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

    Handle_<StorableDupireCalibration_> Calibration() {
        const DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.001), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
        return Handle_<StorableDupireCalibration_>(new StorableDupireCalibration_("source", CalibrateDupireWithRisk(FlatIVS_(), inputs)));
    }

    Handle_<StorableDupireScriptCurvatureRequest_> Request(bool compiled = false) {
        DupireScriptRiskRequest_ risk;
        risk.numPaths_ = 17;
        risk.valuation_.evaluationDate_ = Date_(2026, 9, 12);
        risk.simulation_.compiled_ = compiled;
        risk.directBindings_ = {{0, "quote:3"}};
        const Handle_<StorableDupireScriptRiskRequest_> first(new StorableDupireScriptRiskRequest_("risk", risk));
        Matrix_<Cell_> directions(3, 6, Cell_(0.0));
        directions(0, 3) = 1.0;
        directions(1, 3) = -2.0;
        directions(2, 0) = 1.0;
        Matrix_<Cell_> steps(3, 1, Cell_(0.0002));
        steps(1, 0) = 0.0001;
        Handle_<StorableBumpOverAADRequest_> bumps;
        BumpOverAADRequest_New("bumps", directions, steps, {}, &bumps);
        Handle_<StorableDupireScriptCurvatureRequest_> request;
        DupireScriptCurvatureRequest_New("request", first, bumps, &request);
        return request;
    }

    Handle_<StorableDupireScriptCurvaturePlan_> Plan(const Handle_<StorableDupireScriptCurvatureRequest_>& request,
                                                     Handle_<ScriptProductData_> product = {}) {
        const auto source = Calibration();
        const auto model = NewDupireModelData("model", source->val_, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25);
        if (!product)
            product = NewScriptProduct("quadratic", {Cell_("QUOTE"), Cell_(Date_(2027, 9, 12))}, {"0.001", "pay PAYS QUOTE * QUOTE"});
        Handle_<StorableDupireScriptCurvaturePlan_> plan;
        DupireScriptCurvaturePlan_New("plan", product, model, source, "equity", request, &plan);
        return plan;
    }

    Handle_<StorableDupireScriptCurvatureRequest_> CopyRequest(const DupireScriptCurvatureRequest_& value) {
        const Handle_<StorableDupireScriptRiskRequest_> first(new StorableDupireScriptRiskRequest_("risk", value.risk_));
        const Handle_<StorableBumpOverAADRequest_> bumps(new StorableBumpOverAADRequest_("bumps", value.bumps_));
        Handle_<StorableDupireScriptCurvatureRequest_> request;
        DupireScriptCurvatureRequest_New("request", first, bumps, &request);
        return request;
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

    struct RejectAtSubmission_ final : Script::Detail::SimulationObserver_ {
        size_t calls_ = 0;
        size_t failAt_;
        explicit RejectAtSubmission_(size_t failAt) : failAt_(failAt) {}
        void AfterSubmission() override {
            if (++calls_ == failAt_)
                THROW("injected Excel curvature submission failure");
        }
    };
} // namespace

TEST(ExcelDupireCurvatureTest, TestRequestCopiesRiskAndBumpsIntoDetachedHandles) {
    DupireScriptRiskRequest_ nativeRisk;
    nativeRisk.numPaths_ = 17;
    nativeRisk.valuation_.evaluationDate_ = Date_(2026, 9, 12);
    nativeRisk.directBindings_ = {{0, "quote:3"}};
    const Handle_<StorableDupireScriptRiskRequest_> risk(new StorableDupireScriptRiskRequest_("risk", nativeRisk));
    Matrix_<Cell_> directions(1, 6, Cell_(0.0));
    directions(0, 3) = -2.0;
    Handle_<StorableBumpOverAADRequest_> bumps;
    BumpOverAADRequest_New("bumps", directions, Matrix_<Cell_>(1, 1, Cell_(0.0001)), {}, &bumps);
    Handle_<StorableDupireScriptCurvatureRequest_> request;
    DupireScriptCurvatureRequest_New("request", risk, bumps, &request);
    nativeRisk.numPaths_ = 99;
    directions(0, 3) = 99.0;
    Handle_<StorableDupireScriptRiskRequest_> copiedRisk;
    DupireScriptCurvatureRequest_Get_Risk(request, &copiedRisk);
    ASSERT_NE(copiedRisk.get(), risk.get());
    ASSERT_EQ(copiedRisk->val_.numPaths_, 17);
    ASSERT_EQ(copiedRisk->val_.directBindings_[0].quoteId_, "quote:3");
    Handle_<StorableBumpOverAADRequest_> copiedBumps;
    DupireScriptCurvatureRequest_Get_Bumps(request, &copiedBumps);
    ASSERT_NE(copiedBumps.get(), bumps.get());
    ASSERT_DOUBLE_EQ(copiedBumps->val_.directions_(0, 3), -2.0);
    ASSERT_DOUBLE_EQ(copiedBumps->val_.steps_[0], 0.0001);
    ASSERT_EQ(request->Type(), "DupireScriptCurvatureRequest");
}

TEST(ExcelDupireCurvatureTest, TestSignedQuadraticMatchesIndependentAnalyticGammaInBothEngines) {
    const SingleWorker_ workers;
    const double discount = std::exp(-0.05);
    for (const bool compiled : {false, true}) {
        const auto plan = Plan(Request(compiled));
        Handle_<StorableDupireScriptCurvatureResult_> result;
        DupireScriptCurvatureResult_New("curvature", plan, &result);
        ASSERT_NEAR(result->val_.Base().Valuation().Values()[0], 1e-6 * discount, 1e-14);
        Matrix_<Cell_> gradient;
        DupireScriptCurvatureResult_Get_Gradient(result, &gradient);
        ASSERT_EQ(gradient.Rows(), 6);
        ASSERT_EQ(gradient.Cols(), 1);
        Matrix_<Cell_> products;
        DupireScriptCurvatureResult_Get_HessianProducts(result, &products);
        ASSERT_EQ(products.Rows(), 3);
        ASSERT_EQ(products.Cols(), 6);
        const double multipliers[3] = {1.0, -2.0, 0.0};
        for (int quote = 0; quote < 6; ++quote) {
            ASSERT_NEAR(Cell::ToDouble(gradient(quote, 0)), quote == 3 ? 0.002 * discount : 0.0, 1e-12);
            for (int direction = 0; direction < 3; ++direction)
                ASSERT_NEAR(Cell::ToDouble(products(direction, quote)), quote == 3 ? 2.0 * multipliers[direction] * discount : 0.0, 1e-10);
        }
        ASSERT_EQ(result->val_.Execution().quoteGradientEvaluations_, 7);
        ASSERT_EQ(result->val_.Execution().pathsPerEvaluation_, 17);
        ASSERT_EQ(result->val_.Execution().numericPayloadBytes_, 720);
        ASSERT_EQ(plan->val_.NumericPayloadBytes(), 720);
    }
}

TEST(ExcelDupireCurvatureTest, TestPlanGettersRetainFrozenConfigurationAndDetachedNumerics) {
    const SingleWorker_ workers;
    const auto plan = Plan(Request(true));
    Handle_<StorableDupireScriptRiskPlan_> base;
    DupireScriptCurvaturePlan_Get_BasePlan(plan, &base);
    ASSERT_EQ(base->val_.NumPaths(), 17);
    ASSERT_TRUE(base->val_.SimulationSettings().compiled_.value_or(false));
    ASSERT_EQ(base->val_.QuotePlan().CompleteInputAxis().size(), 6);
    Matrix_<Cell_> cells;
    DupireScriptCurvaturePlan_Get_Point(plan, &cells);
    ASSERT_EQ(cells.Rows(), 6);
    ASSERT_EQ(cells.Cols(), 1);
    for (int quote = 0; quote < 6; ++quote)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(quote, 0)), 0.001);
    cells(0, 0) = 99.0;
    DupireScriptCurvaturePlan_Get_Point(plan, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.001);
    DupireScriptCurvaturePlan_Get_Directions(plan, &cells);
    ASSERT_EQ(cells.Rows(), 3);
    ASSERT_EQ(cells.Cols(), 6);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 6; ++column)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, column)), plan->val_.Directions()(row, column));
    DupireScriptCurvaturePlan_Get_Steps(plan, &cells);
    ASSERT_EQ(cells.Rows(), 3);
    ASSERT_EQ(cells.Cols(), 1);
    for (int row = 0; row < 3; ++row)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 0)), plan->val_.Steps()[row]);
    DupireScriptCurvaturePlan_Get_Shape(plan, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 3.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 6.0);
    double payload = -1.0;
    DupireScriptCurvaturePlan_Get_Payload(plan, &payload);
    ASSERT_DOUBLE_EQ(payload, 720.0);
    ASSERT_EQ(plan->Type(), "DupireScriptCurvaturePlan");
}

TEST(ExcelDupireCurvatureTest, TestResultGettersReturnCompleteAxisWorkCountsAndIndependentCopies) {
    const SingleWorker_ workers;
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("result", Plan(Request()), &result);
    Handle_<StorableDupireScriptRiskResult_> base;
    DupireScriptCurvatureResult_Get_Base(result, &base);
    ASSERT_NEAR(base->val_.Valuation().Values()[0], 1e-6 * std::exp(-0.05), 1e-14);
    Handle_<StorableCalibrationRiskPlan_> quotePlan;
    DupireScriptCurvatureResult_Get_QuotePlan(result, &quotePlan);
    ASSERT_EQ(quotePlan->val_.CompleteInputAxis().size(), 6);
    for (int quote = 0; quote < 6; ++quote)
        ASSERT_EQ(quotePlan->val_.CompleteInputAxis()[quote].id_, "quote:" + String_(std::to_string(quote)));
    Matrix_<Cell_> cells;
    DupireScriptCurvatureResult_Get_Point(result, &cells);
    ASSERT_EQ(cells.Rows(), 6);
    ASSERT_EQ(cells.Cols(), 1);
    for (int quote = 0; quote < 6; ++quote)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(quote, 0)), 0.001);
    cells(0, 0) = 99.0;
    DupireScriptCurvatureResult_Get_Point(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.001);
    DupireScriptCurvatureResult_Get_Directions(result, &cells);
    ASSERT_EQ(cells.Rows(), 3);
    ASSERT_EQ(cells.Cols(), 6);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 6; ++column)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, column)), result->val_.Directions()(row, column));
    DupireScriptCurvatureResult_Get_Steps(result, &cells);
    ASSERT_EQ(cells.Rows(), 3);
    ASSERT_EQ(cells.Cols(), 1);
    for (int row = 0; row < 3; ++row)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 0)), result->val_.Steps()[row]);
    DupireScriptCurvatureResult_Get_Shape(result, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_EQ(cells.Cols(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 3.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 6.0);
    DupireScriptCurvatureResult_Get_Execution(result, &cells);
    ASSERT_EQ(cells.Rows(), 4);
    ASSERT_EQ(cells.Cols(), 2);
    ASSERT_EQ(Cell::ToString(cells(0, 0)), "method");
    ASSERT_EQ(Cell::ToString(cells(0, 1)), "BumpOverRecalibratedNativeDupireMonteCarloAAD");
    ASSERT_EQ(Cell::ToString(cells(1, 0)), "quote_gradient_evaluations");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 1)), 7.0);
    ASSERT_EQ(Cell::ToString(cells(2, 0)), "paths_per_evaluation");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(2, 1)), 17.0);
    ASSERT_EQ(Cell::ToString(cells(3, 0)), "numeric_payload_bytes");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(3, 1)), 720.0);
    ASSERT_EQ(result->Type(), "DupireScriptCurvatureResult");
}

TEST(ExcelDupireCurvatureTest, TestZeroDirectionsKeepLogicalAxisAndEvaluateBaseOnce) {
    const SingleWorker_ workers;
    auto native = Request()->val_;
    Matrix_<Cell_> settings(1, 2);
    settings(0, 0) = "input_count";
    settings(0, 1) = 6.0;
    Handle_<StorableBumpOverAADRequest_> bumps;
    BumpOverAADRequest_New("empty", {}, {}, settings, &bumps);
    native.bumps_ = bumps->val_;
    const auto plan = Plan(CopyRequest(native));
    ASSERT_EQ(plan->val_.NumericPayloadBytes(), 408);
    Matrix_<Cell_> cells;
    DupireScriptCurvaturePlan_Get_Directions(plan, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_EQ(cells.Cols(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    DupireScriptCurvaturePlan_Get_Shape(plan, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 6.0);
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("empty", plan, &result);
    ASSERT_EQ(result->val_.Execution().quoteGradientEvaluations_, 1);
    ASSERT_EQ(result->val_.Execution().numericPayloadBytes_, 408);
    ASSERT_EQ(result->val_.Gradient().size(), 6);
    DupireScriptCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_EQ(cells.Cols(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    DupireScriptCurvatureResult_Get_Steps(result, &cells);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    DupireScriptCurvatureResult_Get_Shape(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 6.0);
    ASSERT_NEAR(result->val_.Gradient()[3], 0.002 * std::exp(-0.05), 1e-12);
}

TEST(ExcelDupireCurvatureTest, TestSelectedReportedBaseRetainsFullRawQuoteProducts) {
    const SingleWorker_ workers;
    auto request = Request()->val_;
    request.risk_.quotes_.inputs_ = Vector_<String_>{"quote:3", "quote:0"};
    request.risk_.quotes_.reportFactors_ = Vector_<>{0.01, 0.5};
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("selected", Plan(CopyRequest(request)), &result);
    Handle_<StorableDupireScriptRiskResult_> base;
    DupireScriptCurvatureResult_Get_Base(result, &base);
    ASSERT_EQ(base->val_.QuoteRisk().Plan().InputAxis().size(), 2);
    ASSERT_DOUBLE_EQ(base->val_.QuoteRisk().ReportedJacobian()(0, 0), 0.01 * result->val_.Gradient()[3]);
    Handle_<StorableCalibrationRiskPlan_> quotes;
    DupireScriptCurvatureResult_Get_QuotePlan(result, &quotes);
    ASSERT_EQ(quotes->val_.InputAxis().size(), 2);
    ASSERT_EQ(quotes->val_.CompleteInputAxis().size(), 6);
    Matrix_<Cell_> cells;
    DupireScriptCurvatureResult_Get_Gradient(result, &cells);
    ASSERT_EQ(cells.Rows(), 6);
    ASSERT_NEAR(Cell::ToDouble(cells(3, 0)), 0.002 * std::exp(-0.05), 1e-12);
    DupireScriptCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 3);
    ASSERT_EQ(cells.Cols(), 6);
    ASSERT_NEAR(Cell::ToDouble(cells(1, 3)), -4.0 * std::exp(-0.05), 1e-10);
}

TEST(ExcelDupireCurvatureTest, TestCombinedAndFirstOrderBudgetsRemainSeparateAndExact) {
    const SingleWorker_ workers;
    auto request = Request()->val_;
    request.risk_.quotes_.numericPayloadBudgetBytes_ = 304;
    request.bumps_.numericPayloadBudgetBytes_ = 720;
    const auto plan = Plan(CopyRequest(request));
    ASSERT_EQ(plan->val_.BasePlan().NumericPayloadBytes(), 304);
    ASSERT_EQ(plan->val_.NumericPayloadBytes(), 720);
    for (const size_t cap : {size_t(0), size_t(719)}) {
        request.bumps_.numericPayloadBudgetBytes_ = cap;
        ASSERT_THROW(static_cast<void>(Plan(CopyRequest(request))), Exception_);
    }
    request.bumps_.numericPayloadBudgetBytes_ = 720;
    request.risk_.quotes_.numericPayloadBudgetBytes_ = 303;
    ASSERT_THROW(static_cast<void>(Plan(CopyRequest(request))), Exception_);
    request.risk_.quotes_.numericPayloadBudgetBytes_ = 304;
    request.bumps_.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(Plan(CopyRequest(request))), Exception_);
    request.bumps_.recordingCapacityBudgetBytes_.reset();
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("recovered", Plan(CopyRequest(request)), &result);
    ASSERT_EQ(result->val_.Execution().numericPayloadBytes_, 720);
}

TEST(ExcelDupireCurvatureTest, TestMixedSurfaceAndDirectProductMatchesRecalibratedFirstOrderGradients) {
    const SingleWorker_ workers;
    constexpr double step = 0.0002;
    const Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
    auto request = Request()->val_;
    request.bumps_.directions_ = Matrix_<>(1, 6);
    std::copy(direction.begin(), direction.end(), request.bumps_.directions_.Data());
    request.bumps_.steps_ = {step};
    const auto calibration = Calibration();
    auto gradient = [&](double sign) {
        auto spreads = calibration->val_.Inputs().quoteSpreads_;
        for (int quote = 0; quote < 6; ++quote)
            spreads.Data()[quote] += sign * step * direction[quote];
        const auto bumped = RecalibrateDupireWithRisk(calibration->val_, spreads);
        const auto model = NewDupireModelData("bumped", bumped, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25);
        std::ostringstream constant;
        constant << std::setprecision(17) << spreads(1, 1);
        const auto product = NewScriptProduct("mixed", {Cell_("QUOTE"), Cell_(Date_(2027, 9, 12))},
                                              {String_(constant.str()), "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) + QUOTE * QUOTE"});
        const auto first = ValueByMonteCarloWithDupireRisk(PlanDupireScriptRisk(product, model, bumped, "equity", request.risk_));
        const auto& raw = first.QuoteRisk().QuoteRisk().TotalAdjoints();
        return Vector_<>(raw.begin(), raw.end());
    };
    for (const bool compiled : {false, true}) {
        request.risk_.simulation_.compiled_ = compiled;
        const auto product = NewScriptProduct("mixed", {Cell_("QUOTE"), Cell_(Date_(2027, 9, 12))},
                                              {"0.001", "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) + QUOTE * QUOTE"});
        Handle_<StorableDupireScriptCurvatureResult_> result;
        DupireScriptCurvatureResult_New("mixed", Plan(CopyRequest(request), product), &result);
        const auto plus = gradient(1.0);
        const auto minus = gradient(-1.0);
        Matrix_<Cell_> products;
        DupireScriptCurvatureResult_Get_HessianProducts(result, &products);
        ASSERT_EQ(products.Rows(), 1);
        ASSERT_EQ(products.Cols(), 6);
        for (int quote = 0; quote < 6; ++quote)
            ASSERT_NEAR(Cell::ToDouble(products(0, quote)), (plus[quote] - minus[quote]) / (2.0 * step), 1e-8);
    }
}

TEST(ExcelDupireCurvatureTest, TestNullQueriesPreserveOutputsAndArchivesRejectExplicitly) {
    const SingleWorker_ workers;
    const auto request = Request();
    const auto plan = Plan(request);
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("result", plan, &result);
    const auto previous = result;
    ASSERT_THROW(DupireScriptCurvatureResult_New("failure", {}, &result), Exception_);
    ASSERT_EQ(result, previous);
    const String_ nul(std::string("bad\0name", 8));
    ASSERT_THROW(DupireScriptCurvatureResult_New(nul, plan, &result), Exception_);
    ASSERT_EQ(result, previous);
    Matrix_<Cell_> cells(1, 1, Cell_(42.0));
    const auto planGetters = {DupireScriptCurvaturePlan_Get_Point, DupireScriptCurvaturePlan_Get_Directions, DupireScriptCurvaturePlan_Get_Steps,
                              DupireScriptCurvaturePlan_Get_Shape};
    for (const auto getter : planGetters) {
        ASSERT_THROW(getter({}, &cells), Exception_);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 42.0);
    }
    const auto resultGetters = {DupireScriptCurvatureResult_Get_Point,           DupireScriptCurvatureResult_Get_Gradient,
                                DupireScriptCurvatureResult_Get_Directions,      DupireScriptCurvatureResult_Get_Steps,
                                DupireScriptCurvatureResult_Get_HessianProducts, DupireScriptCurvatureResult_Get_Shape,
                                DupireScriptCurvatureResult_Get_Execution};
    for (const auto getter : resultGetters) {
        ASSERT_THROW(getter({}, &cells), Exception_);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 42.0);
    }
    double bytes = 42.0;
    ASSERT_THROW(DupireScriptCurvaturePlan_Get_Payload({}, &bytes), Exception_);
    ASSERT_DOUBLE_EQ(bytes, 42.0);
    Handle_<StorableDupireScriptRiskRequest_> first;
    Handle_<StorableBumpOverAADRequest_> bumps;
    Handle_<StorableDupireScriptRiskPlan_> basePlan;
    Handle_<StorableDupireScriptRiskResult_> base;
    Handle_<StorableCalibrationRiskPlan_> quotes;
    ASSERT_THROW(DupireScriptCurvatureRequest_Get_Risk({}, &first), Exception_);
    ASSERT_THROW(DupireScriptCurvatureRequest_Get_Bumps({}, &bumps), Exception_);
    ASSERT_THROW(DupireScriptCurvaturePlan_Get_BasePlan({}, &basePlan), Exception_);
    ASSERT_THROW(DupireScriptCurvatureResult_Get_Base({}, &base), Exception_);
    ASSERT_THROW(DupireScriptCurvatureResult_Get_QuotePlan({}, &quotes), Exception_);
    for (const Handle_<Storable_>& handle : {Handle_<Storable_>(request), Handle_<Storable_>(plan), Handle_<Storable_>(result)}) {
        try {
            static_cast<void>(JSON::WriteString(*handle));
            FAIL() << "expected unsupported curvature archive";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("RiskArchiveUnsupported"), std::string::npos);
        }
    }
    DupireScriptCurvatureResult_New("recovered", plan, &result);
    ASSERT_NEAR(result->val_.HessianProducts()(0, 3), 2.0 * std::exp(-0.05), 1e-10);
}

TEST(ExcelDupireCurvatureTest, TestMalformedPlansRejectBeforeFixingReadsAndWorkerSubmission) {
    const SingleWorker_ workers;
    const auto calibration = Calibration();
    const auto model = NewDupireModelData("model", calibration->val_, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25);
    const auto product = NewScriptProduct("history", {Cell_("QUOTE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2027, 9, 12))},
                                          {"0.001", "past = FIX(EQ[CURVATURE_REJECT])", "pay PAYS past + QUOTE * QUOTE"});
    auto output = Plan(Request());
    const auto previous = output;
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ submissions;
    const ObserveWork_ observe(&history, &submissions);
    auto reject = [&](const DupireScriptCurvatureRequest_& request) {
        ASSERT_THROW(DupireScriptCurvaturePlan_New("invalid", product, model, calibration, "equity", CopyRequest(request), &output), Exception_);
        ASSERT_EQ(output, previous);
        ASSERT_EQ(history.historyCalls_, 0);
        ASSERT_EQ(history.fixingCalls_, 0);
        ASSERT_EQ(submissions.calls_, 0);
    };
    const auto valid = Request()->val_;
    auto invalid = valid;
    invalid.bumps_.directions_ = Matrix_<>(1, 5, 1.0);
    invalid.bumps_.steps_ = {0.0002};
    ASSERT_NO_FATAL_FAILURE(reject(invalid));
    invalid = valid;
    invalid.bumps_.directions_ = Matrix_<>(3, 6, 0.0);
    ASSERT_NO_FATAL_FAILURE(reject(invalid));
    invalid = valid;
    invalid.bumps_.steps_[0] = 1.0;
    ASSERT_NO_FATAL_FAILURE(reject(invalid));
    invalid = valid;
    invalid.bumps_.recordingCapacityBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(reject(invalid));
    invalid = valid;
    invalid.risk_.direct_ = NewCalibrationDirectQuoteAdjoints(NewCalibrationPullback(calibration->val_), Matrix_<>(3, 2, 0.0));
    ASSERT_NO_FATAL_FAILURE(reject(invalid));
    ASSERT_THROW(DupireScriptCurvaturePlan_New("null", {}, model, calibration, "equity", Request(), &output), Exception_);
    ASSERT_THROW(DupireScriptCurvaturePlan_New("null", product, {}, calibration, "equity", Request(), &output), Exception_);
    ASSERT_THROW(DupireScriptCurvaturePlan_New("null", product, model, {}, "equity", Request(), &output), Exception_);
    ASSERT_THROW(DupireScriptCurvaturePlan_New("null", product, model, calibration, "equity", {}, &output), Exception_);
    ASSERT_EQ(output, previous);
}

TEST(ExcelDupireCurvatureTest, TestWorkerFailuresPreserveResultAndAllowDeterministicRecovery) {
    const SingleWorker_ workers;
    const auto plan = Plan(Request());
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("first", plan, &result);
    const auto original = result;
#ifndef _WIN32
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
#endif
    for (const size_t phase : {size_t(1), size_t(2), size_t(7)}) {
        {
            RejectAtSubmission_ failure(phase);
            const ObserveWork_ observe(nullptr, &failure);
            try {
                DupireScriptCurvatureResult_New("failure", plan, &result);
                FAIL() << "expected injected worker failure";
            } catch (const Exception_& error) {
                const std::string message(error.what());
                ASSERT_NE(message.find("DupireScriptCurvature: execution"), std::string::npos);
                ASSERT_NE(message.find("injected Excel curvature submission failure"), std::string::npos);
                ASSERT_NE(message.find(phase == 1 ? "base" : phase == 2 ? "plus" : "minus"), std::string::npos);
            }
            ASSERT_EQ(failure.calls_, phase);
        }
        ASSERT_EQ(result, original);
#ifndef _WIN32
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 4);
#endif
        Handle_<StorableDupireScriptCurvatureResult_> recovered;
        DupireScriptCurvatureResult_New("recovered", plan, &recovered);
        for (int row = 0; row < 3; ++row)
            for (int quote = 0; quote < 6; ++quote)
                ASSERT_DOUBLE_EQ(recovered->val_.HessianProducts()(row, quote), original->val_.HessianProducts()(row, quote));
    }
}

TEST(ExcelDupireCurvatureTest, TestPassiveQueriesAndRejectedNestedWorkPreserveCallerGraphAndSeed) {
    const SingleWorker_ workers;
    const auto request = Request();
    const auto calibration = Calibration();
    const auto model = NewDupireModelData("model", calibration->val_, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25);
    const auto product = NewScriptProduct("quadratic", {Cell_("QUOTE"), Cell_(Date_(2027, 9, 12))}, {"0.001", "pay PAYS QUOTE * QUOTE"});
    auto plan = Plan(request);
    const auto originalPlan = plan;
    Handle_<StorableDupireScriptCurvatureResult_> result;
    DupireScriptCurvatureResult_New("result", plan, &result);
    const auto originalResult = result;
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ submissions;
    const ObserveWork_ observe(&history, &submissions);
#ifdef _WIN32
    double adjoint = 0.0;
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
            const auto copied = CopyRequest(request->val_);
            Handle_<StorableDupireScriptRiskRequest_> first;
            Handle_<StorableBumpOverAADRequest_> bumps;
            Handle_<StorableDupireScriptRiskPlan_> basePlan;
            Handle_<StorableDupireScriptRiskResult_> base;
            Handle_<StorableCalibrationRiskPlan_> quotes;
            DupireScriptCurvatureRequest_Get_Risk(copied, &first);
            DupireScriptCurvatureRequest_Get_Bumps(copied, &bumps);
            DupireScriptCurvaturePlan_Get_BasePlan(plan, &basePlan);
            DupireScriptCurvatureResult_Get_Base(result, &base);
            DupireScriptCurvatureResult_Get_QuotePlan(result, &quotes);
            Matrix_<Cell_> cells;
            for (const auto getter : {DupireScriptCurvaturePlan_Get_Point, DupireScriptCurvaturePlan_Get_Directions,
                                      DupireScriptCurvaturePlan_Get_Steps, DupireScriptCurvaturePlan_Get_Shape})
                getter(plan, &cells);
            double bytes = 0.0;
            DupireScriptCurvaturePlan_Get_Payload(plan, &bytes);
            for (const auto getter :
                 {DupireScriptCurvatureResult_Get_Point, DupireScriptCurvatureResult_Get_Gradient, DupireScriptCurvatureResult_Get_Directions,
                  DupireScriptCurvatureResult_Get_Steps, DupireScriptCurvatureResult_Get_HessianProducts, DupireScriptCurvatureResult_Get_Shape,
                  DupireScriptCurvatureResult_Get_Execution})
                getter(result, &cells);
            ASSERT_THROW(DupireScriptCurvaturePlan_New("nested", product, model, calibration, "equity", copied, &plan), Exception_);
            ASSERT_THROW(DupireScriptCurvatureResult_New("nested", plan, &result), Exception_);
            ASSERT_EQ(plan, originalPlan);
            ASSERT_EQ(result, originalResult);
            ASSERT_EQ(history.historyCalls_, 0);
            ASSERT_EQ(history.fixingCalls_, 0);
            ASSERT_EQ(submissions.calls_, 0);
#ifdef _WIN32
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodes);
        },
        false, &adjoint);
    ASSERT_DOUBLE_EQ(adjoint, 4.0);
#else
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3);
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(square, 2), 7.0);
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 28.0);
    recording.Close();
#endif
}
