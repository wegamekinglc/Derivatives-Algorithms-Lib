//
// Created by Codex on 2026-10-06.
//

#include "__platform.hpp"
#include "__risk.hpp"
#include "__riskrequestinput.hpp"
#include "__script_test_api.hpp"
#include "__settingskeys.hpp"

/*IF--------------------------------------------------------------------------
public WeightedRiskRequest_New
    Create an immutable fixed-weight script risk request
&inputs
name is string
    Request name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "WeightedRiskRequest_New; name");
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateRiskRequestSettings(xl_settings, "WeightedRiskRequest_New");
&optional
settings is cell[][]+
    Two columns: inputs/outputs (semicolon IDs), weights/report_factors (semicolon numbers), numeric_payload_budget_bytes.
    Missing key uses defaults; blank lists explicitly select none; weights may be signed or zero.
&outputs
request is handle StorableWeightedRiskRequest
    Immutable request; no history or valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarlo_ValueWithWeightedRisk
    Value one fixed-weight objective and retain its native gradient
&inputs
product is handle ScriptProductData
    Product handle
+xl_product = Excel::ScriptScalarInput(xl_product);
modelData is handle ModelData
    Model data handle
+xl_modelData = Excel::ScriptScalarInput(xl_modelData);
+argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "MonteCarlo_ValueWithWeightedRisk");
n_paths is number
    Positive integral paths per replicate, at most INT_MAX
+xl_request = Excel::ScriptScalarInput(xl_request);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&optional
request is handle StorableWeightedRiskRequest
    Blank selects payoff with unit weight and all native inputs
valuation is handle StorableScriptValuationSettings
    Blank captures current date and required history
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; explicit enable_aad=false requests price only
&outputs
result is handle StorableWeightedRiskResult
    Immutable result; getters perform no valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Product_Get_RiskOutputs
    List indexed scalar output coordinates without valuation
&inputs
product is handle ScriptProductData
    Product handle
+xl_product = Excel::ScriptScalarInput(xl_product);
&outputs
outputs is cell[][]
    Header and rows: id, label, slot; receiver ID is payoff; vector storage is excluded
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_WeightedValue
    Extract the mean weighted objective
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
value is number
    Objective mean
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Components
    Extract ordered output identities, passive weights and component means
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
components is cell[][]
    Header and rows: id, label, slot, weight, mean
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Jacobian
    Extract a copy of the weighted objective gradient
&inputs
result is handle StorableWeightedRiskResult
    Completed result
+argName = "reported";
+xl_reported = Excel::ScriptScalarInput(xl_reported);
+Excel::ValidateRiskRequestBoolean(xl_reported, "WeightedRiskResult_Get_Jacobian; reported");
&optional
reported is boolean (false)
    True applies each requested report factor once
&outputs
jacobian is cell[][]
    One row and selected input columns; no columns spills one blank cell; use Get_Shape
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Shape
    Extract the exact gradient dimensions
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
shape is cell[][]
    One row: 1 and selected input count
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Inputs
    Extract ordered selected or complete input coordinates
&inputs
result is handle StorableWeightedRiskResult
    Completed result
+argName = "complete";
+xl_complete = Excel::ScriptScalarInput(xl_complete);
+Excel::ValidateRiskRequestBoolean(xl_complete, "WeightedRiskResult_Get_Inputs; complete");
&optional
complete is boolean (false)
    True returns the full original input axis
&outputs
inputs is cell[][]
    Header and rows: ID, label, family, ordinal, value, native unit, physical unit, report scale
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Provenance
    Extract the retained method and execution settings without valuation
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
provenance is cell[][]
    Two columns: field and value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_History
    Extract frozen observations without history access
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
history is cell[][]
    Header and rows: index, fixing_time, historical, value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_Product
    Extract the retained source product table
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
product is cell[][]
    Two columns: date or constant name, event or constant value text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public WeightedRiskResult_Get_ModelSnapshot
    Extract retained passive model JSON chunks
&inputs
result is handle StorableWeightedRiskResult
    Completed result
&outputs
json is string[]
    Concatenate without separators to recover the model snapshot
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        const Script::WeightedRiskResult_& CheckedWeightedResult(const Handle_<StorableWeightedRiskResult_>& result) {
            REQUIRE(result, "InvalidWeightedRiskResult: result=null; expected a completed weighted result handle");
            return result->val_;
        }

        const Script::RiskExecutionSnapshot_& WeightedExecution(const Handle_<StorableWeightedRiskResult_>& result) {
            return Excel::RiskExecution(CheckedWeightedResult(result).Provenance());
        }
    } // namespace

    void WeightedRiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableWeightedRiskRequest_>* request) {
        REQUIRE(name.find('\0') == String_::npos, "InvalidWeightedRiskRequest: name; embedded NUL is unsupported");
        Script::WeightedRiskRequest_ value;
        Excel::ReadRows(
            settings, "WeightedRiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "outputs", "weights", "report_factors", "numeric_payload_budget_bytes"});
                if (key == "weights")
                    value.weights_ = Excel::WeightList(cell, context);
                else
                    Excel::ReadRiskSelectionCell(key, cell, context, &value.selection_);
            },
            true);
        request->reset(new StorableWeightedRiskRequest_(name, std::move(value)));
    }

    void MonteCarlo_ValueWithWeightedRisk(const Handle_<ScriptProductData_>& product,
                                          const Handle_<ModelData_>& modelData,
                                          double nPaths,
                                          const Handle_<StorableWeightedRiskRequest_>& request,
                                          const Handle_<StorableScriptValuationSettings_>& valuation,
                                          const Handle_<StorableMonteCarloSettings_>& simulation,
                                          Handle_<StorableWeightedRiskResult_>* result) {
        auto value = Excel::EvaluateRiskValuation<&ValueByMonteCarloWithWeightedRisk>(product, modelData, nPaths, request, valuation, simulation,
                                                                                      "MonteCarlo_ValueWithWeightedRisk");
        result->reset(new StorableWeightedRiskResult_(String_(), std::move(value)));
    }

    void Product_Get_RiskOutputs(const Handle_<ScriptProductData_>& product, Matrix_<Cell_>* outputs) {
        REQUIRE(product, "InvalidWeightedRiskRequest: product=null; expected a product handle");
        auto indexed = product->Product();
        indexed.IndexVariables();
        *outputs = Excel::OutputCoordinateCells(Script::ScriptRiskOutputAxis(indexed));
    }

    void WeightedRiskResult_Get_WeightedValue(const Handle_<StorableWeightedRiskResult_>& result, double* value) {
        *value = CheckedWeightedResult(result).WeightedValue();
    }

    void WeightedRiskResult_Get_Components(const Handle_<StorableWeightedRiskResult_>& result, Matrix_<Cell_>* components) {
        const auto& value = CheckedWeightedResult(result);
        auto cells = Excel::OutputCoordinateCells(value.OutputAxis(), 5);
        cells(0, 3) = "weight";
        cells(0, 4) = "mean";
        for (size_t index = 0; index < value.Weights().size(); ++index) {
            const int row = static_cast<int>(index) + 1;
            cells(row, 3) = value.Weights()[index];
            cells(row, 4) = value.ComponentMeans()[index];
        }
        *components = std::move(cells);
    }

    void WeightedRiskResult_Get_Jacobian(const Handle_<StorableWeightedRiskResult_>& result, bool reported, Matrix_<Cell_>* jacobian) {
        const auto& value = CheckedWeightedResult(result);
        *jacobian = reported ? Excel::NumericCells(value.ReportedJacobian()) : Excel::NumericCells(value.Jacobian());
    }

    void WeightedRiskResult_Get_Shape(const Handle_<StorableWeightedRiskResult_>& result, Matrix_<Cell_>* shape) {
        *shape = Excel::RiskShapeCells(CheckedWeightedResult(result).Jacobian());
    }

    void WeightedRiskResult_Get_Inputs(const Handle_<StorableWeightedRiskResult_>& result, bool complete, Matrix_<Cell_>* inputs) {
        const auto& value = CheckedWeightedResult(result);
        *inputs = Excel::RiskCoordinateCells(complete ? value.CompleteInputAxis() : value.InputAxis());
    }

    void WeightedRiskResult_Get_Provenance(const Handle_<StorableWeightedRiskResult_>& result, Matrix_<Cell_>* provenance) {
        *provenance = Excel::RiskProvenanceCells(CheckedWeightedResult(result).Provenance());
    }

    void WeightedRiskResult_Get_History(const Handle_<StorableWeightedRiskResult_>& result, Matrix_<Cell_>* history) {
        *history = Excel::RiskHistoryCells(WeightedExecution(result));
    }

    void WeightedRiskResult_Get_Product(const Handle_<StorableWeightedRiskResult_>& result, Matrix_<Cell_>* product) {
        *product = Excel::RiskProductCells(WeightedExecution(result));
    }

    void WeightedRiskResult_Get_ModelSnapshot(const Handle_<StorableWeightedRiskResult_>& result, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(WeightedExecution(result).modelSnapshotJson_, "WeightedRiskResult_Get_ModelSnapshot");
    }

#ifdef _WIN32
#include <dal-excel/auto/MG_MonteCarlo_ValueWithWeightedRisk_public.inc>
#include <dal-excel/auto/MG_Product_Get_RiskOutputs_public.inc>
#include <dal-excel/auto/MG_WeightedRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Components_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_History_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Jacobian_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_ModelSnapshot_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Product_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_WeightedRiskResult_Get_WeightedValue_public.inc>
#endif
} // namespace Dal
