//
// Created by Codex on 2026/10/5.
//

#include "__risk.hpp"
#include "__platform.hpp"
#include "__riskrequestrows.hpp"
#include "__script_test_api.hpp"
#include "__settingskeys.hpp"
#include "__value.hpp"

/*IF--------------------------------------------------------------------------
public RiskRequest_New
    Create an immutable scalar risk request
&inputs
name is string
    Request name
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "RiskRequest_New", "settings");
&optional
settings is cell[][]+
    Two columns: inputs/outputs (semicolon IDs), report_factors (semicolon numbers), numeric_payload_budget_bytes.
    Missing key uses defaults; blank inputs selects none.
&outputs
request is handle StorableRiskRequest
    Immutable request; no date capture or valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarlo_ValueWithRisk
    Run one valuation and retain passive structured scalar risk
&inputs
product is handle ScriptProductData
    Product handle
modelData is handle ModelData
    Model data handle
n_paths is number
    Positive integral paths per replicate, at most INT_MAX
+xl_request = Excel::ScriptScalarInput(xl_request);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
request is handle StorableRiskRequest
    Blank selects all native inputs and payoff
valuation is handle StorableScriptValuationSettings
    Blank captures current date and required history once
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; explicit enable_aad=false requests price only
&outputs
result is handle StorableRiskResult
    Immutable result; getters do no valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Values
    Extract mean numeric output values
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
values is cell[][]
    Output rows and one value column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Jacobian
    Extract a copy of the scalar Jacobian
&inputs
result is handle StorableRiskResult
    Completed result
&optional
reported is boolean (false)
    False returns raw derivatives; true applies each report factor once
&outputs
jacobian is cell[][]
    Output rows and selected input columns; no columns spills one blank cell; use Get_Shape
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Shape
    Extract the exact numeric Jacobian dimensions
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
shape is cell[][]
    One row: output count and selected input count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Inputs
    Extract ordered coordinate definitions
&inputs
result is handle StorableRiskResult
    Completed result
&optional
complete is boolean (false)
    False returns selected columns; true returns the full original axis
&outputs
inputs is cell[][]
    Header and rows: ID, label, family, ordinal, value, native unit, physical unit (blank if unknown), report scale
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Outputs
    Extract ordered output IDs
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
outputs is string[]
    Output IDs in value/Jacobian row order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Provenance
    Extract method and execution settings without valuation
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
provenance is cell[][]
    Two columns: field and value; blank optional settings retain their documented defaults
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_History
    Extract observation keys and frozen historical values
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
history is cell[][]
    Header and rows: index, fixing time, historical, frozen value (blank for model observations)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_Product
    Extract the original product dates and event text
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
product is cell[][]
    Two columns: date or constant name, event or constant value text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_ModelSnapshot
    Extract the retained passive model data JSON
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
json is string[]
    Concatenate chunks without separators to recover the model snapshot
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public RiskResult_Get_LegacyValues
    Extract raw PV and d_label compatibility keys
&inputs
result is handle StorableRiskResult
    Completed result
&outputs
values is cell[][]
    Two columns key/value; equal display labels fail rather than overwrite
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        const Script::RiskResult_& CheckedResult(const Handle_<StorableRiskResult_>& result) {
            REQUIRE(result, "InvalidRiskResult: result=null; expected a completed risk result handle");
            return result->val_;
        }

        const Script::RiskExecutionSnapshot_& Execution(const Handle_<StorableRiskResult_>& result) {
            return Excel::RiskExecution(CheckedResult(result).Provenance());
        }

        using Excel::NumericCells;
    } // namespace

    void RiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableRiskRequest_>* request) {
        Script::RiskRequest_ value;
        Excel::ReadRows(
            settings, "RiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "outputs", "report_factors", "numeric_payload_budget_bytes"});
                Excel::ReadRiskSelectionCell(key, cell, context, &value);
            },
            true);
        request->reset(new StorableRiskRequest_(name, std::move(value)));
    }

    void MonteCarlo_ValueWithRisk(const Handle_<ScriptProductData_>& product,
                                  const Handle_<ModelData_>& modelData,
                                  double nPaths,
                                  const Handle_<StorableRiskRequest_>& request,
                                  const Handle_<StorableScriptValuationSettings_>& valuation,
                                  const Handle_<StorableMonteCarloSettings_>& simulation,
                                  Handle_<StorableRiskResult_>* result) {
        auto value = Excel::EvaluateRiskValuation<&ValueByMonteCarloWithRisk>(product, modelData, nPaths, request, valuation, simulation,
                                                                              "MonteCarlo_ValueWithRisk");
        result->reset(new StorableRiskResult_(String_(), std::move(value)));
    }

    void RiskResult_Get_Values(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* values) {
        const auto& numeric = CheckedResult(result).Values();
        Matrix_<Cell_> cells(static_cast<int>(numeric.size()), 1);
        for (int row = 0; row < cells.Rows(); ++row)
            cells(row, 0) = numeric[static_cast<size_t>(row)];
        *values = std::move(cells);
    }

    void RiskResult_Get_Jacobian(const Handle_<StorableRiskResult_>& result, bool reported, Matrix_<Cell_>* jacobian) {
        const auto& value = CheckedResult(result);
        *jacobian = reported ? NumericCells(value.ReportedJacobian()) : NumericCells(value.Jacobian());
    }

    void RiskResult_Get_Shape(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* shape) {
        *shape = Excel::RiskShapeCells(CheckedResult(result).Jacobian());
    }

    void RiskResult_Get_Inputs(const Handle_<StorableRiskResult_>& result, bool complete, Matrix_<Cell_>* inputs) {
        const auto& value = CheckedResult(result);
        const auto& axis = complete ? value.CompleteInputAxis() : value.InputAxis();
        *inputs = Excel::RiskCoordinateCells(axis);
    }

    void RiskResult_Get_Outputs(const Handle_<StorableRiskResult_>& result, Vector_<String_>* outputs) {
        *outputs = CheckedResult(result).OutputIds();
    }

    void RiskResult_Get_Provenance(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* provenance) {
        *provenance = Excel::RiskProvenanceCells(CheckedResult(result).Provenance());
    }

    void RiskResult_Get_History(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* history) {
        *history = Excel::RiskHistoryCells(Execution(result));
    }

    void RiskResult_Get_Product(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* product) {
        *product = Excel::RiskProductCells(Execution(result));
    }

    void RiskResult_Get_ModelSnapshot(const Handle_<StorableRiskResult_>& result, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(Execution(result).modelSnapshotJson_, "RiskResult_Get_ModelSnapshot");
    }

    void RiskResult_Get_LegacyValues(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* values) {
        *values = Excel::MonteCarloPriceTable(CheckedResult(result).LegacyValues());
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_MonteCarlo_ValueWithRisk_public.inc>
#include <dal-excel/auto/MG_RiskRequest_New_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_History_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Jacobian_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_LegacyValues_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_ModelSnapshot_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Outputs_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Product_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_RiskResult_Get_Values_public.inc>
#endif
} // namespace Dal
