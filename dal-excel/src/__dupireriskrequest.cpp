//
// Created by Codex on 2026/10/6.
//

#include "__platform.hpp"
#include "__riskrequest.hpp"
#include "__riskrequestinput.hpp"
#include "__riskrequestrows.hpp"
#include "__value.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public DupireScriptRiskSettings_New
    Create passive automatic Dupire execution settings
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskSettings_New; name");
+argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths);
+xl_n_paths = Excel::ScriptScalarInput(xl_n_paths);
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
n_paths is number
    Positive exactly integral path count
&optional
valuation is handle StorableScriptValuationSettings
    Blank uses default valuation settings
simulation is handle StorableMonteCarloSettings
    Blank enables native AAD; explicit false rejects during planning
&outputs
settings is handle StorableDupireScriptRiskSettings
    Immutable settings; no date capture or execution
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskSettings_Get_Configuration
    Copy execution count and typed settings
&inputs
settings is handle StorableDupireScriptRiskSettings
    Execution settings
&outputs
n_paths is number
    Path count
valuation is handle StorableScriptValuationSettings
    Copied valuation settings
simulation is handle StorableMonteCarloSettings
    Copied simulation settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskRequest_New
    Create an automatic quote request with explicit direct dependencies
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskRequest_New; name");
settings is handle StorableDupireScriptRiskSettings
    Required execution settings
+xl_quotes = Excel::ScriptScalarInput(xl_quotes);
+argName = "bindings"; Excel::ValidateRiskRequestBindings(xl_bindings);
+const Excel::ScriptSettingsInput_ normalized(xl_bindings); xl_bindings = normalized.Get();
+xl_direct = Excel::ScriptScalarInput(xl_direct);
&optional
quotes is handle StorableCalibrationRiskRequest
    Blank selects all quotes; budget applies to combined retained results
bindings is cell[][]+
    Two data columns: exact constant ordinal, quote ID; no header
direct is handle StorableCalibrationDirectQuoteAdjoints
    Optional external direct seeds; mutually exclusive with bindings
&outputs
request is handle StorableDupireScriptRiskRequest
    Immutable automatic request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskRequest_Get_Configuration
    Copy owning automatic request controls
&inputs
request is handle StorableDupireScriptRiskRequest
    Automatic request
&outputs
settings is handle StorableDupireScriptRiskSettings
    Copied execution settings
quotes is handle StorableCalibrationRiskRequest
    Copied quote selection and budget
bindings is cell[][]
    Direct binding data or one blank cell
has_direct is boolean
    Whether external direct seeds are present
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskRequest_Get_Direct
    Copy external direct seeds or reject if absent
&inputs
request is handle StorableDupireScriptRiskRequest
    Automatic request with external direct seeds
&outputs
direct is handle StorableCalibrationDirectQuoteAdjoints
    Owned direct seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskPlan_New
    Seal an automatic Dupire valuation before history or worker activity
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskPlan_New; name");
product is handle ScriptProductData
    Scalar script payoff
modelData is handle ModelData
    Native single-currency Hybrid with deterministic flat rate
calibration is handle StorableDupireCalibration
    Frozen target Dupire calibration
+argName = "component"; Excel::ValidateRiskRequestText(xl_component, "DupireScriptRiskPlan_New; component");
+xl_component = Excel::ScriptScalarInput(xl_component);
component is string
    Exact target local-vol component name
request is handle StorableDupireScriptRiskRequest
    Required automatic quote and execution request
&outputs
plan is handle StorableDupireScriptRiskPlan
    Owning sealed plan with exact numeric payload
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskPlan_Get_QuotePlan
    Copy the automatic plan's common quote plan
&inputs
plan is handle StorableDupireScriptRiskPlan
    Sealed plan
&outputs
quotePlan is handle StorableCalibrationRiskPlan
    Common quote selection and source
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskPlan_Get_Inputs
    Copy required or complete native model and constant axes
&inputs
plan is handle StorableDupireScriptRiskPlan
    Sealed plan
+argName = "complete"; Excel::ValidateRiskRequestBoolean(xl_complete, "DupireScriptRiskPlan_Get_Inputs; complete");
+xl_complete = Excel::ScriptScalarInput(xl_complete);
&optional
complete is boolean (false)
    True returns the complete model axis; default returns required inputs
&outputs
inputs is cell[][]
    Header plus exact model/constant coordinates
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskPlan_Get_Configuration
    Copy captured execution settings and direct bindings
&inputs
plan is handle StorableDupireScriptRiskPlan
    Sealed plan
&outputs
settings is handle StorableDupireScriptRiskSettings
    Captured settings
bindings is cell[][]
    Direct binding data or one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskPlan_Get_Provenance
    Copy passive automatic plan metadata
&inputs
plan is handle StorableDupireScriptRiskPlan
    Sealed plan
&outputs
provenance is cell[][]
    Component, path count, axis counts and combined retained numeric payload
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskResult_New
    Execute native AAD valuation and common quote pullback
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskResult_New; name");
plan is handle StorableDupireScriptRiskPlan
    Sealed automatic plan
&outputs
result is handle StorableDupireScriptRiskResult
    Owning scalar valuation and separated quote risks
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskResult_Get_Valuation
    Copy the retained fixed-calibration model valuation
&inputs
result is handle StorableDupireScriptRiskResult
    Automatic result
&outputs
valuation is handle StorableRiskResult
    Structured native valuation, required Jacobian, settings and history
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskResult_Get_QuoteRisk
    Copy the retained requested common quote risk
&inputs
result is handle StorableDupireScriptRiskResult
    Automatic result
&outputs
quoteRisk is handle StorableCalibrationRiskResult
    Requested raw and reported quote contributions
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptRiskResult_Get_Provenance
    Copy automatic result method and retained numeric payload
&inputs
result is handle StorableDupireScriptRiskResult
    Automatic result
&outputs
provenance is cell[][]
    Component, combined method and numeric payload bytes
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        Vector_<DupireQuoteBinding_> ReadBindings(const Matrix_<Cell_>& cells) {
            if (Excel::IsDefaultSettingsInput(cells))
                return {};
            REQUIRE(cells.Cols() == 2, "InvalidRiskRequest: DupireScriptRiskRequest_New; bindings; expected two columns, ordinal and quote ID");
            Vector_<DupireQuoteBinding_> bindings;
            for (int row = 0; row < cells.Rows(); ++row) {
                if (Cell::IsEmpty(cells(row, 0)) && Cell::IsEmpty(cells(row, 1)))
                    continue;
                const auto context = Excel::ScriptSettingLocation("DupireScriptRiskRequest_New", "bindings", row + 1, 1);
                const auto ordinal = Excel::PayloadBudget(cells(row, 0), context, "constant ordinal");
                const auto quoteContext = Excel::ScriptSettingLocation("DupireScriptRiskRequest_New", "bindings", row + 1, 2);
                const auto id = Excel::TextValue(cells(row, 1), quoteContext);
                Excel::CheckRequestText(id, quoteContext);
                bindings.push_back({ordinal, id});
            }
            return bindings;
        }

        Matrix_<Cell_> BindingCells(const Vector_<DupireQuoteBinding_>& bindings) {
            if (bindings.empty())
                return Matrix_<Cell_>(1, 1);
            Matrix_<Cell_> cells(static_cast<int>(bindings.size()), 2);
            for (int row = 0; row < cells.Rows(); ++row) {
                cells(row, 0) = double(bindings[row].constantOrdinal_);
                cells(row, 1) = bindings[row].quoteId_;
            }
            return cells;
        }

        Handle_<StorableDupireScriptRiskSettings_>
        ExecutionSettings(int paths, const ScriptValuationSettings_& valuation, const MonteCarloSettings_& simulation) {
            return Handle_<StorableDupireScriptRiskSettings_>(new StorableDupireScriptRiskSettings_(String_(), {paths, valuation, simulation}));
        }
    } // namespace

    void DupireScriptRiskSettings_New(const String_& name,
                                      double nPaths,
                                      const Handle_<StorableScriptValuationSettings_>& valuation,
                                      const Handle_<StorableMonteCarloSettings_>& simulation,
                                      Handle_<StorableDupireScriptRiskSettings_>* settings) {
        Excel::CheckRequestText(name, "DupireScriptRiskSettings_New; name");
        Excel::DupireScriptRiskSettings_ value{Excel::CheckedMonteCarloPathCount(nPaths, "DupireScriptRiskSettings_New"),
                                               valuation ? valuation->val_ : ScriptValuationSettings_(),
                                               simulation ? simulation->val_ : DefaultRiskMonteCarloSettings()};
        settings->reset(new StorableDupireScriptRiskSettings_(name, std::move(value)));
    }

    void DupireScriptRiskSettings_Get_Configuration(const Handle_<StorableDupireScriptRiskSettings_>& settings,
                                                    double* nPaths,
                                                    Handle_<StorableScriptValuationSettings_>* valuation,
                                                    Handle_<StorableMonteCarloSettings_>* simulation) {
        const auto& value = Excel::CheckedRequestValue(settings, "DupireScriptRiskSettings_Get_Configuration; settings");
        const Handle_<StorableScriptValuationSettings_> val(new StorableScriptValuationSettings_(String_(), value.valuation_));
        const Handle_<StorableMonteCarloSettings_> sim(new StorableMonteCarloSettings_(String_(), value.simulation_));
        *nPaths = double(value.numPaths_);
        *valuation = val;
        *simulation = sim;
    }

    void DupireScriptRiskRequest_New(const String_& name,
                                     const Handle_<StorableDupireScriptRiskSettings_>& settings,
                                     const Handle_<StorableCalibrationRiskRequest_>& quotes,
                                     const Matrix_<Cell_>& bindings,
                                     const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                     Handle_<StorableDupireScriptRiskRequest_>* request) {
        Excel::CheckRequestText(name, "DupireScriptRiskRequest_New; name");
        const auto& config = Excel::CheckedRequestValue(settings, "DupireScriptRiskRequest_New; settings");
        DupireScriptRiskRequest_ value;
        value.numPaths_ = config.numPaths_;
        value.valuation_ = config.valuation_;
        value.simulation_ = config.simulation_;
        value.quotes_ = quotes ? quotes->val_ : CalibrationRiskRequest_();
        value.directBindings_ = ReadBindings(bindings);
        value.direct_ = direct ? std::optional<CalibrationDirectQuoteAdjoints_>(direct->val_) : std::nullopt;
        request->reset(new StorableDupireScriptRiskRequest_(name, std::move(value)));
    }

    void DupireScriptRiskRequest_Get_Configuration(const Handle_<StorableDupireScriptRiskRequest_>& request,
                                                   Handle_<StorableDupireScriptRiskSettings_>* settings,
                                                   Handle_<StorableCalibrationRiskRequest_>* quotes,
                                                   Matrix_<Cell_>* bindings,
                                                   bool* hasDirect) {
        const auto& value = Excel::CheckedRequestValue(request, "DupireScriptRiskRequest_Get_Configuration; request");
        const auto config = ExecutionSettings(value.numPaths_, value.valuation_, value.simulation_);
        const Handle_<StorableCalibrationRiskRequest_> quoteRequest(new StorableCalibrationRiskRequest_(String_(), value.quotes_));
        auto cells = BindingCells(value.directBindings_);
        *settings = config;
        *quotes = quoteRequest;
        *bindings = std::move(cells);
        *hasDirect = value.direct_.has_value();
    }

    void DupireScriptRiskRequest_Get_Direct(const Handle_<StorableDupireScriptRiskRequest_>& request,
                                            Handle_<StorableCalibrationDirectQuoteAdjoints_>* direct) {
        const auto& value = Excel::CheckedRequestValue(request, "DupireScriptRiskRequest_Get_Direct; request");
        REQUIRE(value.direct_, "InvalidRiskRequest: DupireScriptRiskRequest_Get_Direct; external direct seeds are absent");
        direct->reset(new StorableCalibrationDirectQuoteAdjoints_(String_(), *value.direct_));
    }

    void DupireScriptRiskPlan_New(const String_& name,
                                  const Handle_<ScriptProductData_>& product,
                                  const Handle_<ModelData_>& modelData,
                                  const Handle_<StorableDupireCalibration_>& calibration,
                                  const String_& component,
                                  const Handle_<StorableDupireScriptRiskRequest_>& request,
                                  Handle_<StorableDupireScriptRiskPlan_>* plan) {
        Excel::CheckRequestText(name, "DupireScriptRiskPlan_New; name");
        Excel::CheckRequestText(component, "DupireScriptRiskPlan_New; component");
        REQUIRE(calibration, "InvalidRiskRequest: DupireScriptRiskPlan_New; calibration; handle is null");
        const auto& requested = Excel::CheckedRequestValue(request, "DupireScriptRiskPlan_New; request");
        plan->reset(new StorableDupireScriptRiskPlan_(name, PlanDupireScriptRisk(product, modelData, calibration->val_, component, requested)));
    }

    void DupireScriptRiskPlan_Get_QuotePlan(const Handle_<StorableDupireScriptRiskPlan_>& plan, Handle_<StorableCalibrationRiskPlan_>* quotePlan) {
        const auto& value = Excel::CheckedRequestValue(plan, "DupireScriptRiskPlan_Get_QuotePlan; plan");
        quotePlan->reset(new StorableCalibrationRiskPlan_(String_(), value.QuotePlan()));
    }

    void DupireScriptRiskPlan_Get_Inputs(const Handle_<StorableDupireScriptRiskPlan_>& plan, bool complete, Matrix_<Cell_>* inputs) {
        const auto& value = Excel::CheckedRequestValue(plan, "DupireScriptRiskPlan_Get_Inputs; plan");
        *inputs = Excel::RiskCoordinateCells(complete ? value.CompleteInputAxis() : value.RequiredInputAxis());
    }

    void DupireScriptRiskPlan_Get_Configuration(const Handle_<StorableDupireScriptRiskPlan_>& plan,
                                                Handle_<StorableDupireScriptRiskSettings_>* settings,
                                                Matrix_<Cell_>* bindings) {
        const auto& value = Excel::CheckedRequestValue(plan, "DupireScriptRiskPlan_Get_Configuration; plan");
        const auto config = ExecutionSettings(value.NumPaths(), value.ValuationSettings(), value.SimulationSettings());
        auto cells = BindingCells(value.DirectBindings());
        *settings = config;
        *bindings = std::move(cells);
    }

    void DupireScriptRiskPlan_Get_Provenance(const Handle_<StorableDupireScriptRiskPlan_>& plan, Matrix_<Cell_>* provenance) {
        const auto& value = Excel::CheckedRequestValue(plan, "DupireScriptRiskPlan_Get_Provenance; plan");
        *provenance = Excel::FieldCells({Excel::Field("component", value.Component()), Excel::Field("n_paths", double(value.NumPaths())),
                                         Excel::Field("required_input_count", double(value.RequiredInputAxis().size())),
                                         Excel::Field("complete_input_count", double(value.CompleteInputAxis().size())),
                                         Excel::Field("numeric_payload_bytes", double(value.NumericPayloadBytes()))});
    }

    void DupireScriptRiskResult_New(const String_& name,
                                    const Handle_<StorableDupireScriptRiskPlan_>& plan,
                                    Handle_<StorableDupireScriptRiskResult_>* result) {
        Excel::CheckRequestText(name, "DupireScriptRiskResult_New; name");
        const auto& value = Excel::CheckedRequestValue(plan, "DupireScriptRiskResult_New; plan");
        result->reset(new StorableDupireScriptRiskResult_(name, ValueByMonteCarloWithDupireRisk(value)));
    }

    void DupireScriptRiskResult_Get_Valuation(const Handle_<StorableDupireScriptRiskResult_>& result, Handle_<StorableRiskResult_>* valuation) {
        const auto& value = Excel::CheckedRequestValue(result, "DupireScriptRiskResult_Get_Valuation; result");
        valuation->reset(new StorableRiskResult_(String_(), value.Valuation()));
    }

    void DupireScriptRiskResult_Get_QuoteRisk(const Handle_<StorableDupireScriptRiskResult_>& result,
                                              Handle_<StorableCalibrationRiskResult_>* quoteRisk) {
        const auto& value = Excel::CheckedRequestValue(result, "DupireScriptRiskResult_Get_QuoteRisk; result");
        quoteRisk->reset(new StorableCalibrationRiskResult_(String_(), value.QuoteRisk()));
    }

    void DupireScriptRiskResult_Get_Provenance(const Handle_<StorableDupireScriptRiskResult_>& result, Matrix_<Cell_>* provenance) {
        const auto& value = Excel::CheckedRequestValue(result, "DupireScriptRiskResult_Get_Provenance; result");
        *provenance = Excel::FieldCells({Excel::Field("component", value.Component()), Excel::Field("method", value.Method()),
                                         Excel::Field("numeric_payload_bytes", double(value.NumericPayloadBytes()))});
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_DupireScriptRiskSettings_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskRequest_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskRequest_Get_Direct_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskPlan_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskPlan_Get_QuotePlan_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskPlan_Get_Inputs_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskPlan_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskPlan_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskResult_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskResult_Get_Valuation_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskResult_Get_QuoteRisk_public.inc>
#include <dal-excel/auto/MG_DupireScriptRiskResult_Get_Provenance_public.inc>
#endif
    // clang-format on
} // namespace Dal
