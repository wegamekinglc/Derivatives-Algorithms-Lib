#include "__calibrationinstrumentinput.hpp"

#include "__dupireinput.hpp"
#include "__platform.hpp"
#include "__script_test_api.hpp"
#include "__scriptinput.hpp"

/*IF--------------------------------------------------------------------------
public CalibrationDates_New
    Owning typed script calibration interface
&inputs
dates is cell[]
    Typed dates
+const Excel::ScriptSettingsInput_ normalized_dates(xl_dates); xl_dates = normalized_dates.Get();
&outputs
value is handle StorableCalibrationDates
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationNumbers_New
    Owning typed script calibration interface
&inputs
numbers is cell[]
    Typed numbers
+const Excel::ScriptSettingsInput_ normalized_numbers(xl_numbers); xl_numbers = normalized_numbers.Get();
&outputs
value is handle StorableCalibrationNumbers
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptRoutes_New
    Owning typed script calibration interface
&inputs
routes is cell[][]+
    Typed routes
+const Excel::ScriptSettingsInput_ normalized_routes(xl_routes); xl_routes = normalized_routes.Get();
&outputs
value is handle StorableCurveScriptRoutes
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationDiscountCurves_New
    Owning typed script calibration interface
&inputs
bindings is cell[][]+
    Typed bindings
+const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
&outputs
value is handle StorableCalibrationDiscountCurves
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationForwardCurves_New
    Owning typed script calibration interface
&inputs
bindings is cell[][]+
    Typed bindings
+const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
&outputs
value is handle StorableCalibrationForwardCurves
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationObservations_New
    Owning typed script calibration interface
&inputs
observations is handle[]
    Typed observations
&outputs
value is handle StorableCalibrationObservations
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptInstrumentDefinition_New
    Owning typed script calibration interface
&inputs
source is cell[][]+
    Typed source
+const Excel::ScriptSettingsInput_ normalized_source(xl_source); xl_source = normalized_source.Get();
header is cell[][]+
    Typed header
+const Excel::ScriptSettingsInput_ normalized_header(xl_header); xl_header = normalized_header.Get();
+argName = "header"; Excel::ValidateScriptSettingsRange(xl_header, "ScriptInstrumentDefinition_New", "header");
parameters is cell[][]+
    Typed parameters
+const Excel::ScriptSettingsInput_ normalized_parameters(xl_parameters); xl_parameters = normalized_parameters.Get();
measures is cell[][]+
    Typed measures
+const Excel::ScriptSettingsInput_ normalized_measures(xl_measures); xl_measures = normalized_measures.Get();
&optional
settings is handle StorableScriptProductSettings
    Typed settings
&outputs
definition is handle ScriptInstrumentDefinition
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCalibrationInstrument_New
    Owning typed script calibration interface
&inputs
id is string
    Typed id
+argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
definition is handle ScriptInstrumentDefinition
    Typed definition
&optional
bindings is cell[][]+
    Typed bindings
+const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
pillar is cell
    Typed pillar
+const Excel::ScriptSettingsInput_ normalized_pillar(xl_pillar); xl_pillar = normalized_pillar.Get();
&outputs
instrument is handle CalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_BindParameters
    Owning typed script calibration interface
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
bindings is cell[][]+
    Typed bindings
+const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
&outputs
result is handle CalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DepositScriptInstrument_New
    Owning typed script calibration interface
&inputs
deposit is handle StorableYCInstrument
    Typed deposit
settings is handle StorableScriptInstrumentImportSettings
    Typed settings
&outputs
instrument is handle CalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanRateOptionScriptInstrument_New
    Owning typed script calibration interface
&inputs
option is handle StorableEuropeanRateOption
    Typed option
settings is handle StorableScriptInstrumentImportSettings
    Typed settings
&outputs
instrument is handle CalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationMarketQuote_New
    Owning typed script calibration interface
&inputs
id is string
    Typed id
+argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
value is cell
    Typed value
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
unit is string
    Typed unit
+argName = "unit"; Excel::ValidateNativeTextInput(xl_unit, "/unit", "InvalidCalibrationInput");
&optional
convention is cell[][]+
    Typed convention
+const Excel::ScriptSettingsInput_ normalized_convention(xl_convention); xl_convention = normalized_convention.Get();
+argName = "convention"; Excel::ValidateScriptSettingsRange(xl_convention, "CalibrationMarketQuote_New", "convention");
&outputs
quote is handle StorableCalibrationMarketQuote
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationObservation_New
    Owning typed script calibration interface
&inputs
id is string
    Typed id
+argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
instrument is handle CalibrationInstrument
    Typed instrument
measureId is string
    Typed measureId
+argName = "measureId"; Excel::ValidateNativeTextInput(xl_measureId, "/measureId", "InvalidCalibrationInput");
quote is handle StorableCalibrationMarketQuote
    Typed quote
errorScale is cell
    Typed errorScale
+const Excel::ScriptSettingsInput_ normalized_errorScale(xl_errorScale); xl_errorScale = normalized_errorScale.Get();
&outputs
observation is handle StorableCalibrationObservation
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_CurveInstruments
    Owning typed script calibration interface
&inputs
spec is handle StorableCurveInstrumentCalibrationSpec
    Typed spec
&optional
options is handle StorableCurveInstrumentCalibrationOptions
    Typed options
&outputs
outcome is handle StorableCalibrationOutcome
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_ValueByMonteCarlo
    Owning typed script calibration interface
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
model is handle ModelData
    Typed model
request is handle StorableCalibrationMonteCarloRequest
    Typed request
&outputs
outcome is handle StorableCalibrationOutcome
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationPlan_New
    Owning typed script calibration interface
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
measureId is string
    Typed measureId
+argName = "measureId"; Excel::ValidateNativeTextInput(xl_measureId, "/measureId", "InvalidCalibrationInput");
context is handle StorableCalibrationPreparationContext
    Typed context
&optional
options is handle StorableCalibrationEvaluationOptions
    Typed options
&outputs
plan is handle StorablePreparedCalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOutcome_Get_Report
    Owning typed script calibration interface
&inputs
outcome is handle StorableCalibrationOutcome
    Typed outcome
&outputs
report is handle CalibrationReport
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOutcome_Get_Status
    Owning typed script calibration interface
&inputs
outcome is handle StorableCalibrationOutcome
    Typed outcome
&outputs
status is string
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOutcome_Get_Curve
    Owning typed script calibration interface
&inputs
outcome is handle StorableCalibrationOutcome
    Typed outcome
&outputs
curve is handle StorableDiscountCurve
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOutcome_Get_Mean
    Owning typed script calibration interface
&inputs
outcome is handle StorableCalibrationOutcome
    Typed outcome
&outputs
mean is number
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Json
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
json is string[]
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_QuoteJacobian
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is number[][]
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_QuoteMap
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is number[][]
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptInstrumentDefinition_Get_Json
    Owning typed script calibration interface
&inputs
definition is handle ScriptInstrumentDefinition
    Typed definition
&outputs
json is string[]
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptInstrumentDefinition_LoadJson
    Owning typed script calibration interface
&inputs
json is string
    Typed json
+argName = "json"; Excel::ValidateNativeTextInput(xl_json, "/json", "InvalidCalibrationInput");
&outputs
definition is handle ScriptInstrumentDefinition
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_Get_Json
    Owning typed script calibration interface
&inputs
instrument is handle CalibrationInstrument
    Typed instrument
&outputs
json is string[]
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_LoadJson
    Owning typed script calibration interface
&inputs
json is string
    Typed json
+argName = "json"; Excel::ValidateNativeTextInput(xl_json, "/json", "InvalidCalibrationInput");
&outputs
instrument is handle CalibrationInstrument
    Owning result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Summary
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Observations
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Quotes
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Parameters
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Plans
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationReport_Get_Errors
    Owning typed script calibration interface
&inputs
report is handle CalibrationReport
    Typed report
&outputs
value is cell[][]
    Owning result
+const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationPlan_Get_PlanIdentity
    Inspect an immutable admitted preparation plan
&inputs
plan is handle StorablePreparedCalibrationInstrument
    Prepared instrument plan
&outputs
value is string
    Content identity of the admitted plan
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public FraScriptInstrument_New
    Import resolved deterministic native curve cashflows once
&inputs
native is handle StorableYCInstrument
    Matching native product
payment is cell
    Explicit FRA payment date or integral serial
settings is handle StorableScriptInstrumentImportSettings
    Explicit units, route, notional and annuity floor
&outputs
instrument is handle CalibrationInstrument
    Immutable shared definition and binding
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public SwapScriptInstrument_New
    Import resolved deterministic native curve cashflows once
&inputs
native is handle StorableYCInstrument
    Matching native product
settings is handle StorableScriptInstrumentImportSettings
    Explicit units, route, notional and annuity floor
&outputs
instrument is handle CalibrationInstrument
    Immutable shared definition and binding
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public PeriodForwardOisScriptInstrument_New
    Import resolved deterministic native curve cashflows once
&inputs
native is handle StorableYCInstrument
    Matching native product
settings is handle StorableScriptInstrumentImportSettings
    Explicit units, route, notional and annuity floor
&outputs
instrument is handle CalibrationInstrument
    Immutable shared definition and binding
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BasisSwapScriptInstrument_New
    Import resolved deterministic native curve cashflows once
&inputs
native is handle StorableYCInstrument
    Matching native product
settings is handle StorableScriptInstrumentImportSettings
    Explicit units, route, notional and annuity floor
&outputs
instrument is handle CalibrationInstrument
    Immutable shared definition and binding
-IF-------------------------------------------------------------------------*/
namespace Dal {
    namespace {
        using Excel::Calibration::Fields_;
        using Excel::Calibration::Rows_;
        template <class T_> const T_& Settings(const Handle_<StorableCalibrationValue_<T_>>& value) {
            REQUIRE(value, "InvalidCalibrationInput: missing typed settings handle");
            return value->val_;
        }
        bool Empty(const Matrix_<Cell_>& cells) {
            return cells.Rows() == 0 || (cells.Rows() == 1 && cells.Cols() == 1 && Cell::IsEmpty(cells(0, 0)));
        }
        CalibrationParameterBindings_ Bindings(const Matrix_<Cell_>& cells) {
            CalibrationParameterBindings_ result;
            if (Empty(cells))
                return result;
            const Rows_ rows(cells, {"key", "value", "unit", "quote_id", "quote_fingerprint"});
            for (int i = 0; i < rows.Size(); ++i) {
                const auto key = rows.Get<String_>(i, "key");
                result.values_.push_back({key, rows.Get<double>(i, "value"), rows.Get<CalibrationValueUnit_>(i, "unit")});
                const auto id = rows.Maybe<String_>(i, "quote_id"), fingerprint = rows.Maybe<String_>(i, "quote_fingerprint");
                if (id || fingerprint)
                    result.quoteCopies_.push_back({key, id.value_or(""), fingerprint.value_or("")});
            }
            return result;
        }
        ScriptInstrumentSource_ Source(const Matrix_<Cell_>& cells) {
            ScriptInstrumentSource_ value;
            const Rows_ rows(cells, {"kind", "key", "text"});
            for (int i = 0; i < rows.Size(); ++i) {
                const auto kind = rows.Get<ScriptSourceRowKind_>(i, "kind");
                const auto key =
                    kind == ScriptSourceRowKind_::Value_::Event ? Date::ToString(rows.Get<Date_>(i, "key")) : rows.Get<String_>(i, "key");
                value.rows_.push_back({kind, key, rows.Get<String_>(i, "text")});
            }
            return value;
        }
        ScriptInstrumentHeader_ Header(const Matrix_<Cell_>& cells) {
            Fields_ f(cells);
            ScriptInstrumentHeader_ value;
            value.payCurrency_ = f.Required<Ccy_>("pay_currency");
            value.resultCurrency_ = f.Required<Ccy_>("result_currency");
            value.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
            value.collateralType_ = f.Default<CollateralType_>("collateral_type", value.collateralType_);
            value.receiver_ = f.Default<String_>("receiver", value.receiver_);
            value.notionalParameter_ = f.Default<String_>("notional_parameter", value.notionalParameter_);
            value.settlement_ = f.Default<ScriptSettlement_>("settlement", value.settlement_);
            value.valuationCashflows_ = f.Default<ScriptValuationCashflows_>("valuation_cashflows", value.valuationCashflows_);
            value.discountRoute_ = f.Required<String_>("discount_route");
            value.conventionSet_ = f.Default<String_>("convention_set", value.conventionSet_);
            f.Finish();
            return value;
        }
        Vector_<ScriptParameterDeclaration_> Parameters(const Matrix_<Cell_>& cells) {
            const Rows_ rows(cells, {"key", "type", "unit", "role", "bindable", "lower", "upper", "lower_inclusive", "upper_inclusive"});
            Vector_<ScriptParameterDeclaration_> value;
            for (int i = 0; i < rows.Size(); ++i)
                value.push_back({rows.Get<String_>(i, "key"), rows.Get<ScriptParameterType_>(i, "type"), rows.Get<CalibrationValueUnit_>(i, "unit"),
                                 rows.Get<ScriptParameterRole_>(i, "role"), rows.Get<bool>(i, "bindable"), rows.Maybe<double>(i, "lower"),
                                 rows.Maybe<double>(i, "upper"), rows.Get<bool>(i, "lower_inclusive"), rows.Get<bool>(i, "upper_inclusive")});
            return value;
        }
        Vector_<CalibrationMeasureSpec_> Measures(const Matrix_<Cell_>& cells) {
            const Rows_ rows(cells, {"id", "kind", "receiver", "parameter", "unit", "annuity_sign", "min_abs_annuity"});
            Vector_<CalibrationMeasureSpec_> value;
            for (int i = 0; i < rows.Size(); ++i)
                value.push_back({rows.Get<String_>(i, "id"), rows.Get<CalibrationMeasureKind_>(i, "kind"), rows.Get<String_>(i, "receiver"),
                                 rows.Maybe<String_>(i, "parameter"), rows.Get<CalibrationValueUnit_>(i, "unit"), rows.Maybe<int>(i, "annuity_sign"),
                                 rows.Maybe<double>(i, "min_abs_annuity")});
            return value;
        }
        Handle_<StorableCalibrationOutcome_> FailureOutcome(const CalibrationError_& error) {
            const auto& failure = error.Failure();
            Handle_<DiscountCurve_> curve;
            if (const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(failure.candidate_.get()))
                curve = candidate->Result().curve_;
            return Handle_<StorableCalibrationOutcome_>(new StorableCalibrationOutcome_("Failed", failure.report_, curve));
        }
        template <class K_, class T_> void CurveBindings(const Matrix_<Cell_>& bindings, Handle_<T_>* value) {
            const Rows_ rows(bindings, {"key", "curve"});
            std::map<K_, Handle_<DiscountCurve_>> result;
            for (int i = 0; i < rows.Size(); ++i)
                REQUIRE(result.emplace(rows.Get<K_>(i, "key"), rows.Get<Handle_<DiscountCurve_>>(i, "curve")).second,
                        "InvalidCalibrationInput: duplicate curve key");
            value->reset(new T_(result));
        }
    } // namespace
    void FraScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                 const Cell_& payment,
                                 const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                 Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(native && native->val_, "InvalidCalibrationInput: expected native curve instrument");
        *instrument = NewFraScriptInstrument(native->val_, Excel::Calibration::Input_<Date_>::Read(payment), Settings(settings));
    }
    void SwapScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                  const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                  Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(native && native->val_, "InvalidCalibrationInput: expected native curve instrument");
        *instrument = NewSwapScriptInstrument(native->val_, Settings(settings));
    }
    void PeriodForwardOisScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                              const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                              Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(native && native->val_, "InvalidCalibrationInput: expected native curve instrument");
        *instrument = NewPeriodForwardOisScriptInstrument(native->val_, Settings(settings));
    }
    void BasisSwapScriptInstrument_New(const Handle_<StorableYCInstrument_>& native,
                                       const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                       Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(native && native->val_, "InvalidCalibrationInput: expected native curve instrument");
        *instrument = NewBasisSwapScriptInstrument(native->val_, Settings(settings));
    }
    void CalibrationDates_New(const Vector_<Cell_>& dates, Handle_<StorableCalibrationDates_>* value) {
        Vector_<Date_> result;
        for (const auto& date : dates)
            result.push_back(Excel::Calibration::Input_<Date_>::Read(date));
        value->reset(new StorableCalibrationDates_(result));
    }
    void CalibrationNumbers_New(const Vector_<Cell_>& numbers, Handle_<StorableCalibrationNumbers_>* value) {
        Vector_<> result;
        for (const auto& number : numbers)
            result.push_back(Excel::Calibration::Input_<double>::Read(number));
        value->reset(new StorableCalibrationNumbers_(result));
    }
    void CurveScriptRoutes_New(const Matrix_<Cell_>& routes, Handle_<StorableCurveScriptRoutes_>* value) {
        const Rows_ rows(routes, {"id", "projection", "collateral", "tenor"});
        Vector_<CurveScriptRoute_> result;
        for (int i = 0; i < rows.Size(); ++i) {
            const auto tenor = rows.Maybe<PeriodLength_>(i, "tenor");
            result.push_back({rows.Get<String_>(i, "id"), rows.Get<bool>(i, "projection"), rows.Get<CollateralType_>(i, "collateral"),
                              tenor.value_or(PeriodLength_())});
        }
        value->reset(new StorableCurveScriptRoutes_(result));
    }
    void CalibrationDiscountCurves_New(const Matrix_<Cell_>& bindings, Handle_<StorableCalibrationDiscountCurves_>* value) {
        CurveBindings<CollateralType_>(bindings, value);
    }
    void CalibrationForwardCurves_New(const Matrix_<Cell_>& bindings, Handle_<StorableCalibrationForwardCurves_>* value) {
        CurveBindings<PeriodLength_>(bindings, value);
    }
    void CalibrationObservations_New(const Vector_<Handle_<Storable_>>& observations, Handle_<StorableCalibrationObservations_>* value) {
        Vector_<Handle_<CalibrationObservation_>> result;
        for (const auto& observation : observations) {
            const auto typed = handle_cast<StorableCalibrationObservation_>(observation);
            REQUIRE(typed && typed->val_, "InvalidCalibrationInput: expected observation handle");
            result.push_back(typed->val_);
        }
        value->reset(new StorableCalibrationObservations_(result));
    }
    void ScriptInstrumentDefinition_New(const Matrix_<Cell_>& source,
                                        const Matrix_<Cell_>& header,
                                        const Matrix_<Cell_>& parameters,
                                        const Matrix_<Cell_>& measures,
                                        const Handle_<StorableScriptProductSettings_>& settings,
                                        Handle_<ScriptInstrumentDefinition_>* definition) {
        ScriptInstrumentDefinitionSpec_ spec{Source(source), Header(header), Parameters(parameters), Measures(measures)};
        if (settings)
            spec.source_.settings_ = settings->val_;
        *definition = NewScriptInstrumentDefinition(spec);
    }
    void ScriptCalibrationInstrument_New(const String_& id,
                                         const Handle_<ScriptInstrumentDefinition_>& definition,
                                         const Matrix_<Cell_>& bindings,
                                         const Cell_& pillar,
                                         Handle_<CalibrationInstrument_>* instrument) {
        const auto date = Cell::IsEmpty(pillar) ? std::nullopt : std::optional<Date_>(Excel::Calibration::Input_<Date_>::Read(pillar));
        *instrument = NewScriptCalibrationInstrument({id, definition, Bindings(bindings), date});
    }
    void CalibrationInstrument_BindParameters(const Handle_<CalibrationInstrument_>& instrument,
                                              const Matrix_<Cell_>& bindings,
                                              Handle_<CalibrationInstrument_>* result) {
        *result = BindCalibrationInstrumentParameters(instrument, Bindings(bindings));
    }
    void DepositScriptInstrument_New(const Handle_<StorableYCInstrument_>& deposit,
                                     const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                     Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(deposit && deposit->val_, "InvalidCalibrationInput: expected Deposit handle");
        *instrument = NewDepositScriptInstrument(deposit->val_, Settings(settings));
    }
    void EuropeanRateOptionScriptInstrument_New(const Handle_<StorableEuropeanRateOption_>& option,
                                                const Handle_<StorableScriptInstrumentImportSettings_>& settings,
                                                Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(option, "InvalidCalibrationInput: expected rate-option handle");
        *instrument = NewEuropeanRateOptionScriptInstrument(option->option_, Settings(settings));
    }
    void CalibrationMarketQuote_New(const String_& id,
                                    const Cell_& value,
                                    const String_& unit,
                                    const Matrix_<Cell_>& convention,
                                    Handle_<StorableCalibrationMarketQuote_>* quote) {
        std::optional<CalibrationQuoteConvention_> c;
        if (!Empty(convention)) {
            Fields_ f(convention);
            c = CalibrationQuoteConvention_{f.Required<CalibrationQuoteConventionId_>("id"), f.Required<int>("version")};
            f.Finish();
        }
        quote->reset(new StorableCalibrationMarketQuote_(
            NewCalibrationMarketQuote(id, Excel::Calibration::Input_<double>::Read(value), CalibrationValueUnit_::Parse(unit), c)));
    }
    void CalibrationObservation_New(const String_& id,
                                    const Handle_<CalibrationInstrument_>& instrument,
                                    const String_& measureId,
                                    const Handle_<StorableCalibrationMarketQuote_>& quote,
                                    const Cell_& errorScale,
                                    Handle_<StorableCalibrationObservation_>* observation) {
        observation->reset(new StorableCalibrationObservation_(
            NewCalibrationObservation(id, instrument, measureId, Settings(quote), Excel::Calibration::Input_<double>::Read(errorScale))));
    }
    void Calibrate_CurveInstruments(const Handle_<StorableCurveInstrumentCalibrationSpec_>& spec,
                                    const Handle_<StorableCurveInstrumentCalibrationOptions_>& options,
                                    Handle_<StorableCalibrationOutcome_>* outcome) {
        const auto& input = Settings(spec);
        const auto configuration = options ? options->val_ : CurveInstrumentCalibrationOptions_();
        try {
            const auto result = CalibrateCurveInstruments(input, configuration);
            outcome->reset(new StorableCalibrationOutcome_("Succeeded", result.report_, result.curve_));
        } catch (const CalibrationError_& error) {
            *outcome = FailureOutcome(error);
        }
    }
    void CalibrationInstrument_ValueByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                 const Handle_<ModelData_>& model,
                                                 const Handle_<StorableCalibrationMonteCarloRequest_>& request,
                                                 Handle_<StorableCalibrationOutcome_>* outcome) {
        const auto& configuration = Settings(request);
        try {
            const auto result = ValueCalibrationInstrumentByMonteCarlo(instrument, model, configuration);
            outcome->reset(new StorableCalibrationOutcome_("Succeeded", result.report_, {}, result.mean_));
        } catch (const CalibrationError_& error) {
            *outcome = FailureOutcome(error);
        }
    }
    void CalibrationPlan_New(const Handle_<CalibrationInstrument_>& instrument,
                             const String_& measureId,
                             const Handle_<StorableCalibrationPreparationContext_>& context,
                             const Handle_<StorableCalibrationEvaluationOptions_>& options,
                             Handle_<StorablePreparedCalibrationInstrument_>* plan) {
        const auto settings = options ? options->val_ : CalibrationEvaluationOptions_();
        plan->reset(new StorablePreparedCalibrationInstrument_(PrepareCalibrationInstrument(instrument, measureId, Settings(context), settings)));
    }
    void CalibrationOutcome_Get_Report(const Handle_<StorableCalibrationOutcome_>& outcome, Handle_<CalibrationReport_>* report) {
        REQUIRE(outcome && outcome->report_, "InvalidCalibrationInput: expected outcome with owning report");
        *report = outcome->report_;
    }
    void CalibrationPlan_Get_PlanIdentity(const Handle_<StorablePreparedCalibrationInstrument_>& plan, String_* value) {
        *value = Settings(plan).PlanIdentity();
    }
    void CalibrationOutcome_Get_Status(const Handle_<StorableCalibrationOutcome_>& outcome, String_* status) {
        REQUIRE(outcome, "InvalidCalibrationInput: expected outcome");
        *status = outcome->status_;
    }
    void CalibrationOutcome_Get_Curve(const Handle_<StorableCalibrationOutcome_>& outcome, Handle_<StorableDiscountCurve_>* curve) {
        REQUIRE(outcome && outcome->curve_, "CalibrationCandidateUnavailable: no curve candidate");
        curve->reset(new StorableDiscountCurve_(outcome->curve_));
    }
    void CalibrationOutcome_Get_Mean(const Handle_<StorableCalibrationOutcome_>& outcome, double* mean) {
        REQUIRE(outcome && outcome->mean_, "CalibrationCandidateUnavailable: no MC mean");
        *mean = *outcome->mean_;
    }
    void CalibrationReport_Get_Json(const Handle_<CalibrationReport_>& report, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(DumpCalibrationReportJson(report), "CalibrationReport_Get_Json");
    }
    void CalibrationReport_Get_QuoteJacobian(const Handle_<CalibrationReport_>& report, Matrix_<>* value) {
        *value = CalibrationReportQuoteJacobian(report);
    }
    void CalibrationReport_Get_QuoteMap(const Handle_<CalibrationReport_>& report, Matrix_<>* value) {
        REQUIRE(report && report->Axes().quoteMap_.Rows() > 0 && report->Axes().quoteMap_.Cols() > 0,
                "InvalidCalibrationPlan: quote map unavailable without observation and quote axes");
        *value = report->Axes().quoteMap_;
    }
    void ScriptInstrumentDefinition_Get_Json(const Handle_<ScriptInstrumentDefinition_>& definition, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(DumpScriptInstrumentDefinitionJson(definition), "ScriptInstrumentDefinition_Get_Json");
    }
    void ScriptInstrumentDefinition_LoadJson(const String_& json, Handle_<ScriptInstrumentDefinition_>* definition) {
        *definition = LoadScriptInstrumentDefinitionJson(json);
    }
    void CalibrationInstrument_Get_Json(const Handle_<CalibrationInstrument_>& instrument, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(DumpCalibrationInstrumentJson(instrument), "CalibrationInstrument_Get_Json");
    }
    void CalibrationInstrument_LoadJson(const String_& json, Handle_<CalibrationInstrument_>* instrument) {
        *instrument = LoadCalibrationInstrumentJson(json);
    }
    void CalibrationReport_Get_Summary(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "summary");
    }
    void CalibrationReport_Get_Observations(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "observations");
    }
    void CalibrationReport_Get_Quotes(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "quotes");
    }
    void CalibrationReport_Get_Parameters(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "parameters");
    }
    void CalibrationReport_Get_Plans(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "plans");
    }
    void CalibrationReport_Get_Errors(const Handle_<CalibrationReport_>& report, Matrix_<Cell_>* value) {
        *value = CalibrationReportTable(report, "errors");
    }
} // namespace Dal

#ifdef _WIN32
namespace Dal {
#include <dal-excel/auto/MG_BasisSwapScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_Calibrate_CurveInstruments_public.inc>
#include <dal-excel/auto/MG_CalibrationDates_New_public.inc>
#include <dal-excel/auto/MG_CalibrationDiscountCurves_New_public.inc>
#include <dal-excel/auto/MG_CalibrationForwardCurves_New_public.inc>
#include <dal-excel/auto/MG_CalibrationInstrument_BindParameters_public.inc>
#include <dal-excel/auto/MG_CalibrationInstrument_Get_Json_public.inc>
#include <dal-excel/auto/MG_CalibrationInstrument_LoadJson_public.inc>
#include <dal-excel/auto/MG_CalibrationInstrument_ValueByMonteCarlo_public.inc>
#include <dal-excel/auto/MG_CalibrationMarketQuote_New_public.inc>
#include <dal-excel/auto/MG_CalibrationNumbers_New_public.inc>
#include <dal-excel/auto/MG_CalibrationObservation_New_public.inc>
#include <dal-excel/auto/MG_CalibrationObservations_New_public.inc>
#include <dal-excel/auto/MG_CalibrationOutcome_Get_Curve_public.inc>
#include <dal-excel/auto/MG_CalibrationOutcome_Get_Mean_public.inc>
#include <dal-excel/auto/MG_CalibrationOutcome_Get_Report_public.inc>
#include <dal-excel/auto/MG_CalibrationOutcome_Get_Status_public.inc>
#include <dal-excel/auto/MG_CalibrationPlan_Get_PlanIdentity_public.inc>
#include <dal-excel/auto/MG_CalibrationPlan_New_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Errors_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Json_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Observations_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Parameters_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Plans_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_QuoteJacobian_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_QuoteMap_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Quotes_public.inc>
#include <dal-excel/auto/MG_CalibrationReport_Get_Summary_public.inc>
#include <dal-excel/auto/MG_CurveScriptRoutes_New_public.inc>
#include <dal-excel/auto/MG_DepositScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_EuropeanRateOptionScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_FraScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_PeriodForwardOisScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_ScriptCalibrationInstrument_New_public.inc>
#include <dal-excel/auto/MG_ScriptInstrumentDefinition_Get_Json_public.inc>
#include <dal-excel/auto/MG_ScriptInstrumentDefinition_LoadJson_public.inc>
#include <dal-excel/auto/MG_ScriptInstrumentDefinition_New_public.inc>
#include <dal-excel/auto/MG_SwapScriptInstrument_New_public.inc>
} // namespace Dal
#endif
