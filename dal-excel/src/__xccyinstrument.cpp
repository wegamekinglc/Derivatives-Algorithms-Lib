#include "__xccyinstrument.hpp"
#include "__calibrationinstrumentinput.hpp"
#include "__platform.hpp"
#include "__scriptinput.hpp"

/*IF--------------------------------------------------------------------------
public XccyScriptContext_New
    Create an immutable NativeXccyTimestamp context
&inputs
settings is cell[][]
    Exact field/value rows with explicit currency pair, collateral, route and timestamp
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyScriptContext_New", "settings");
&outputs
value is handle StorableXccyScriptContext
    Owning session context
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyScriptImportSettings_New
    Create immutable XCCY import settings
&inputs
settings is cell[][]
    Exact instrument_id, discount_route and optional orientation/annuity fields
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyScriptImportSettings_New", "settings");
&outputs
value is handle StorableXccyScriptImportSettings
    Owning session settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyInstrumentCalibrationSpec_New
    Create immutable staged XCCY instrument calibration inputs
&inputs
settings is cell[][]
    Exact field/value rows; nested context, blocks, observations, dates and solver are handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyInstrumentCalibrationSpec_New", "settings");
&outputs
value is handle StorableXccyInstrumentCalibrationSpec
    Owning session spec
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyScriptInstrument_New
    Import native XCCY terms once into shared script cashflows
&inputs
native is handle StorableCrossCurrencySwap
    Native import input
settings is handle StorableXccyScriptImportSettings
    Immutable import settings
&outputs
value is handle CalibrationInstrument
    Shared immutable instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_XccyInstruments
    Fit staged XCCY basis from shared observations
&inputs
spec is handle StorableXccyInstrumentCalibrationSpec
    Frozen calibration inputs
&optional
options is handle StorableCurveInstrumentCalibrationOptions
    Deterministic evaluation and raw derivative settings
&outputs
outcome is handle StorableCalibrationOutcome
    Owning status, report and nullable validated basis curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_Get_XccyCapabilities
    Query admitted and rejected XCCY execution capabilities
&inputs
instrument is handle CalibrationInstrument
    Shared immutable instrument
measureId is string
    Price or BasisSpread
context is handle StorableXccyScriptContext
    Explicit NativeXccyTimestamp context
&outputs
report is handle CalibrationReport
    Plan attempts including the MC unsupported reason
-IF-------------------------------------------------------------------------*/

namespace Dal::Excel::Calibration {
    template <> struct Input_<DateTime_> {
        static DateTime_ Read(const Cell_& cell) {
            if (Cell::IsDateTime(cell))
                return Cell::ToDateTime(cell);
            if (Cell::IsDate(cell) || Cell::IsString(cell))
                return DateTime_(Input_<Date_>::Read(cell));
            const double serial = Input_<double>::Read(cell), day = std::floor(serial);
            REQUIRE(day >= 0. && day <= INT_MAX, "InvalidCalibrationInput: valuation_time outside date range");
            return DateTime_(Date::FromExcel(static_cast<int>(day)), serial - day);
        }
    };
} // namespace Dal::Excel::Calibration

namespace Dal {
    namespace {
        template <class T_> const T_& Settings(const Handle_<StorableCalibrationValue_<T_>>& value) {
            REQUIRE(value, "InvalidCalibrationInput: nonnull typed XCCY settings");
            return value->val_;
        }
    } // namespace
    void XccyScriptContext_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyScriptContext_>* value) {
        using namespace Excel::Calibration;
        Fields_ f(settings);
        XccyScriptContext_ v;
        v.valuationTime_ = f.Required<DateTime_>("valuation_time");
        v.pair_ = f.Required<Handle_<StorableCurrencyPair_>>("pair")->val_;
        v.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
        v.discountRoute_ = f.Required<String_>("discount_route");
        const auto fixings = f.Maybe<Handle_<StorableMarketFixingSnapshot_>>("fixings");
        if (fixings)
            v.fixings_ = (*fixings)->val_;
        v.todayFixingOverride_ = f.Maybe<TodayFixingPolicy_>("today_fixing_override");
        f.Finish();
        value->reset(new StorableXccyScriptContext_(v));
    }
    void XccyScriptImportSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyScriptImportSettings_>* value) {
        using namespace Excel::Calibration;
        Fields_ f(settings);
        XccyScriptImportSettings_ v;
        v.instrumentId_ = f.Required<String_>("instrument_id");
        v.discountRoute_ = f.Required<String_>("discount_route");
        v.receiveNonSpreadPaySpread_ = f.Default<bool>("receive_nonspread_pay_spread", true);
        v.minAbsAnnuity_ = f.Default<double>("min_abs_annuity", 1e-12);
        f.Finish();
        value->reset(new StorableXccyScriptImportSettings_(v));
    }
    void XccyInstrumentCalibrationSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableXccyInstrumentCalibrationSpec_>* value) {
        using namespace Excel::Calibration;
        Fields_ f(settings);
        XccyInstrumentCalibrationSpec_ v;
        v.context_ = f.Required<XccyScriptContext_>("context");
        v.domesticCurveBlock_ = f.Required<Handle_<StorableCurveBlock_>>("domestic_curve_block")->val_;
        v.foreignCurveBlock_ = f.Required<Handle_<StorableCurveBlock_>>("foreign_curve_block")->val_;
        v.fxSpot_ = f.Required<double>("fx_spot");
        v.knotDates_ = f.Required<Vector_<Date_>>("knot_dates");
        v.observations_ = f.Required<Vector_<Handle_<CalibrationObservation_>>>("observations");
        v.solver_ = f.Default<CurveInstrumentSolver_>("solver", v.solver_);
        f.Finish();
        value->reset(new StorableXccyInstrumentCalibrationSpec_(v));
    }
    void XccyScriptInstrument_New(const Handle_<StorableCrossCurrencySwap_>& native,
                                  const Handle_<StorableXccyScriptImportSettings_>& settings,
                                  Handle_<CalibrationInstrument_>* value) {
        REQUIRE(native, "InvalidCalibrationInput: nonnull native XCCY handle");
        *value = NewXccyScriptInstrument(native->val_, Settings(settings));
    }
    void Calibrate_XccyInstruments(const Handle_<StorableXccyInstrumentCalibrationSpec_>& spec,
                                   const Handle_<StorableCurveInstrumentCalibrationOptions_>& options,
                                   Handle_<StorableCalibrationOutcome_>* outcome) {
        const auto& input = Settings(spec);
        const auto configuration = options ? options->val_ : CurveInstrumentCalibrationOptions_();
        try {
            const auto result = CalibrateXccyInstrumentMarket(input, configuration);
            outcome->reset(new StorableCalibrationOutcome_("Succeeded", result.report_, result.curve_));
        } catch (const CalibrationError_& error) {
            const auto candidate = handle_cast<XccyInstrumentCalibrationCandidate_>(error.Failure().candidate_);
            outcome->reset(new StorableCalibrationOutcome_("Failed", error.Failure().report_,
                                                           candidate ? candidate->Result().curve_ : Handle_<DiscountCurve_>()));
        }
    }
    void CalibrationInstrument_Get_XccyCapabilities(const Handle_<CalibrationInstrument_>& instrument,
                                                    const String_& measureId,
                                                    const Handle_<StorableXccyScriptContext_>& context,
                                                    Handle_<CalibrationReport_>* report) {
        *report = XccyInstrumentCapabilities(instrument, measureId, Settings(context));
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_XccyInstruments_public.inc>
#include <dal-excel/auto/MG_CalibrationInstrument_Get_XccyCapabilities_public.inc>
#include <dal-excel/auto/MG_XccyInstrumentCalibrationSpec_New_public.inc>
#include <dal-excel/auto/MG_XccyScriptContext_New_public.inc>
#include <dal-excel/auto/MG_XccyScriptImportSettings_New_public.inc>
#include <dal-excel/auto/MG_XccyScriptInstrument_New_public.inc>
#endif
} // namespace Dal
