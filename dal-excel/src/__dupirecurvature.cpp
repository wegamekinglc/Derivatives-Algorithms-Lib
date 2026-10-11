//
// Created by Codex on 2026/10/11.
//

#include "__dupirecurvature.hpp"

#include "__curvatureinput.hpp"
#include "__curvaturerows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureRequest_New
    Create a passive recalibrated Dupire quote curvature request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvatureRequest_New; name");
risk is handle StorableDupireScriptRiskRequest
    Existing first-order settings and optional direct scalar quote bindings
bumps is handle StorableBumpOverAADRequest
    Full raw decimal-vol quote directions and positive steps; worker recording caps are unsupported
&outputs
request is handle StorableDupireScriptCurvatureRequest
    Detached passive copy; financial admission occurs in planning
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureRequest_Get_Risk
    Copy the first-order request retained by Dupire curvature
&inputs
request is handle StorableDupireScriptCurvatureRequest
    Passive curvature request
&outputs
risk is handle StorableDupireScriptRiskRequest
    Detached first-order request, including quote selection and scalar bindings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureRequest_Get_Bumps
    Copy directions, steps and native budgets retained by Dupire curvature
&inputs
request is handle StorableDupireScriptCurvatureRequest
    Passive curvature request
&outputs
bumps is handle StorableBumpOverAADRequest
    Detached common finite-step request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_New
    Admit and freeze a common-path recalibrated Dupire curvature plan
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvaturePlan_New; name");
+argName = "component"; Excel::ValidateRiskRequestText(xl_component, "DupireScriptCurvaturePlan_New; component");
product is handle ScriptProductData
    European script product with rebuildable scalar quote bindings
modelData is handle ModelData
    Matching native Hybrid model
calibration is handle StorableDupireCalibration
    Frozen Dupire calibration and full strike-major quote order
component is string
    Native local-vol component name
request is handle StorableDupireScriptCurvatureRequest
    Copied first-order settings and full raw quote direction request
&outputs
plan is handle StorableDupireScriptCurvaturePlan
    Owning admitted plan; no valuation paths are submitted during planning
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_New
    Evaluate finite-step Dupire quote curvature through full recalibration
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvatureResult_New; name");
plan is handle StorableDupireScriptCurvaturePlan
    Frozen common-path plan; active outer recordings reject
&outputs
result is handle StorableDupireScriptCurvatureResult
    Owning base value, full raw gradient, signed products and work counters
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Gradient
    Copy the complete raw decimal-vol quote gradient
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
gradient is cell[][]
    Full quote-order column; selected first-order report factors do not apply
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_HessianProducts
    Copy signed finite-step Dupire quote Hessian products
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
products is cell[][]
    Direction rows and complete raw quote columns; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_BasePlan
    Copy the frozen first-order plan retained by Dupire curvature
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
basePlan is handle StorableDupireScriptRiskPlan
    Immutable first-order plan with frozen date, history and simulation settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_Point
    Copy the frozen full raw Dupire quote point
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
point is cell[][]
    Full strike-major decimal-vol spread quote column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_Directions
    Copy admitted Dupire quote direction rows
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
directions is cell[][]
    Full raw quote columns; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_Steps
    Copy admitted Dupire finite-step sizes
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
steps is cell[][]
    One positive step per direction as a column; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_Shape
    Copy logical Dupire curvature direction and quote counts
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
shape is cell[][]
    One row containing direction count then complete raw quote count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvaturePlan_Get_Payload
    Copy admitted native Dupire curvature numeric bytes
&inputs
plan is handle StorableDupireScriptCurvaturePlan
    Admitted curvature plan
&outputs
payload is number
    Native retained numeric bytes; worksheet handles, cells, copies and worker memory are excluded
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Base
    Copy the selected first-order valuation retained by Dupire curvature
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
base is handle StorableDupireScriptRiskResult
    Detached first-order result with its selected quote report factors
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_QuotePlan
    Copy the calibration quote plan retained by Dupire curvature
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
quotePlan is handle StorableCalibrationRiskPlan
    Use CALIBRATIONRISKPLAN.GET.INPUTS with complete=true for full raw quote metadata
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Point
    Copy the full raw quote point retained by Dupire curvature
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
point is cell[][]
    Full strike-major decimal-vol spread quote column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Directions
    Copy completed Dupire quote direction rows
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
directions is cell[][]
    Full raw quote columns; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Steps
    Copy completed Dupire finite-step sizes
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
steps is cell[][]
    One positive step per direction as a column; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Shape
    Copy logical Dupire product direction and quote counts
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
shape is cell[][]
    One row containing direction count then complete raw quote count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptCurvatureResult_Get_Execution
    Copy completed recalibrated Dupire curvature work counters
&inputs
result is handle StorableDupireScriptCurvatureResult
    Completed owning result
&outputs
execution is cell[][]
    Method, quote gradient evaluation count, paths per evaluation and native numeric bytes
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    void DupireScriptCurvatureRequest_New(const String_& name,
                                          const Handle_<StorableDupireScriptRiskRequest_>& risk,
                                          const Handle_<StorableBumpOverAADRequest_>& bumps,
                                          Handle_<StorableDupireScriptCurvatureRequest_>* request) {
        Excel::CheckCurvatureText(name, "DupireScriptCurvatureRequest_New; name");
        const auto& first = Excel::CheckedRequestValue(risk, "DupireScriptCurvatureRequest_New; risk");
        const auto& second = Excel::CheckedCurvatureValue(bumps, "DupireScriptCurvatureRequest_New; bumps");
        request->reset(new StorableDupireScriptCurvatureRequest_(name, {first, second}));
    }

    void DupireScriptCurvatureRequest_Get_Risk(const Handle_<StorableDupireScriptCurvatureRequest_>& request,
                                               Handle_<StorableDupireScriptRiskRequest_>* risk) {
        const auto& value = Excel::CheckedCurvatureValue(request, "DupireScriptCurvatureRequest_Get_Risk; request");
        risk->reset(new StorableDupireScriptRiskRequest_("", value.risk_));
    }

    void DupireScriptCurvatureRequest_Get_Bumps(const Handle_<StorableDupireScriptCurvatureRequest_>& request,
                                                Handle_<StorableBumpOverAADRequest_>* bumps) {
        const auto& value = Excel::CheckedCurvatureValue(request, "DupireScriptCurvatureRequest_Get_Bumps; request");
        bumps->reset(new StorableBumpOverAADRequest_("", value.bumps_));
    }

    void DupireScriptCurvaturePlan_New(const String_& name,
                                       const Handle_<ScriptProductData_>& product,
                                       const Handle_<ModelData_>& modelData,
                                       const Handle_<StorableDupireCalibration_>& calibration,
                                       const String_& component,
                                       const Handle_<StorableDupireScriptCurvatureRequest_>& request,
                                       Handle_<StorableDupireScriptCurvaturePlan_>* plan) {
        Excel::CheckCurvatureText(name, "DupireScriptCurvaturePlan_New; name");
        Excel::CheckCurvatureText(component, "DupireScriptCurvaturePlan_New; component");
        REQUIRE(product, "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; product; handle is null");
        REQUIRE(modelData, "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; modelData; handle is null");
        REQUIRE(calibration, "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; calibration; handle is null");
        const auto& value = Excel::CheckedCurvatureValue(request, "DupireScriptCurvaturePlan_New; request");
        const auto& quotes = calibration->val_.Inputs().quoteSpreads_;
        const size_t count = static_cast<size_t>(quotes.Rows()) * static_cast<size_t>(quotes.Cols());
        REQUIRE(count < static_cast<size_t>(Excel::CURVATURE_MAX_ROWS),
                "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; full quote axis plus header exceeds worksheet rows");
        REQUIRE(value.bumps_.directions_.Rows() <= Excel::CURVATURE_MAX_ROWS,
                "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; direction count exceeds worksheet rows");
        REQUIRE(value.bumps_.directions_.Rows() == 0 || value.bumps_.directions_.Cols() <= Excel::CURVATURE_MAX_COLUMNS,
                "InvalidCurvatureRequest: DupireScriptCurvaturePlan_New; direction columns exceed worksheet columns");
        plan->reset(new StorableDupireScriptCurvaturePlan_(name, PlanDupireScriptCurvature(product, modelData, calibration->val_, component, value)));
    }

    void DupireScriptCurvaturePlan_Get_BasePlan(const Handle_<StorableDupireScriptCurvaturePlan_>& plan,
                                                Handle_<StorableDupireScriptRiskPlan_>* basePlan) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_BasePlan; plan");
        basePlan->reset(new StorableDupireScriptRiskPlan_("", value.BasePlan()));
    }

    void DupireScriptCurvaturePlan_Get_Point(const Handle_<StorableDupireScriptCurvaturePlan_>& plan, Matrix_<Cell_>* point) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_Point; plan");
        *point = Excel::CurvatureVectorCells(value.Point());
    }

    void DupireScriptCurvaturePlan_Get_Directions(const Handle_<StorableDupireScriptCurvaturePlan_>& plan, Matrix_<Cell_>* directions) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_Directions; plan");
        *directions = Excel::CurvatureMatrixCells(value.Directions());
    }

    void DupireScriptCurvaturePlan_Get_Steps(const Handle_<StorableDupireScriptCurvaturePlan_>& plan, Matrix_<Cell_>* steps) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_Steps; plan");
        *steps = Excel::CurvatureVectorCells(value.Steps());
    }

    void DupireScriptCurvaturePlan_Get_Shape(const Handle_<StorableDupireScriptCurvaturePlan_>& plan, Matrix_<Cell_>* shape) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_Shape; plan");
        *shape = Excel::RiskShapeCells(value.Directions());
    }

    void DupireScriptCurvaturePlan_Get_Payload(const Handle_<StorableDupireScriptCurvaturePlan_>& plan, double* payload) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvaturePlan_Get_Payload; plan");
        *payload = double(value.NumericPayloadBytes());
    }

    void DupireScriptCurvatureResult_New(const String_& name,
                                         const Handle_<StorableDupireScriptCurvaturePlan_>& plan,
                                         Handle_<StorableDupireScriptCurvatureResult_>* result) {
        Excel::CheckCurvatureText(name, "DupireScriptCurvatureResult_New; name");
        const auto& value = Excel::CheckedCurvatureValue(plan, "DupireScriptCurvatureResult_New; plan");
        result->reset(new StorableDupireScriptCurvatureResult_(name, ValueByMonteCarloWithDupireCurvature(value)));
    }

    void DupireScriptCurvatureResult_Get_Gradient(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* gradient) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Gradient; result");
        *gradient = Excel::CurvatureVectorCells(value.Gradient());
    }

    void DupireScriptCurvatureResult_Get_Base(const Handle_<StorableDupireScriptCurvatureResult_>& result,
                                              Handle_<StorableDupireScriptRiskResult_>* base) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Base; result");
        base->reset(new StorableDupireScriptRiskResult_("", value.Base()));
    }

    void DupireScriptCurvatureResult_Get_QuotePlan(const Handle_<StorableDupireScriptCurvatureResult_>& result,
                                                   Handle_<StorableCalibrationRiskPlan_>* quotePlan) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_QuotePlan; result");
        quotePlan->reset(new StorableCalibrationRiskPlan_("", value.Base().QuoteRisk().Plan()));
    }

    void DupireScriptCurvatureResult_Get_Point(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* point) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Point; result");
        *point = Excel::CurvatureVectorCells(value.Point());
    }

    void DupireScriptCurvatureResult_Get_Directions(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* directions) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Directions; result");
        *directions = Excel::CurvatureMatrixCells(value.Directions());
    }

    void DupireScriptCurvatureResult_Get_Steps(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* steps) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Steps; result");
        *steps = Excel::CurvatureVectorCells(value.Steps());
    }

    void DupireScriptCurvatureResult_Get_Shape(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* shape) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Shape; result");
        *shape = Excel::RiskShapeCells(value.HessianProducts());
    }

    void DupireScriptCurvatureResult_Get_Execution(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* execution) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_Execution; result").Execution();
        *execution = FieldCells({Field("method", value.method_), Field("quote_gradient_evaluations", double(value.quoteGradientEvaluations_)),
                                 Field("paths_per_evaluation", double(value.pathsPerEvaluation_)),
                                 Field("numeric_payload_bytes", double(value.numericPayloadBytes_))});
    }

    void DupireScriptCurvatureResult_Get_HessianProducts(const Handle_<StorableDupireScriptCurvatureResult_>& result, Matrix_<Cell_>* products) {
        const auto& value = Excel::CheckedCurvatureValue(result, "DupireScriptCurvatureResult_Get_HessianProducts; result");
        *products = Excel::CurvatureMatrixCells(value.HessianProducts());
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_DupireScriptCurvatureRequest_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureRequest_Get_Risk_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureRequest_Get_Bumps_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Gradient_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_HessianProducts_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_BasePlan_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_Point_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_Directions_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_Steps_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_Shape_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvaturePlan_Get_Payload_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Base_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_QuotePlan_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Point_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Directions_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Steps_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_DupireScriptCurvatureResult_Get_Execution_public.inc>
#endif
    // clang-format on
} // namespace Dal
