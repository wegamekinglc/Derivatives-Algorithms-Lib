//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal-cpp/benchmarks/rate_risk_perf/quoteriskbenchfixtures.hpp>
#include <dal-excel/src/__riskrequest.hpp>
#include <dal-excel/src/__script_test_api.hpp>

#include <script_test_observers.hpp>
#include <tests/curve/jointxccyquoteriskfixtures.hpp>

#include <dal-excel/src/__riskrequestinput.hpp>

using namespace Dal;

namespace {
    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    auto Calibration() {
        const DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
        return CalibrateDupireWithRisk(FlatIVS_(), inputs);
    }

    auto Boundary() {
        return Handle_<StorableCalibrationPullback_>(new StorableCalibrationPullback_("source", NewCalibrationPullback(Calibration())));
    }

    Matrix_<Cell_> Row(const String_& key, const Cell_& value) {
        Matrix_<Cell_> result(1, 2);
        result(0, 0) = key;
        result(0, 1) = value;
        return result;
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

    template <class M_, class F_> CalibrationPullback_ CapturedSource(const M_& materials, F_ build) {
        auto config = materials.config_;
        config.retainCalibrationRecord_ = true;
        return NewCalibrationPullback(build(materials.spec_, *materials.calibration_, materials.options_, materials.market_, config));
    }

    auto JointSource(CurveJacobianMode_ mode) {
        const auto spec = JointQuoteRiskFixtures::Spec(8, 2, CurveParameterization_::Value_::LOG_DISCOUNT, true);
        const auto options = JointXccyQuoteRiskFixtures::Options(mode);
        const auto calibrated = CalibrateJointMultiCurve(spec, options);
        auto config = JointQuoteRiskFixtures::Config(2);
        config.retainCalibrationRecord_ = true;
        return NewCalibrationPullback(
            BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, JointQuoteRiskFixtures::Market(spec, calibrated), config));
    }
} // namespace

TEST(ExcelCalibrationRequestTest, TestOwningQuotePlanAndReportedProjectionMatchNative) {
    Excel::ScriptTestInitialize(1);
    const auto boundary = NewCalibrationPullback(Calibration());
    const Handle_<StorableCalibrationPullback_> source(new StorableCalibrationPullback_("source", boundary));
    Matrix_<Cell_> settings(3, 2);
    settings(0, 0) = "inputs";
    settings(0, 1) = "quote:3;quote:0";
    settings(1, 0) = "report_factors";
    settings(1, 1) = "0.01;0.5";
    settings(2, 0) = "numeric_payload_budget_bytes";
    settings(2, 1) = 144.0;
    Handle_<StorableCalibrationRiskRequest_> request;
    CalibrationRiskRequest_New("request", settings, &request);
    Handle_<StorableCalibrationRiskPlan_> plan;
    CalibrationRiskPlan_New("plan", source, request, &plan);
    ASSERT_EQ(plan->val_.SelectedOrdinals(), (Vector_<size_t>{3, 0}));
    ASSERT_EQ(plan->val_.NumericPayloadBytes(), 144);
    const auto seeds = NewCalibrationParameterAdjoints(boundary, Matrix_<>(9, 2, -0.25));
    const Handle_<StorableCalibrationParameterAdjoints_> parameters(new StorableCalibrationParameterAdjoints_("parameters", seeds));
    Handle_<StorableCalibrationRiskResult_> result;
    CalibrationRiskResult_New("result", plan, parameters, {}, &result);
    const auto native = PullbackCalibration(boundary, seeds);
    Matrix_<Cell_> reported;
    CalibrationRiskResult_Get_Jacobian(result, "Reported", &reported);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(0, 0)), native.TotalAdjoints()(1, 1) * 0.01);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(0, 1)), native.TotalAdjoints()(0, 0) * 0.5);
}

TEST(ExcelCalibrationRequestTest, TestSettingsRoundTripPreservesAbsentEmptyAndExactFactors) {
    Excel::ScriptTestInitialize(1);
    const auto source = Boundary();
    for (const auto& cells : {Matrix_<Cell_>(), Matrix_<Cell_>(1, 1), Row("inputs", Cell_())}) {
        Handle_<StorableCalibrationRiskRequest_> request, recovered;
        CalibrationRiskRequest_New("request", cells, &request);
        Matrix_<Cell_> settings;
        CalibrationRiskRequest_Get_Settings(request, &settings);
        CalibrationRiskRequest_New("copy", settings, &recovered);
        ASSERT_EQ(recovered->val_.inputs_, request->val_.inputs_);
        Handle_<StorableCalibrationRiskPlan_> plan;
        CalibrationRiskPlan_New("plan", source, recovered, &plan);
        ASSERT_EQ(plan->val_.InputAxis().size(), request->val_.inputs_ ? 0 : 6);
        ASSERT_EQ(plan->val_.NumericPayloadBytes(), 144);
    }
    const auto factors = Row("report_factors", Cell_("0.12345678901234567;0.0001;1;0.5;2;1e-50"));
    Handle_<StorableCalibrationRiskRequest_> request, recovered;
    CalibrationRiskRequest_New("factors", factors, &request);
    Matrix_<Cell_> cells;
    CalibrationRiskRequest_Get_Settings(request, &cells);
    CalibrationRiskRequest_New("copy", cells, &recovered);
    ASSERT_EQ(*recovered->val_.reportFactors_, *request->val_.reportFactors_);
    cells(0, 1) = "7";
    ASSERT_EQ(request->val_.reportFactors_->size(), 6);
}

TEST(ExcelCalibrationRequestTest, TestMalformedSettingsAndNativePlanFailuresPreserveHandles) {
    Excel::ScriptTestInitialize(1);
    Handle_<StorableCalibrationRiskRequest_> request;
    CalibrationRiskRequest_New("valid", {}, &request);
    const auto saved = request;
    for (const auto& bad :
         {Row("outputs", Cell_("payoff")), Row("inputs", Cell_(true)), Row("inputs", Cell_(";quote:0")), Row("report_factors", Cell_("0")),
          Row("report_factors", Cell_("nan")), Row("numeric_payload_budget_bytes", Cell_(true)), Row("numeric_payload_budget_bytes", Cell_(1.5)),
          Row("numeric_payload_budget_bytes", Cell_(-1.0)), Row("numeric_payload_budget_bytes", Cell_(9007199254740992.0)), Matrix_<Cell_>(1, 3)}) {
        ASSERT_THROW(CalibrationRiskRequest_New("bad", bad, &request), Exception_);
        ASSERT_EQ(request.get(), saved.get());
    }
    Matrix_<Cell_> duplicate(2, 2);
    duplicate(0, 0) = "inputs";
    duplicate(0, 1) = "quote:0";
    duplicate(1, 0) = "INPUTS";
    duplicate(1, 1) = "quote:1";
    ASSERT_THROW(CalibrationRiskRequest_New("duplicate", duplicate, &request), Exception_);
    const auto boundary = Boundary();
    Handle_<StorableCalibrationRiskPlan_> plan;
    CalibrationRiskPlan_New("valid", boundary, {}, &plan);
    const auto oldPlan = plan;
    for (const auto& cells : {Row("inputs", Cell_("quote:99")), Row("inputs", Cell_("quote:0;quote:0")), Row("report_factors", Cell_("1")),
                              Row("numeric_payload_budget_bytes", Cell_(143.0))}) {
        CalibrationRiskRequest_New("invalid", cells, &request);
        ASSERT_THROW(CalibrationRiskPlan_New("bad", boundary, request, &plan), Exception_);
        ASSERT_EQ(plan.get(), oldPlan.get());
    }
    ASSERT_THROW(CalibrationRiskPlan_New("null", {}, {}, &plan), Exception_);
    ASSERT_EQ(plan.get(), oldPlan.get());
    CalibrationRiskPlan_New("recovered", boundary, {}, &plan);
    ASSERT_EQ(plan->val_.InputAxis().size(), 6);
}

TEST(ExcelCalibrationRequestTest, TestEmptySelectionRetainsRawRisksAndExactShape) {
    Excel::ScriptTestInitialize(1);
    const auto boundary = Boundary();
    Handle_<StorableCalibrationRiskRequest_> request;
    CalibrationRiskRequest_New("empty", Row("inputs", Cell_()), &request);
    Handle_<StorableCalibrationRiskPlan_> plan;
    CalibrationRiskPlan_New("plan", boundary, request, &plan);
    Handle_<StorableCalibrationParameterAdjoints_> seeds;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, -0.25), &seeds);
    Handle_<StorableCalibrationRiskResult_> result;
    CalibrationRiskResult_New("result", plan, seeds, {}, &result);
    for (const auto& projection : {"Total", "Calibration", "Direct", "Reported"}) {
        Matrix_<Cell_> cells;
        CalibrationRiskResult_Get_Jacobian(result, projection, &cells);
        ASSERT_EQ(cells.Rows(), 1);
        ASSERT_EQ(cells.Cols(), 1);
        ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    }
    Handle_<StorableCalibrationQuoteRisk_> raw;
    CalibrationRiskResult_Get_QuoteRisk(result, &raw);
    ASSERT_EQ(raw->val_.TotalAdjoints().Rows(), 3);
    ASSERT_EQ(raw->val_.TotalAdjoints().Cols(), 2);
    Matrix_<Cell_> shape;
    CalibrationRiskPlan_Get_Shape(plan, &shape);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(1, 1)), 0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(4, 1)), 144);
}

TEST(ExcelCalibrationRequestTest, TestPassiveDetachedGettersPreserveUnrelatedRecordingAndNullOutputs) {
    Excel::ScriptTestInitialize(1);
    const auto boundary = Boundary();
    Handle_<StorableCalibrationRiskPlan_> plan;
    CalibrationRiskPlan_New("plan", boundary, {}, &plan);
    Handle_<StorableCalibrationParameterAdjoints_> seeds;
    CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(9, 2, -0.25), &seeds);
    Handle_<StorableCalibrationRiskResult_> result;
    CalibrationRiskResult_New("result", plan, seeds, {}, &result);
    const RejectGetterWork_ reject;
    double adjoint = 0;
    Excel::ScriptTestWithRecording(
        [&](size_t before) {
            Matrix_<Cell_> cells;
            CalibrationRiskPlan_Get_Inputs(plan, true, &cells);
            ASSERT_EQ(cells.Rows(), 7);
            ASSERT_EQ(cells.Cols(), 12);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 8)), 75);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 9)), 0.4);
            ASSERT_TRUE(Cell::IsEmpty(cells(1, 10)));
            cells(1, 0) = "changed";
            CalibrationRiskPlan_Get_Inputs(plan, false, &cells);
            ASSERT_EQ(Cell::ToString(cells(1, 0)), "quote:0");
            Handle_<StorableCalibrationPullback_> copy;
            CalibrationRiskPlan_Get_Calibration(plan, &copy);
            ASSERT_EQ(copy->val_.QuoteRows(), 3);
            const auto savedPlan = plan;
            CalibrationRiskResult_Get_Plan(result, &plan);
            ASSERT_EQ(plan->val_.SelectedOrdinals(), savedPlan->val_.SelectedOrdinals());
            const auto savedCells = cells;
            ASSERT_THROW(CalibrationRiskPlan_Get_Inputs({}, false, &cells), Exception_);
            ASSERT_EQ(Cell::ToString(cells(1, 0)), Cell::ToString(savedCells(1, 0)));
            ASSERT_THROW(CalibrationRiskResult_Get_Jacobian(result, "bad", &cells), Exception_);
            ASSERT_EQ(Cell::ToString(cells(1, 0)), "quote:0");
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), before);
        },
        false, &adjoint);
    ASSERT_DOUBLE_EQ(adjoint, 4.0);
}

TEST(ExcelCalibrationRequestTest, TestAllCapturedCurveKindsRetainNativeAxesAndDv01Projections) {
    using namespace Dal::RateRiskPerf;
    Excel::ScriptTestInitialize(1);
    for (const auto mode : {CurveJacobianMode_::Value_::ANALYTIC, CurveJacobianMode_::Value_::BUMPED}) {
        const Vector_<CalibrationPullback_> sources{
            CapturedSource(MakeSingleCurveProvenanceMaterials(8, mode),
                           [](const auto&... args) { return BuildSingleCurveQuoteRiskProvenance(args...); }),
            CapturedSource(MakeJointXccyProvenanceMaterials(8, mode), BuildJointXccyQuoteRiskProvenance),
            CapturedSource(MakeStagedXccyProvenanceMaterials(8, mode), BuildStagedXccyBasisQuoteRiskProvenance), JointSource(mode)};
        for (const auto& source : sources) {
            const Handle_<StorableCalibrationPullback_> boundary(new StorableCalibrationPullback_("curve", source));
            const auto nativePlan = PlanCalibrationRiskRequest(source);
            Handle_<StorableCalibrationRiskRequest_> request;
            Matrix_<Cell_> settings(2, 2);
            settings(0, 0) = "inputs";
            settings(0, 1) = nativePlan.InputAxis().back().id_ + ";quote:0";
            settings(1, 0) = "report_factors";
            settings(1, 1) = "0.0001;0.0001";
            CalibrationRiskRequest_New("dv01", settings, &request);
            Handle_<StorableCalibrationRiskPlan_> plan;
            CalibrationRiskPlan_New("plan", boundary, request, &plan);
            Matrix_<Cell_> cells;
            CalibrationRiskPlan_Get_Inputs(plan, true, &cells);
            for (int row = 1; row < cells.Rows(); ++row) {
                const auto& coordinate = nativePlan.InputAxis()[row - 1];
                ASSERT_EQ(Cell::ToString(cells(row, 0)), coordinate.id_);
                ASSERT_EQ(Cell::ToString(cells(row, 1)), coordinate.label_);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 2)), double(coordinate.ordinal_));
                ASSERT_EQ(Cell::ToString(cells(row, 5)), coordinate.nativeUnit_);
                ASSERT_TRUE(Cell::IsEmpty(cells(row, 7)));
                ASSERT_TRUE(Cell::IsEmpty(cells(row, 8)));
                ASSERT_TRUE(Cell::IsEmpty(cells(row, 9)));
                ASSERT_EQ(Cell::ToString(cells(row, 10)), *coordinate.blockKey_);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 11)), *coordinate.blockOrdinal_);
            }
            Handle_<StorableCalibrationParameterAdjoints_> parameters;
            CalibrationParameterAdjoints_New("parameters", boundary, Matrix_<>(source.ParameterRows(), 1, -0.25), &parameters);
            Handle_<StorableCalibrationDirectQuoteAdjoints_> direct;
            CalibrationDirectQuoteAdjoints_New("direct", boundary, Matrix_<>(source.QuoteRows(), 1, -1.25), &direct);
            Handle_<StorableCalibrationRiskResult_> result;
            CalibrationRiskResult_New("result", plan, parameters, direct, &result);
            const auto native = PullbackCalibration(source, parameters->val_, direct->val_);
            CalibrationRiskResult_Get_Jacobian(result, "Reported", &cells);
            for (int column = 0; column < cells.Cols(); ++column) {
                const int row = static_cast<int>(plan->val_.SelectedOrdinals()[column]);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, column)), native.TotalAdjoints()(row, 0) * 1e-4);
            }
        }
    }
}

#ifdef _WIN32
TEST(ExcelCalibrationRequestTest, TestWorksheetSettingsRejectErrorsShapesAndEmbeddedNul) {
    wchar_t key[]{6, L'i', L'n', L'p', L'u', L't', L's'};
    wchar_t value[]{7, L'q', L'u', L'o', L't', L'e', L':', L'0'};
    OPER_ cells[2]{};
    cells[0].xltype = xltypeStr;
    cells[0].val.str = key;
    cells[1].xltype = xltypeStr;
    cells[1].val.str = value;
    OPER_ input{};
    input.xltype = xltypeMulti;
    input.val.array = {cells, 1, 2};
    ASSERT_NO_THROW(Excel::ValidateRiskRequestSettings(&input, "CalibrationRiskRequest_New"));
    value[3] = L'\0';
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "CalibrationRiskRequest_New"), Exception_);
    cells[1].xltype = xltypeErr;
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "CalibrationRiskRequest_New"), Exception_);
    input.val.array = {nullptr, (std::numeric_limits<int>::max)(), 3};
    ASSERT_THROW(Excel::ValidateRiskRequestSettings(&input, "CalibrationRiskRequest_New"), Exception_);
}
#endif
