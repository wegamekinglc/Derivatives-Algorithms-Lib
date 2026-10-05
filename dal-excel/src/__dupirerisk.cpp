//
// Created by Codex on 2026/10/5.
//

#include <cmath>

#include "__dupirerisk.hpp"
#include <dal-public/src/models.hpp>

// Load native AAD declarations before the Windows SDK's REGISTERING macro.
#include "__dupireinput.hpp"
#include "__platform.hpp"
#include "__scriptsettingsrows.hpp"
#include "__settingskeys.hpp"

/*IF--------------------------------------------------------------------------
public MertonIVS_New
    Create a checked Merton implied-volatility base
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "MertonIVS_New; name");
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings"; Excel::ValidateDupireSettingsInput(xl_settings);
settings is cell[][]+
    Required numeric keys: spot, vol, intensity, average_jump, jump_std
&outputs
base is handle StorableMertonIVS
    Passive base with zero deterministic carry
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireGrid_New
    Create immutable Dupire calibration grid settings
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireGrid_New; name");
+const Excel::ScriptSettingsInput_ inclusionSpotsInput(xl_inclusionSpots); xl_inclusionSpots = inclusionSpotsInput.Get();
inclusionSpots is number[]
    Positive increasing spot grid
+const Excel::ScriptSettingsInput_ maxSpotSpacingInput(xl_maxSpotSpacing); xl_maxSpotSpacing = maxSpotSpacingInput.Get();
maxSpotSpacing is number
    Maximum positive spot spacing
+const Excel::ScriptSettingsInput_ inclusionTimesInput(xl_inclusionTimes); xl_inclusionTimes = inclusionTimesInput.Get();
inclusionTimes is number[]
    Positive increasing time grid
+const Excel::ScriptSettingsInput_ maxTimeSpacingInput(xl_maxTimeSpacing); xl_maxTimeSpacing = maxTimeSpacingInput.Get();
maxTimeSpacing is number
    Maximum positive time spacing
&outputs
grid is handle StorableDupireGrid
    Copied grid settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireRiskInputs_New
    Create immutable spread quotes and grid inputs
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireRiskInputs_New; name");
+const Excel::ScriptSettingsInput_ quoteStrikesInput(xl_quoteStrikes); xl_quoteStrikes = quoteStrikesInput.Get();
quoteStrikes is number[]
    Positive increasing strike axis
+const Excel::ScriptSettingsInput_ quoteMaturitiesInput(xl_quoteMaturities); xl_quoteMaturities = quoteMaturitiesInput.Get();
quoteMaturities is number[]
    Positive increasing maturity axis
+const Excel::ScriptSettingsInput_ quoteSpreadsInput(xl_quoteSpreads); xl_quoteSpreads = quoteSpreadsInput.Get();
quoteSpreads is number[][]
    Strike rows, maturity columns; absolute decimal-vol spreads
grid is handle StorableDupireGrid
    Immutable grid settings
&outputs
inputs is handle StorableDupireRiskInputs
    Copied quote and grid inputs
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_New
    Freeze checked discrete Dupire calibration
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireCalibration_New; name");
base is handle Storable
    Existing BS model or MertonIVS handle
inputs is handle StorableDupireRiskInputs
    Quote and grid inputs
&outputs
calibration is handle StorableDupireCalibration
    Frozen passive calibration
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireModelData_New
    Create a Hybrid local-vol model from frozen calibration
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireModelData_New; name");
calibration is handle StorableDupireCalibration
    Frozen calibration
index is string
    Equity index, for example EQ[LOCAL]
+argName = "index"; Excel::ValidateDupireTextInput(xl_index, "DupireModelData_New; index");
currency is string
    Domestic currency
+argName = "currency"; Excel::ValidateDupireTextInput(xl_currency, "DupireModelData_New; currency");
factor is string
    Single Brownian factor name
+argName = "factor"; Excel::ValidateDupireTextInput(xl_factor, "DupireModelData_New; factor");
+const Excel::ScriptSettingsInput_ maxStepInput(xl_maxStep); xl_maxStep = maxStepInput.Get();
&optional
maxStep is number (0.08333333333333333)
    Positive model time step
&outputs
model is handle ModelData
    Detached local-vol equity and deterministic-rate model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireParameterAdjoints_New
    Create immutable local-vol node adjoints
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireParameterAdjoints_New; name");
calibration is handle StorableDupireCalibration
    Frozen calibration
+const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
adjoints is number[][]
    Spot rows, model-time columns; finite raw PV adjoints
&outputs
parameters is handle StorableDupireParameterAdjoints
    Copied surface-node seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireDirectQuoteAdjoints_New
    Create immutable direct quote adjoints
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireDirectQuoteAdjoints_New; name");
calibration is handle StorableDupireCalibration
    Frozen calibration
+const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
adjoints is number[][]
    Strike rows, maturity columns; finite direct PV adjoints
&outputs
direct is handle StorableDupireDirectQuoteAdjoints
    Copied direct quote seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireParameterAdjoints_FromRisk
    Extract all local-vol node adjoints from a valuation
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireParameterAdjoints_FromRisk; name");
valuation is handle StorableRiskResult
    Completed scalar valuation risk
calibration is handle StorableDupireCalibration
    Frozen calibration
component is string
    Local-vol component name, equity for DupireModelData_New
+argName = "component"; Excel::ValidateDupireTextInput(xl_component, "DupireParameterAdjoints_FromRisk; component");
&outputs
parameters is handle StorableDupireParameterAdjoints
    Validated raw surface-node adjoints
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireQuoteRisk_New
    Pull local-vol node adjoints back to spread quotes
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireQuoteRisk_New; name");
calibration is handle StorableDupireCalibration
    Frozen calibration
parameters is handle StorableDupireParameterAdjoints
    Raw surface-node adjoints
+xl_direct = Excel::ScriptScalarInput(xl_direct);
&optional
direct is handle StorableDupireDirectQuoteAdjoints
    Blank omits the direct quote contribution
&outputs
result is handle StorableDupireQuoteRisk
    Passive separated quote contributions
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptQuoteRisk_New
    Map completed valuation risk to frozen spread quotes
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireScriptQuoteRisk_New; name");
valuation is handle StorableRiskResult
    Completed scalar valuation risk
calibration is handle StorableDupireCalibration
    Frozen calibration
component is string
    Local-vol component name
+argName = "component"; Excel::ValidateDupireTextInput(xl_component, "DupireScriptQuoteRisk_New; component");
+xl_direct = Excel::ScriptScalarInput(xl_direct);
&optional
direct is handle StorableDupireDirectQuoteAdjoints
    Blank omits the direct quote contribution
&outputs
result is handle StorableDupireScriptQuoteRisk
    Retained valuation and separated quote risk
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Surface
    Copy the calibrated local-vol surface
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
surface is handle LocalVolSurfaceData
    Detached surface data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Spots
    Copy completed calibration spot coordinates
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
spots is number[]
    Ordered spot axis
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Times
    Copy completed calibration model times
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
times is number[]
    Ordered time axis
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Vols
    Copy calibrated local-vol values
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
vols is number[][]
    Spot rows, model-time columns
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Quotes
    Extract frozen spread quote coordinates and values
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
quotes is cell[][]
    Header and strike, maturity, spread rows
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireCalibration_Get_Provenance
    Extract frozen carry and discrete calibration settings
&inputs
calibration is handle StorableDupireCalibration
    Frozen calibration
&outputs
provenance is cell[][]
    Field/value rows; includes algorithm, boundary and inclusion coordinates
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireParameterAdjoints_Get_Adjoints
    Copy local-vol node adjoints
&inputs
parameters is handle StorableDupireParameterAdjoints
    Surface-node seeds
&outputs
adjoints is number[][]
    Raw numeric surface-node seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireDirectQuoteAdjoints_Get_Adjoints
    Copy direct quote adjoints
&inputs
direct is handle StorableDupireDirectQuoteAdjoints
    Direct quote seeds
&outputs
adjoints is number[][]
    Raw numeric quote seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireQuoteRisk_Get_Adjoints
    Copy separated raw quote derivatives
&inputs
result is handle StorableDupireQuoteRisk
    Completed passive quote risk
&optional
contribution is string
    Blank selects total; otherwise exactly total, calibration or direct
+argName = "contribution"; Excel::ValidateDupireTextInput(xl_contribution, "DupireQuoteRisk_Get_Adjoints; contribution");
&outputs
adjoints is number[][]
    Strike rows, maturity columns; no path normalization or report scaling
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireQuoteRisk_Get_Provenance
    Extract quote-risk method and units
&inputs
result is handle StorableDupireQuoteRisk
    Completed passive quote risk
&outputs
provenance is cell[][]
    Method, decimal-vol unit, fixed-input boundary and algorithm
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptQuoteRisk_Get_Valuation
    Copy the retained scalar valuation result
&inputs
result is handle StorableDupireScriptQuoteRisk
    Completed valuation and quote risk
&outputs
valuation is handle StorableRiskResult
    Passive source result; no valuation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptQuoteRisk_Get_QuoteRisk
    Copy retained spread quote risk
&inputs
result is handle StorableDupireScriptQuoteRisk
    Completed valuation and quote risk
&outputs
quoteRisk is handle StorableDupireQuoteRisk
    Passive quote risk; no reverse
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireScriptQuoteRisk_Get_Provenance
    Extract composite quote-risk method and component
&inputs
result is handle StorableDupireScriptQuoteRisk
    Completed valuation and quote risk
&outputs
provenance is cell[][]
    Field/value method, component and quote unit
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        template <class T_> const T_& Checked(const Handle_<Excel::StorableDupireValue_<T_>>& handle, const String_& context) {
            REQUIRE(handle, "InvalidDupireInput: " + context + "; required handle is null");
            return handle->val_;
        }

        void CheckText(const String_& text, const String_& field) {
            REQUIRE(text.find('\0') == String_::npos, "InvalidDupireInput: " + field + "; embedded NUL is unsupported");
        }

        void CheckSeeds(const Matrix_<>& adjoints, const Matrix_<>& coordinates, const String_& context) {
            REQUIRE(adjoints.Rows() == coordinates.Rows() && adjoints.Cols() == coordinates.Cols(),
                    "InvalidDupirePullback: " + context + "; adjoints shape must match coordinates");
            for (const auto value : adjoints)
                REQUIRE(std::isfinite(value), "InvalidDupirePullback: " + context + "; adjoints must be finite");
        }

        template <class T_, class F_>
        void NewAdjoints(const String_& name,
                         const Handle_<StorableDupireCalibration_>& calibration,
                         const Matrix_<>& adjoints,
                         const String_& function,
                         F_ coordinates,
                         Handle_<Excel::StorableDupireValue_<T_>>* result) {
            CheckText(name, function + "; name");
            const auto& snapshot = Checked(calibration, function + "; calibration");
            CheckSeeds(adjoints, coordinates(snapshot), function);
            result->reset(new Excel::StorableDupireValue_<T_>(name, {snapshot, adjoints}));
        }

        DupireCalibrationSnapshot_ CalibrateBase(const Handle_<Storable_>& base, const DupireRiskInputs_& inputs, const String_& name) {
            if (const auto flat = handle_cast<BSModelData_>(base))
                return CalibrateDupireWithRisk(*flat, inputs, name);
            if (const auto merton = handle_cast<StorableMertonIVS_>(base))
                return CalibrateDupireWithRisk(merton->val_, inputs, name);
            THROW("InvalidDupireInput: DupireCalibration_New; base must be a BS model or MertonIVS handle");
        }

        std::optional<DupireDirectQuoteAdjoints_> Direct(const Handle_<StorableDupireDirectQuoteAdjoints_>& direct) {
            return direct ? std::optional<DupireDirectQuoteAdjoints_>(direct->val_) : std::nullopt;
        }

        const Script::RiskResult_& CheckedValuation(const Handle_<StorableRiskResult_>& valuation, const String_& context) {
            REQUIRE(valuation, "InvalidDupireInput: " + context + "; valuation handle is null");
            return valuation->val_;
        }

        Matrix_<Cell_> Fields(const Vector_<std::pair<String_, Cell_>>& fields) {
            Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
            for (int row = 0; row < result.Rows(); ++row) {
                result(row, 0) = fields[static_cast<size_t>(row)].first;
                result(row, 1) = fields[static_cast<size_t>(row)].second;
            }
            return result;
        }
    } // namespace

    void MertonIVS_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableMertonIVS_>* base) {
        CheckText(name, "MertonIVS_New; name");
        std::map<String_, double> values;
        Excel::ReadRows(settings, "MertonIVS_New", "settings", [&](const String_& key, const Cell_& cell, const String_&, const String_& context) {
            NOTE(context.c_str());
            CheckText(key, context);
            RequireKnownSettingsKey(key, {"spot", "vol", "intensity", "average_jump", "jump_std"});
            const auto* number = std::get_if<double>(&cell.val_);
            REQUIRE(number && std::isfinite(*number), context + "expected a finite numeric value, excluding bool/text");
            REQUIRE(key != "spot" || *number > 0.0, context + "spot must be positive");
            REQUIRE((key != "vol" && key != "intensity" && key != "jump_std") || *number >= 0.0, context + "value must be nonnegative");
            values.emplace(key, *number);
        });
        for (const auto* key : {"spot", "vol", "intensity", "average_jump", "jump_std"})
            REQUIRE(values.count(key) == 1, "InvalidSetting: MertonIVS_New; settings; missing required key=" + String_(key));
        base->reset(new StorableMertonIVS_(
            name, NewMertonIVS(values.at("spot"), values.at("vol"), values.at("intensity"), values.at("average_jump"), values.at("jump_std"))));
    }

    void DupireModelData_New(const String_& name,
                             const Handle_<StorableDupireCalibration_>& calibration,
                             const String_& index,
                             const String_& currency,
                             const String_& factor,
                             double maxStep,
                             Handle_<ModelData_>* model) {
        CheckText(name, "DupireModelData_New; name");
        CheckText(index, "DupireModelData_New; index");
        CheckText(currency, "DupireModelData_New; currency");
        CheckText(factor, "DupireModelData_New; factor");
        *model = NewDupireModelData(name, Checked(calibration, "DupireModelData_New; calibration"), index, currency, factor, maxStep);
    }

    void DupireDirectQuoteAdjoints_New(const String_& name,
                                       const Handle_<StorableDupireCalibration_>& calibration,
                                       const Matrix_<>& adjoints,
                                       Handle_<StorableDupireDirectQuoteAdjoints_>* direct) {
        NewAdjoints<DupireDirectQuoteAdjoints_>(
            name, calibration, adjoints, "DupireDirectQuoteAdjoints_New",
            [](const auto& snapshot) -> const Matrix_<>& { return snapshot.Inputs().quoteSpreads_; }, direct);
    }

    void DupireParameterAdjoints_FromRisk(const String_& name,
                                          const Handle_<StorableRiskResult_>& valuation,
                                          const Handle_<StorableDupireCalibration_>& calibration,
                                          const String_& component,
                                          Handle_<StorableDupireParameterAdjoints_>* parameters) {
        CheckText(name, "DupireParameterAdjoints_FromRisk; name");
        CheckText(component, "DupireParameterAdjoints_FromRisk; component");
        const auto value = ExtractDupireParameterAdjoints(CheckedValuation(valuation, "DupireParameterAdjoints_FromRisk"),
                                                          Checked(calibration, "DupireParameterAdjoints_FromRisk; calibration"), component);
        parameters->reset(new StorableDupireParameterAdjoints_(name, value));
    }

    void DupireScriptQuoteRisk_New(const String_& name,
                                   const Handle_<StorableRiskResult_>& valuation,
                                   const Handle_<StorableDupireCalibration_>& calibration,
                                   const String_& component,
                                   const Handle_<StorableDupireDirectQuoteAdjoints_>& direct,
                                   Handle_<StorableDupireScriptQuoteRisk_>* result) {
        CheckText(name, "DupireScriptQuoteRisk_New; name");
        CheckText(component, "DupireScriptQuoteRisk_New; component");
        const auto value = PullbackDupireScriptRisk(CheckedValuation(valuation, "DupireScriptQuoteRisk_New"),
                                                    Checked(calibration, "DupireScriptQuoteRisk_New; calibration"), component, Direct(direct));
        result->reset(new StorableDupireScriptQuoteRisk_(name, value));
    }

    void DupireCalibration_Get_Surface(const Handle_<StorableDupireCalibration_>& calibration, Handle_<LocalVolSurfaceData_>* surface) {
        const auto& value = *Checked(calibration, "DupireCalibration_Get_Surface; calibration").Surface();
        *surface = NewLocalVolSurfaceData(value.Name(), value.spots_, value.times_, value.vols_);
    }

    void DupireCalibration_Get_Spots(const Handle_<StorableDupireCalibration_>& calibration, Vector_<>* spots) {
        *spots = Checked(calibration, "DupireCalibration_Get_Spots; calibration").Surface()->spots_;
    }

    void DupireCalibration_Get_Times(const Handle_<StorableDupireCalibration_>& calibration, Vector_<>* times) {
        *times = Checked(calibration, "DupireCalibration_Get_Times; calibration").Surface()->times_;
    }

    void DupireCalibration_Get_Vols(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<>* vols) {
        *vols = Checked(calibration, "DupireCalibration_Get_Vols; calibration").Surface()->vols_;
    }

    void DupireCalibration_Get_Quotes(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<Cell_>* quotes) {
        const auto& inputs = Checked(calibration, "DupireCalibration_Get_Quotes; calibration").Inputs();
        Matrix_<Cell_> result(inputs.quoteSpreads_.Rows() * inputs.quoteSpreads_.Cols() + 1, 3);
        result(0, 0) = "strike";
        result(0, 1) = "maturity";
        result(0, 2) = "spread";
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                const int target = row * inputs.quoteSpreads_.Cols() + column + 1;
                result(target, 0) = inputs.quoteStrikes_[static_cast<size_t>(row)];
                result(target, 1) = inputs.quoteMaturities_[static_cast<size_t>(column)];
                result(target, 2) = inputs.quoteSpreads_(row, column);
            }
        *quotes = std::move(result);
    }

    void DupireCalibration_Get_Provenance(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<Cell_>* provenance) {
        const auto& value = Checked(calibration, "DupireCalibration_Get_Provenance; calibration");
        const auto& inputs = value.Inputs();
        Vector_<std::pair<String_, Cell_>> fields{{"algorithm", Cell_(value.Algorithm())},
                                                  {"boundary", Cell_("FixedBaseIVSDeterministicCarryFixedGrids")},
                                                  {"spot", Cell_(value.Spot())},
                                                  {"rate", Cell_(value.Rate())},
                                                  {"dividend_yield", Cell_(value.DividendYield())},
                                                  {"max_spot_spacing", Cell_(inputs.maxSpotSpacing_)},
                                                  {"max_time_spacing", Cell_(inputs.maxTimeSpacing_)}};
        for (size_t i = 0; i < inputs.inclusionSpots_.size(); ++i)
            fields.emplace_back("inclusion_spot:" + String_(std::to_string(i)), Cell_(inputs.inclusionSpots_[i]));
        for (size_t i = 0; i < inputs.inclusionTimes_.size(); ++i)
            fields.emplace_back("inclusion_time:" + String_(std::to_string(i)), Cell_(inputs.inclusionTimes_[i]));
        *provenance = Fields(fields);
    }

    void DupireParameterAdjoints_Get_Adjoints(const Handle_<StorableDupireParameterAdjoints_>& parameters, Matrix_<>* adjoints) {
        *adjoints = Checked(parameters, "DupireParameterAdjoints_Get_Adjoints; parameters").adjoints_;
    }

    void DupireDirectQuoteAdjoints_Get_Adjoints(const Handle_<StorableDupireDirectQuoteAdjoints_>& direct, Matrix_<>* adjoints) {
        *adjoints = Checked(direct, "DupireDirectQuoteAdjoints_Get_Adjoints; direct").adjoints_;
    }

    void DupireQuoteRisk_Get_Provenance(const Handle_<StorableDupireQuoteRisk_>& result, Matrix_<Cell_>* provenance) {
        const auto& value = Checked(result, "DupireQuoteRisk_Get_Provenance; result");
        *provenance = Fields({{"method", Cell_(value.Method())},
                              {"unit", Cell_(value.Unit())},
                              {"boundary", Cell_(value.Boundary())},
                              {"algorithm", Cell_(value.Calibration().Algorithm())}});
    }

    void DupireScriptQuoteRisk_Get_Valuation(const Handle_<StorableDupireScriptQuoteRisk_>& result, Handle_<StorableRiskResult_>* valuation) {
        const auto& value = Checked(result, "DupireScriptQuoteRisk_Get_Valuation; result");
        valuation->reset(new StorableRiskResult_(String_(), value.Valuation()));
    }

    void DupireScriptQuoteRisk_Get_QuoteRisk(const Handle_<StorableDupireScriptQuoteRisk_>& result, Handle_<StorableDupireQuoteRisk_>* quoteRisk) {
        const auto& value = Checked(result, "DupireScriptQuoteRisk_Get_QuoteRisk; result");
        quoteRisk->reset(new StorableDupireQuoteRisk_(String_(), value.QuoteRisk()));
    }

    void DupireScriptQuoteRisk_Get_Provenance(const Handle_<StorableDupireScriptQuoteRisk_>& result, Matrix_<Cell_>* provenance) {
        const auto& value = Checked(result, "DupireScriptQuoteRisk_Get_Provenance; result");
        *provenance = Fields({{"method", Cell_(value.Method())}, {"component", Cell_(value.Component())}, {"unit", Cell_(value.QuoteRisk().Unit())}});
    }

    void DupireGrid_New(const String_& name,
                        const Vector_<>& inclusionSpots,
                        double maxSpotSpacing,
                        const Vector_<>& inclusionTimes,
                        double maxTimeSpacing,
                        Handle_<StorableDupireGrid_>* grid) {
        CheckText(name, "DupireGrid_New; name");
        grid->reset(new StorableDupireGrid_(name, {inclusionSpots, maxSpotSpacing, inclusionTimes, maxTimeSpacing}));
    }

    void DupireRiskInputs_New(const String_& name,
                              const Vector_<>& quoteStrikes,
                              const Vector_<>& quoteMaturities,
                              const Matrix_<>& quoteSpreads,
                              const Handle_<StorableDupireGrid_>& grid,
                              Handle_<StorableDupireRiskInputs_>* inputs) {
        CheckText(name, "DupireRiskInputs_New; name");
        const auto& settings = Checked(grid, "DupireRiskInputs_New; grid");
        inputs->reset(new StorableDupireRiskInputs_(name, {quoteStrikes, quoteMaturities, quoteSpreads, settings.inclusionSpots_,
                                                           settings.maxSpotSpacing_, settings.inclusionTimes_, settings.maxTimeSpacing_}));
    }

    void DupireCalibration_New(const String_& name,
                               const Handle_<Storable_>& base,
                               const Handle_<StorableDupireRiskInputs_>& inputs,
                               Handle_<StorableDupireCalibration_>* calibration) {
        CheckText(name, "DupireCalibration_New; name");
        const auto& settings = Checked(inputs, "DupireCalibration_New; inputs");
        calibration->reset(new StorableDupireCalibration_(name, CalibrateBase(base, settings, name)));
    }

    void DupireParameterAdjoints_New(const String_& name,
                                     const Handle_<StorableDupireCalibration_>& calibration,
                                     const Matrix_<>& adjoints,
                                     Handle_<StorableDupireParameterAdjoints_>* parameters) {
        NewAdjoints<DupireParameterAdjoints_>(
            name, calibration, adjoints, "DupireParameterAdjoints_New",
            [](const auto& snapshot) -> const Matrix_<>& { return snapshot.Surface()->vols_; }, parameters);
    }

    void DupireQuoteRisk_New(const String_& name,
                             const Handle_<StorableDupireCalibration_>& calibration,
                             const Handle_<StorableDupireParameterAdjoints_>& parameters,
                             const Handle_<StorableDupireDirectQuoteAdjoints_>& direct,
                             Handle_<StorableDupireQuoteRisk_>* result) {
        CheckText(name, "DupireQuoteRisk_New; name");
        const auto value = PullbackDupireCalibration(Checked(calibration, "DupireQuoteRisk_New; calibration"),
                                                     Checked(parameters, "DupireQuoteRisk_New; parameters"), Direct(direct));
        result->reset(new StorableDupireQuoteRisk_(name, value));
    }

    void DupireQuoteRisk_Get_Adjoints(const Handle_<StorableDupireQuoteRisk_>& result, const String_& contribution, Matrix_<>* adjoints) {
        const auto& value = Checked(result, "DupireQuoteRisk_Get_Adjoints; result");
        if (contribution.empty() || contribution == "total")
            *adjoints = value.TotalAdjoints();
        else if (contribution == "calibration")
            *adjoints = value.CalibrationAdjoints();
        else if (contribution == "direct")
            *adjoints = value.DirectAdjoints();
        else
            THROW("InvalidDupireInput: DupireQuoteRisk_Get_Adjoints; contribution must be total, calibration or direct");
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_DupireCalibration_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_Get_Quotes_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_Get_Spots_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_Get_Surface_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_Get_Times_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_Get_Vols_public.inc>
#include <dal-excel/auto/MG_DupireCalibration_New_public.inc>
#include <dal-excel/auto/MG_DupireDirectQuoteAdjoints_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_DupireDirectQuoteAdjoints_New_public.inc>
#include <dal-excel/auto/MG_DupireGrid_New_public.inc>
#include <dal-excel/auto/MG_DupireModelData_New_public.inc>
#include <dal-excel/auto/MG_DupireParameterAdjoints_FromRisk_public.inc>
#include <dal-excel/auto/MG_DupireParameterAdjoints_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_DupireParameterAdjoints_New_public.inc>
#include <dal-excel/auto/MG_DupireQuoteRisk_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_DupireQuoteRisk_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_DupireQuoteRisk_New_public.inc>
#include <dal-excel/auto/MG_DupireRiskInputs_New_public.inc>
#include <dal-excel/auto/MG_DupireScriptQuoteRisk_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_DupireScriptQuoteRisk_Get_QuoteRisk_public.inc>
#include <dal-excel/auto/MG_DupireScriptQuoteRisk_Get_Valuation_public.inc>
#include <dal-excel/auto/MG_DupireScriptQuoteRisk_New_public.inc>
#include <dal-excel/auto/MG_MertonIVS_New_public.inc>
#endif
} // namespace Dal
