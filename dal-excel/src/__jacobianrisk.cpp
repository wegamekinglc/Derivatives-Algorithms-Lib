//
// Created by Codex on 2026/10/06.
//

#include "__platform.hpp"
#include "__risk.hpp"
#include "__riskrequestinput.hpp"
#include "__script_test_api.hpp"
#include "__settingskeys.hpp"

/*IF--------------------------------------------------------------------------
public JacobianRiskRequest_New
    Create an immutable budgeted script Jacobian request
&inputs
name is string
    Request name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "JacobianRiskRequest_New; name");
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateRiskRequestSettings(xl_settings, "JacobianRiskRequest_New");
&optional
settings is cell[][]+
    Two columns: inputs/outputs (semicolon IDs), report_factors (semicolon numbers), max_block_width,
    numeric_payload_budget_bytes, recording_capacity_budget_bytes, scratch_capacity_budget_bytes.
    Missing keys use defaults; blank ID lists explicitly select none. Width defaults to one.
&outputs
request is handle StorableJacobianRiskRequest
    Immutable request; no history or valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarlo_ValueWithJacobianRisk
    Value ordered script outputs and their budgeted native Jacobian
&inputs
product is handle ScriptProductData
    Product handle
+xl_product = Excel::ScriptScalarInput(xl_product);
modelData is handle ModelData
    Model data handle
+xl_modelData = Excel::ScriptScalarInput(xl_modelData);
+argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "MonteCarlo_ValueWithJacobianRisk");
n_paths is number
    Positive integral paths per replay, at most INT_MAX
+xl_request = Excel::ScriptScalarInput(xl_request);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
request is handle StorableJacobianRiskRequest
    Blank selects payoff, all native inputs and width one
valuation is handle StorableScriptValuationSettings
    Blank captures current date and required history once
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; explicit price-only requires empty inputs
&outputs
result is handle StorableJacobianRiskResult
    Immutable result; each native output block replays the complete path range
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Values
    Extract ordered output identities and means
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
values is cell[][]
    Header and rows: id, label, slot, mean
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Jacobian
    Extract a copy of the complete selected-output Jacobian
&inputs
result is handle StorableJacobianRiskResult
    Completed result
+argName = "reported";
+xl_reported = Excel::ScriptScalarInput(xl_reported);
+Excel::ValidateRiskRequestBoolean(xl_reported, "JacobianRiskResult_Get_Jacobian; reported");
&optional
reported is boolean (false)
    True applies each selected input report factor once
&outputs
jacobian is cell[][]
    Selected outputs by selected inputs; empty columns spill one blank cell; use Get_Shape
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Shape
    Extract exact selected-output Jacobian dimensions
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
shape is cell[][]
    One row: output count and input count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Outputs
    Extract ordered selected or complete output coordinates
&inputs
result is handle StorableJacobianRiskResult
    Completed result
+argName = "complete";
+xl_complete = Excel::ScriptScalarInput(xl_complete);
+Excel::ValidateRiskRequestBoolean(xl_complete, "JacobianRiskResult_Get_Outputs; complete");
&optional
complete is boolean (false)
    True returns the original complete scalar output axis
&outputs
outputs is cell[][]
    Header and rows: id, label, slot
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Inputs
    Extract ordered selected or complete input coordinates
&inputs
result is handle StorableJacobianRiskResult
    Completed result
+argName = "complete";
+xl_complete = Excel::ScriptScalarInput(xl_complete);
+Excel::ValidateRiskRequestBoolean(xl_complete, "JacobianRiskResult_Get_Inputs; complete");
&optional
complete is boolean (false)
    True returns the original complete input axis
&outputs
inputs is cell[][]
    Header and rows: id, label, family, ordinal, value, native unit, physical unit, report scale
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Execution
    Extract actual widths, replayed work and numeric capacity peaks
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
execution is cell[][]
    Two columns: field and value; widths are semicolon-separated; integers above 2^53-1 are text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Provenance
    Extract frozen method and valuation settings
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
provenance is cell[][]
    Two columns: field and value; no valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_History
    Extract frozen observations without history access
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
history is cell[][]
    Header and rows: index, fixing_time, historical, value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_Product
    Extract the frozen source product table
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
product is cell[][]
    Two columns: date or constant name, event or constant value text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public JacobianRiskResult_Get_ModelSnapshot
    Extract frozen passive model JSON chunks
&inputs
result is handle StorableJacobianRiskResult
    Completed result
&outputs
json is string[]
    Concatenate without separators to recover the snapshot
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        const Script::JacobianRiskResult_& CheckedJacobianResult(const Handle_<StorableJacobianRiskResult_>& result) {
            REQUIRE(result, "InvalidJacobianRiskResult: result=null; expected a completed Jacobian result handle");
            return result->val_;
        }

        const Script::RiskExecutionSnapshot_& JacobianExecutionSnapshot(const Handle_<StorableJacobianRiskResult_>& result) {
            return Excel::RiskExecution(CheckedJacobianResult(result).Provenance());
        }

        void ReadJacobianRequestCell(const String_& key, const Cell_& cell, const String_& context, Script::JacobianRiskRequest_* value) {
            RequireKnownSettingsKey(key, {"inputs", "outputs", "report_factors", "numeric_payload_budget_bytes", "max_block_width",
                                          "recording_capacity_budget_bytes", "scratch_capacity_budget_bytes"});
            if (key == "max_block_width") {
                const auto width = Excel::PayloadBudget(cell, context, "max_block_width");
                REQUIRE(width > 0 && width <= AAD::ADJ_SIZE,
                        context + "max_block_width must be in [1, " + String_(std::to_string(AAD::ADJ_SIZE)) + "]");
                value->maxBlockWidth_ = width;
            } else if (key == "recording_capacity_budget_bytes")
                value->recordingCapacityBudgetBytes_ = Excel::PayloadBudget(cell, context, "recording_capacity_budget_bytes");
            else if (key == "scratch_capacity_budget_bytes")
                value->scratchCapacityBudgetBytes_ = Excel::PayloadBudget(cell, context, "scratch_capacity_budget_bytes");
            else
                Excel::ReadRiskSelectionCell(key, cell, context, &value->selection_);
        }

        Cell_ ExecutionIntegerCell(size_t value) {
            return value <= 9007199254740991ULL ? Cell_(double(value)) : Cell_(String_(std::to_string(value)));
        }
    } // namespace

    void JacobianRiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableJacobianRiskRequest_>* request) {
        REQUIRE(name.find('\0') == String_::npos, "InvalidJacobianRiskRequest: name; embedded NUL is unsupported");
        Script::JacobianRiskRequest_ value;
        Excel::ReadRows(
            settings, "JacobianRiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                ReadJacobianRequestCell(key, cell, context, &value);
            },
            true);
        request->reset(new StorableJacobianRiskRequest_(name, std::move(value)));
    }

    void MonteCarlo_ValueWithJacobianRisk(const Handle_<ScriptProductData_>& product,
                                          const Handle_<ModelData_>& modelData,
                                          double nPaths,
                                          const Handle_<StorableJacobianRiskRequest_>& request,
                                          const Handle_<StorableScriptValuationSettings_>& valuation,
                                          const Handle_<StorableMonteCarloSettings_>& simulation,
                                          Handle_<StorableJacobianRiskResult_>* result) {
        auto value = Excel::EvaluateRiskValuation<&ValueByMonteCarloWithJacobianRisk>(product, modelData, nPaths, request, valuation, simulation,
                                                                                      "MonteCarlo_ValueWithJacobianRisk");
        result->reset(new StorableJacobianRiskResult_(String_(), std::move(value)));
    }

    void JacobianRiskResult_Get_Values(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* values) {
        const auto& value = CheckedJacobianResult(result);
        auto cells = Excel::OutputCoordinateCells(value.OutputAxis(), 4);
        cells(0, 3) = "mean";
        for (size_t index = 0; index < value.Values().size(); ++index)
            cells(static_cast<int>(index) + 1, 3) = value.Values()[index];
        *values = std::move(cells);
    }

    void JacobianRiskResult_Get_Jacobian(const Handle_<StorableJacobianRiskResult_>& result, bool reported, Matrix_<Cell_>* jacobian) {
        const auto& value = CheckedJacobianResult(result);
        *jacobian = reported ? Excel::NumericCells(value.ReportedJacobian()) : Excel::NumericCells(value.Jacobian());
    }

    void JacobianRiskResult_Get_Shape(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* shape) {
        *shape = Excel::RiskShapeCells(CheckedJacobianResult(result).Jacobian());
    }

    void JacobianRiskResult_Get_Outputs(const Handle_<StorableJacobianRiskResult_>& result, bool complete, Matrix_<Cell_>* outputs) {
        const auto& value = CheckedJacobianResult(result);
        *outputs = Excel::OutputCoordinateCells(complete ? value.CompleteOutputAxis() : value.OutputAxis());
    }

    void JacobianRiskResult_Get_Inputs(const Handle_<StorableJacobianRiskResult_>& result, bool complete, Matrix_<Cell_>* inputs) {
        const auto& value = CheckedJacobianResult(result);
        *inputs = Excel::RiskCoordinateCells(complete ? value.CompleteInputAxis() : value.InputAxis());
    }

    void JacobianRiskResult_Get_Execution(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* execution) {
        const auto& evidence = CheckedJacobianResult(result).Execution();
        String_ widths;
        for (const auto width : evidence.actualWidths_)
            widths += (widths.empty() ? "" : ";") + String_(std::to_string(width));
        *execution = Excel::FieldCells({{"actual_widths", Cell_(widths)},
                                        {"replay_attempts", ExecutionIntegerCell(evidence.replayAttempts_)},
                                        {"executed_paths", ExecutionIntegerCell(evidence.executedPaths_)},
                                        {"peak_recording_bytes", ExecutionIntegerCell(evidence.peakRecordingBytes_)},
                                        {"peak_scratch_bytes", ExecutionIntegerCell(evidence.peakScratchBytes_)}});
    }

    void JacobianRiskResult_Get_Provenance(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* provenance) {
        *provenance = Excel::RiskProvenanceCells(CheckedJacobianResult(result).Provenance());
    }

    void JacobianRiskResult_Get_History(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* history) {
        *history = Excel::RiskHistoryCells(JacobianExecutionSnapshot(result));
    }

    void JacobianRiskResult_Get_Product(const Handle_<StorableJacobianRiskResult_>& result, Matrix_<Cell_>* product) {
        *product = Excel::RiskProductCells(JacobianExecutionSnapshot(result));
    }

    void JacobianRiskResult_Get_ModelSnapshot(const Handle_<StorableJacobianRiskResult_>& result, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(JacobianExecutionSnapshot(result).modelSnapshotJson_, "JacobianRiskResult_Get_ModelSnapshot");
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_JacobianRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_History_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Jacobian_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_ModelSnapshot_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Outputs_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Product_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_JacobianRiskResult_Get_Values_public.inc>
#include <dal-excel/auto/MG_MonteCarlo_ValueWithJacobianRisk_public.inc>
#endif
} // namespace Dal
