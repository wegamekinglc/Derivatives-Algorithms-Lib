//
// Created by Codex on 2026/10/5.
//

#include <array>
#include <cmath>
#include <limits>

#include "__platform.hpp"
#include "__risk.hpp"
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
            const auto& provenance = CheckedResult(result).Provenance();
            REQUIRE(provenance.execution_, "InvalidRiskResult: execution snapshot is unavailable on a caller-provided conversion result");
            return *provenance.execution_;
        }

        using Excel::FactorList;
        using Excel::Field;
        using Excel::ListValue;
        using Excel::NumericCells;
        using Excel::OptionalCell;
        using Excel::PayloadBudget;
    } // namespace

    void RiskRequest_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableRiskRequest_>* request) {
        Script::RiskRequest_ value;
        Excel::ReadRows(
            settings, "RiskRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
                RequireKnownSettingsKey(key, {"inputs", "outputs", "report_factors", "numeric_payload_budget_bytes"});
                if (key == "inputs")
                    value.inputs_ = ListValue(cell, context);
                else if (key == "outputs")
                    value.outputs_ = ListValue(cell, context);
                else if (key == "report_factors")
                    value.reportFactors_ = FactorList(cell, context);
                else
                    value.numericPayloadBudgetBytes_ = PayloadBudget(cell, context);
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
        const int count = Excel::CheckedMonteCarloPathCount(nPaths, "MonteCarlo_ValueWithRisk");
        const auto requested = request ? request->val_ : Script::RiskRequest_();
        const auto settings = valuation ? valuation->val_ : ScriptValuationSettings_();
        const auto execution = simulation ? simulation->val_ : DefaultRiskMonteCarloSettings();
        auto value = ValueByMonteCarloWithRisk(product, modelData, count, requested, settings, execution);
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
        const auto& value = CheckedResult(result).Jacobian();
        Matrix_<Cell_> cells(1, 2);
        cells(0, 0) = double(value.Rows());
        cells(0, 1) = double(value.Cols());
        *shape = std::move(cells);
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
        const auto& source = CheckedResult(result).Provenance();
        const auto& execution = Execution(result);
        const auto& simulation = execution.simulation_;
        const Vector_<std::pair<String_, Cell_>> fields{
            Field("method", source.method_),
            Field("engine", source.engine_),
            Field("normalization", source.normalization_),
            Field("calibration", source.calibration_),
            Field("model_type", source.modelType_),
            Field("evaluation_date", OptionalCell(source.evaluationDate_)),
            Field("paths_per_replicate", double(execution.pathsPerReplicate_)),
            Field("pricing_replicates", double(execution.pricingReplicates_)),
            Field("all_expired", execution.allExpired_),
            Field("rsg", simulation.rsg_),
            Field("use_bb", simulation.useBb_),
            Field("enable_aad", simulation.enableAad_),
            Field("smooth", simulation.smooth_),
            Field("compiled", simulation.compiled_.value_or(false)),
            Field("lsmc_basis_degree", double(simulation.lsmcBasisDegree_)),
            Field("lsmc_training_paths", OptionalCell(simulation.lsmcTrainingPaths_)),
            Field("lsmc_validation_paths", OptionalCell(simulation.lsmcValidationPaths_)),
            Field("lsmc_rqmc_replicates", OptionalCell(simulation.lsmcRqmcReplicates_)),
            Field("lsmc_training_seed", OptionalCell(simulation.lsmcTrainingSeed_)),
            Field("lsmc_pricing_seed", OptionalCell(simulation.lsmcPricingSeed_)),
            Field("lsmc_policy_risk_mode", simulation.lsmcPolicyRiskMode_),
            Field("lsmc_policy_bump_relative", simulation.lsmcPolicyBumpRelative_),
            Field("today_fixing_policy", execution.todayFixingPolicy_),
            Field("fixing_source", execution.fixingSource_),
            Field("default_index", execution.productSettings_.defaultIndex_),
            Field("regression_features", String::Accumulate(execution.productSettings_.regressionFeatures_, ";"))};
        Matrix_<Cell_> cells(static_cast<int>(fields.size()), 2);
        for (size_t index = 0; index < fields.size(); ++index) {
            cells(static_cast<int>(index), 0) = fields[index].first;
            cells(static_cast<int>(index), 1) = fields[index].second;
        }
        *provenance = std::move(cells);
    }

    void RiskResult_Get_History(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* history) {
        const auto& observations = Execution(result).observations_;
        Matrix_<Cell_> cells(static_cast<int>(observations.size()) + 1, 4);
        constexpr std::array<const char*, 4> HEADERS{"index", "fixing_time", "historical", "value"};
        for (int column = 0; column < 4; ++column)
            cells(0, column).val_.emplace<String_>(HEADERS[column]);
        for (size_t index = 0; index < observations.size(); ++index) {
            const auto& observation = observations[index];
            const int row = static_cast<int>(index) + 1;
            cells(row, 0) = observation.index_;
            cells(row, 1) = DateTime::ToString(observation.fixingTime_);
            cells(row, 2) = observation.historical_;
            cells(row, 3) = OptionalCell(observation.value_);
        }
        *history = std::move(cells);
    }

    void RiskResult_Get_Product(const Handle_<StorableRiskResult_>& result, Matrix_<Cell_>* product) {
        const auto& execution = Execution(result);
        Matrix_<Cell_> cells(static_cast<int>(execution.productDates_.size()), 2);
        for (size_t index = 0; index < execution.productDates_.size(); ++index) {
            cells(static_cast<int>(index), 0) = execution.productDates_[index];
            cells(static_cast<int>(index), 1) = execution.productEvents_[index];
        }
        *product = std::move(cells);
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
