//
// Created by Codex on 2026/10/6.
//

#include <iomanip>
#include <sstream>

#include "__platform.hpp"
#include "__riskrequest.hpp"
#include "__riskrequestinput.hpp"
#include "__riskrequestrows.hpp"
#include "__settingskeys.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public CalibrationRiskRequest_New
    Create an owning quote request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskRequest_New; name");
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "CalibrationRiskRequest_New");
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
&optional
settings is cell[][]+
    Two columns: inputs, report_factors, numeric_payload_budget_bytes; semicolon lists
&outputs
request is handle StorableCalibrationRiskRequest
    Immutable request; omitted inputs select all, explicit blank inputs select none
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskRequest_Get_Settings
    Copy configured quote request rows
&inputs
request is handle StorableCalibrationRiskRequest
    Quote request
&outputs
settings is cell[][]
    Configured rows only; default request spills one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskPlan_New
    Plan canonical quote selection and numeric payload
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskPlan_New; name");
calibration is handle StorableCalibrationPullback
    Common calibration boundary
+xl_request = Excel::ScriptScalarInput(xl_request);
&optional
request is handle StorableCalibrationRiskRequest
    Blank selects all quotes
&outputs
plan is handle StorableCalibrationRiskPlan
    Owning immutable plan
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskPlan_Get_Calibration
    Copy a plan's common boundary
&inputs
plan is handle StorableCalibrationRiskPlan
    Quote plan
&outputs
calibration is handle StorableCalibrationPullback
    Common calibration boundary
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskPlan_Get_Inputs
    Copy complete or selected canonical quote coordinates
&inputs
plan is handle StorableCalibrationRiskPlan
    Quote plan
+argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "CalibrationRiskPlan_Get_Inputs; complete");
+xl_complete = Excel::ScriptScalarInput(xl_complete);
&optional
complete is boolean (false)
    True returns every quote; default returns selected quotes in requested order
&outputs
inputs is cell[][]
    Header plus quote metadata; missing native fields are blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskPlan_Get_Shape
    Copy exact quote projection shape and retained payload
&inputs
plan is handle StorableCalibrationRiskPlan
    Quote plan
&outputs
shape is cell[][]
    Key/value rows including selected columns, native quote dimensions and payload bytes
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskResult_New
    Execute the native calibration pullback for a quote plan
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskResult_New; name");
plan is handle StorableCalibrationRiskPlan
    Quote plan
parameters is handle StorableCalibrationParameterAdjoints
    Canonical parameter seeds
+xl_direct = Excel::ScriptScalarInput(xl_direct);
&optional
direct is handle StorableCalibrationDirectQuoteAdjoints
    Blank omits direct quote seeds
&outputs
result is handle StorableCalibrationRiskResult
    Owning requested quote result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskResult_Get_Plan
    Copy the result's quote plan
&inputs
result is handle StorableCalibrationRiskResult
    Requested quote result
&outputs
plan is handle StorableCalibrationRiskPlan
    Owning quote plan
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskResult_Get_QuoteRisk
    Copy the full separated native quote contribution result
&inputs
result is handle StorableCalibrationRiskResult
    Requested quote result
&outputs
quoteRisk is handle StorableCalibrationQuoteRisk
    Full raw calibration, direct and total quote matrices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationRiskResult_Get_Jacobian
    Copy a selected raw or reported quote row
&inputs
result is handle StorableCalibrationRiskResult
    Requested quote result
+argName = "projection"; Excel::ValidateRiskRequestText(xl_projection, "CalibrationRiskResult_Get_Jacobian; projection");
+xl_projection = Excel::ScriptScalarInput(xl_projection);
&optional
projection is string
    Total (default), Calibration, Direct or Reported
&outputs
jacobian is cell[][]
    Scalar quote row; zero selected columns spill one blank cell
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        String_ FactorText(const Vector_<>& factors) {
            std::ostringstream text;
            text << std::setprecision(17);
            for (size_t index = 0; index < factors.size(); ++index) {
                if (index)
                    text << ';';
                text << factors[index];
            }
            return String_(text.str());
        }

        Matrix_<Cell_> QuoteCoordinateCells(const Vector_<CalibrationQuoteCoordinate_>& axis) {
            const Vector_<String_> headers{"id",           "label", "ordinal", "row",      "column",    "native_unit",
                                           "report_scale", "value", "strike",  "maturity", "block_key", "block_ordinal"};
            Matrix_<Cell_> cells(static_cast<int>(axis.size()) + 1, static_cast<int>(headers.size()));
            for (int column = 0; column < cells.Cols(); ++column)
                cells(0, column) = headers[column];
            for (size_t index = 0; index < axis.size(); ++index) {
                const auto& coordinate = axis[index];
                const int row = static_cast<int>(index) + 1;
                cells(row, 0) = coordinate.id_;
                cells(row, 1) = coordinate.label_;
                cells(row, 2) = double(coordinate.ordinal_);
                cells(row, 3) = double(coordinate.row_);
                cells(row, 4) = double(coordinate.column_);
                cells(row, 5) = coordinate.nativeUnit_;
                cells(row, 6) = coordinate.reportScale_;
                cells(row, 7) = Excel::OptionalCell(coordinate.value_);
                cells(row, 8) = Excel::OptionalCell(coordinate.strike_);
                cells(row, 9) = Excel::OptionalCell(coordinate.maturity_);
                cells(row, 10) = Excel::OptionalCell(coordinate.blockKey_);
                cells(row, 11) = Excel::OptionalCell(coordinate.blockOrdinal_);
            }
            return cells;
        }

        Matrix_<> ProjectQuoteRisk(const CalibrationRiskResult_& value, const String_& projection) {
            if (projection.empty() || projection == "Total")
                return value.Jacobian();
            if (projection == "Calibration")
                return value.CalibrationJacobian();
            if (projection == "Direct")
                return value.DirectJacobian();
            REQUIRE(projection == "Reported", "InvalidRiskRequest: projection must be Total, Calibration, Direct or Reported");
            return value.ReportedJacobian();
        }
    } // namespace

    void CalibrationRiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableCalibrationRiskRequest_>* request) {
        Excel::CheckRequestText(name, "CalibrationRiskRequest_New; name");
        CalibrationRiskRequest_ value;
        Excel::ReadRows(
            settings, "CalibrationRiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "report_factors", "numeric_payload_budget_bytes"});
                if (key == "inputs")
                    value.inputs_ = Excel::ListValue(cell, context);
                else if (key == "report_factors")
                    value.reportFactors_ = Excel::FactorList(cell, context);
                else
                    value.numericPayloadBudgetBytes_ = Excel::PayloadBudget(cell, context);
            },
            true);
        request->reset(new StorableCalibrationRiskRequest_(name, std::move(value)));
    }

    void CalibrationRiskRequest_Get_Settings(const Handle_<StorableCalibrationRiskRequest_>& request, Matrix_<Cell_>* settings) {
        const auto& value = Excel::CheckedRequestValue(request, "CalibrationRiskRequest_Get_Settings; request");
        Vector_<std::pair<String_, Cell_>> fields;
        if (value.inputs_)
            fields.push_back(Excel::Field("inputs", String::Accumulate(*value.inputs_, ";")));
        if (value.reportFactors_)
            fields.push_back(Excel::Field("report_factors", FactorText(*value.reportFactors_)));
        if (value.numericPayloadBudgetBytes_)
            fields.push_back(Excel::Field("numeric_payload_budget_bytes", double(*value.numericPayloadBudgetBytes_)));
        *settings = Excel::FieldCells(fields);
    }

    void CalibrationRiskPlan_New(const String_& name,
                                 const Handle_<StorableCalibrationPullback_>& calibration,
                                 const Handle_<StorableCalibrationRiskRequest_>& request,
                                 Handle_<StorableCalibrationRiskPlan_>* plan) {
        Excel::CheckRequestText(name, "CalibrationRiskPlan_New; name");
        const auto& boundary = Excel::CheckedRequestValue(calibration, "CalibrationRiskPlan_New; calibration");
        plan->reset(
            new StorableCalibrationRiskPlan_(name, PlanCalibrationRiskRequest(boundary, request ? request->val_ : CalibrationRiskRequest_())));
    }

    void CalibrationRiskPlan_Get_Calibration(const Handle_<StorableCalibrationRiskPlan_>& plan, Handle_<StorableCalibrationPullback_>* calibration) {
        const auto& value = Excel::CheckedRequestValue(plan, "CalibrationRiskPlan_Get_Calibration; plan");
        calibration->reset(new StorableCalibrationPullback_(String_(), value.Calibration()));
    }

    void CalibrationRiskPlan_Get_Inputs(const Handle_<StorableCalibrationRiskPlan_>& plan, bool complete, Matrix_<Cell_>* inputs) {
        const auto& value = Excel::CheckedRequestValue(plan, "CalibrationRiskPlan_Get_Inputs; plan");
        *inputs = QuoteCoordinateCells(complete ? value.CompleteInputAxis() : value.InputAxis());
    }

    void CalibrationRiskPlan_Get_Shape(const Handle_<StorableCalibrationRiskPlan_>& plan, Matrix_<Cell_>* shape) {
        const auto& value = Excel::CheckedRequestValue(plan, "CalibrationRiskPlan_Get_Shape; plan");
        *shape = Excel::FieldCells({Excel::Field("adjoint_rows", 1.0), Excel::Field("selected_quote_columns", double(value.InputAxis().size())),
                                    Excel::Field("native_quote_rows", double(value.Calibration().QuoteRows())),
                                    Excel::Field("native_quote_columns", double(value.Calibration().QuoteCols())),
                                    Excel::Field("numeric_payload_bytes", double(value.NumericPayloadBytes()))});
    }

    void CalibrationRiskResult_New(const String_& name,
                                   const Handle_<StorableCalibrationRiskPlan_>& plan,
                                   const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                   const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                   Handle_<StorableCalibrationRiskResult_>* result) {
        Excel::CheckRequestText(name, "CalibrationRiskResult_New; name");
        const auto& requested = Excel::CheckedRequestValue(plan, "CalibrationRiskResult_New; plan");
        const auto& seeds = Excel::CheckedRequestValue(parameters, "CalibrationRiskResult_New; parameters");
        const auto contribution = direct ? std::optional<CalibrationDirectQuoteAdjoints_>(direct->val_) : std::nullopt;
        result->reset(new StorableCalibrationRiskResult_(name, PullbackCalibrationWithRisk(requested, seeds, contribution)));
    }

    void CalibrationRiskResult_Get_Plan(const Handle_<StorableCalibrationRiskResult_>& result, Handle_<StorableCalibrationRiskPlan_>* plan) {
        const auto& value = Excel::CheckedRequestValue(result, "CalibrationRiskResult_Get_Plan; result");
        plan->reset(new StorableCalibrationRiskPlan_(String_(), value.Plan()));
    }

    void CalibrationRiskResult_Get_QuoteRisk(const Handle_<StorableCalibrationRiskResult_>& result,
                                             Handle_<StorableCalibrationQuoteRisk_>* quoteRisk) {
        const auto& value = Excel::CheckedRequestValue(result, "CalibrationRiskResult_Get_QuoteRisk; result");
        quoteRisk->reset(new StorableCalibrationQuoteRisk_(String_(), value.QuoteRisk()));
    }

    void
    CalibrationRiskResult_Get_Jacobian(const Handle_<StorableCalibrationRiskResult_>& result, const String_& projection, Matrix_<Cell_>* jacobian) {
        Excel::CheckRequestText(projection, "CalibrationRiskResult_Get_Jacobian; projection");
        *jacobian =
            Excel::NumericCells(ProjectQuoteRisk(Excel::CheckedRequestValue(result, "CalibrationRiskResult_Get_Jacobian; result"), projection));
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrationRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskRequest_Get_Settings_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskPlan_New_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskPlan_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskPlan_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskPlan_Get_Shape_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskResult_New_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskResult_Get_Plan_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskResult_Get_QuoteRisk_public.inc>
#include <dal-excel/auto/MG_CalibrationRiskResult_Get_Jacobian_public.inc>
#endif
    // clang-format on
} // namespace Dal
