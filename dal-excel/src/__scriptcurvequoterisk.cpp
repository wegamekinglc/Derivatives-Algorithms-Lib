#include "__scriptcurvequoterisk.hpp"

#include <functional>
#include <set>

#include "__calibrationinstrumentinput.hpp"
#include "__platform.hpp"
#include "__scriptsettingsrows.hpp"

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskAccuracy_New
    Create explicit script curve risk accuracy
&inputs
settings is cell[][]+
    Exact field/value table; residual_absolute_limits is a CalibrationNumbers handle
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptCurveQuoteRiskAccuracy_New", "settings");
&outputs
value is handle StorableScriptCurveQuoteRiskAccuracy
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskRequest_New
    Create a script curve risk request
&inputs
settings is cell[][]+
    Exact optional fields; quote_ids is a ScriptCurveQuoteIds handle
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptCurveQuoteRiskRequest_New", "settings");
&outputs
value is handle StorableScriptCurveQuoteRiskRequest
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteIds_New
    Preserve case-sensitive quote selection order
&inputs
ids is cell[]
    Ordered text quote IDs
+const Excel::ScriptSettingsInput_ normalized_ids(xl_ids); xl_ids = normalized_ids.Get();
&outputs
value is handle StorableScriptCurveQuoteIds
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskSnapshot_New
    Capture an owning script curve quote risk snapshot
&inputs
calibration is handle StorableCalibrationOutcome
    Complete curve fit outcome
accuracy is handle StorableScriptCurveQuoteRiskAccuracy
    Explicit numerical admission
&outputs
value is handle StorableScriptCurveQuoteRiskSnapshot
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationInstrument_ValueWithQuoteRisk
    Value fixed script payments with implicit quote risk
&inputs
instrument is handle CalibrationInstrument
    Fixed bound contract
snapshot is handle StorableScriptCurveQuoteRiskSnapshot
    Owning fitted snapshot
context is handle StorableCurveInstrumentCalibrationSpec
    Explicit current context
&optional
request is handle StorableScriptCurveQuoteRiskRequest
    Optional quote selection and Price measure
&outputs
value is handle StorableScriptCurveQuoteRiskResult
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskSnapshot_Get_Status
    Read owning script curve quote risk Status
&inputs
snapshot is handle StorableScriptCurveQuoteRiskSnapshot
    Owning risk handle
&outputs
value is string
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskSnapshot_Get_Summary
    Read owning script curve quote risk Summary
&inputs
snapshot is handle StorableScriptCurveQuoteRiskSnapshot
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskSnapshot_Get_QuoteAxis
    Read owning script curve quote risk QuoteAxis
&inputs
snapshot is handle StorableScriptCurveQuoteRiskSnapshot
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskSnapshot_Get_Diagnostics
    Read owning script curve quote risk Diagnostics
&inputs
snapshot is handle StorableScriptCurveQuoteRiskSnapshot
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_Status
    Read owning script curve quote risk Status
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is string
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_Summary
    Read owning script curve quote risk Summary
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_QuoteAxis
    Read owning script curve quote risk QuoteAxis
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_Diagnostics
    Read owning script curve quote risk Diagnostics
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_Sensitivity
    Read owning script curve quote risk Sensitivity
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is cell[][]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_PV
    Read owning script curve quote risk PV
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is number
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_RawSensitivity
    Read owning script curve quote risk RawSensitivity
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is number[]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptCurveQuoteRiskResult_Get_DV01
    Read owning script curve quote risk DV01
&inputs
result is handle StorableScriptCurveQuoteRiskResult
    Owning risk handle
&outputs
value is number[]
    Owning result; unavailable table metrics remain blank
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        using Readers_ = std::map<std::string, std::function<void(const Cell_&)>>;
        void
        ReadFields(const Matrix_<Cell_>& settings, const String_& function, const Readers_& readers, std::initializer_list<const char*> required) {
            std::set<std::string> seen;
            Excel::ReadRows(settings, function, "settings", [&](const String_& key, const Cell_& cell, const String_&, const String_& location) {
                const auto name = Excel::Calibration::Bytes(key);
                const auto reader = readers.find(name);
                REQUIRE(reader != readers.end(), location + "unknown field");
                seen.insert(name);
                try {
                    reader->second(cell);
                } catch (const Exception_& error) {
                    THROW(location + String_(error.what()));
                }
            });
            for (const auto* key : required)
                REQUIRE(seen.count(key), function + "; settings field=" + key + "; missing required field");
        }
        template <class T_> const T_& Value(const Handle_<StorableCalibrationValue_<T_>>& handle) {
            REQUIRE(handle, "InvalidScriptCurveQuoteRiskInput: required typed handle");
            return handle->val_;
        }
        String_ Status(bool available) { return available ? "Available" : "Unavailable"; }
    } // namespace

    void ScriptCurveQuoteRiskAccuracy_New(const Matrix_<Cell_>& settings, Handle_<StorableScriptCurveQuoteRiskAccuracy_>* value) {
        ScriptCurveQuoteRiskAccuracy_ accuracy;
        using namespace Excel::Calibration;
        ReadFields(settings, "ScriptCurveQuoteRiskAccuracy_New",
                   {{"residual_absolute_limits", [&](const Cell_& c) { accuracy.residualAbsoluteLimits_ = Input_<Vector_<>>::Read(c); }},
                    {"min_reciprocal_condition", [&](const Cell_& c) { accuracy.minReciprocalCondition_ = Input_<double>::Read(c); }},
                    {"response_backward_error_limit", [&](const Cell_& c) { accuracy.responseBackwardErrorLimit_ = Input_<double>::Read(c); }},
                    {"relative_pivot_tolerance", [&](const Cell_& c) { accuracy.relativePivotTolerance_ = Input_<double>::Read(c); }}},
                   {"residual_absolute_limits", "min_reciprocal_condition", "response_backward_error_limit"});
        value->reset(new StorableScriptCurveQuoteRiskAccuracy_(accuracy));
    }
    void ScriptCurveQuoteRiskRequest_New(const Matrix_<Cell_>& settings, Handle_<StorableScriptCurveQuoteRiskRequest_>* value) {
        ScriptCurveQuoteRiskRequest_ request;
        using namespace Excel::Calibration;
        ReadFields(settings, "ScriptCurveQuoteRiskRequest_New",
                   {{"quote_ids", [&](const Cell_& c) { request.quoteIds_ = Input_<Vector_<String_>>::Read(c); }},
                    {"measure_id", [&](const Cell_& c) { request.measureId_ = Input_<String_>::Read(c); }},
                    {"live_quote_payoff", [&](const Cell_& c) { request.liveQuotePayoff_ = Input_<bool>::Read(c); }}},
                   {});
        value->reset(new StorableScriptCurveQuoteRiskRequest_(request));
    }
    void ScriptCurveQuoteIds_New(const Vector_<Cell_>& ids, Handle_<StorableScriptCurveQuoteIds_>* value) {
        Vector_<String_> result;
        for (size_t i = 0; i < ids.size(); ++i) {
            const auto location = Excel::ScriptSettingLocation("ScriptCurveQuoteIds_New", "ids", static_cast<int>(i) + 1, 1);
            REQUIRE(Cell::IsString(ids[i]), location + "expected text quote ID");
            result.push_back(Excel::Calibration::Input_<String_>::Read(ids[i]));
        }
        value->reset(new StorableScriptCurveQuoteIds_(result));
    }
    void ScriptCurveQuoteRiskSnapshot_New(const Handle_<StorableCalibrationOutcome_>& calibration,
                                          const Handle_<StorableScriptCurveQuoteRiskAccuracy_>& accuracy,
                                          Handle_<StorableScriptCurveQuoteRiskSnapshot_>* value) {
        REQUIRE(calibration && calibration->curveFit_, "ScriptCurveQuoteRiskSnapshot_New: calibration; expected complete curve fit outcome");
        value->reset(new StorableScriptCurveQuoteRiskSnapshot_(NewScriptCurveQuoteRiskSnapshot(*calibration->curveFit_, Value(accuracy))));
    }
    void CalibrationInstrument_ValueWithQuoteRisk(const Handle_<CalibrationInstrument_>& instrument,
                                                  const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot,
                                                  const Handle_<StorableCurveInstrumentCalibrationSpec_>& context,
                                                  const Handle_<StorableScriptCurveQuoteRiskRequest_>& request,
                                                  Handle_<StorableScriptCurveQuoteRiskResult_>* value) {
        value->reset(new StorableScriptCurveQuoteRiskResult_(ValueCalibrationInstrumentWithQuoteRisk(
            instrument, Value(snapshot), Value(context), request ? request->val_ : ScriptCurveQuoteRiskRequest_())));
    }
    void ScriptCurveQuoteRiskSnapshot_Get_Status(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot, String_* value) {
        *value = Status(Value(snapshot).RiskAvailable());
    }
    void ScriptCurveQuoteRiskSnapshot_Get_Summary(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(snapshot), "Summary");
    }
    void ScriptCurveQuoteRiskSnapshot_Get_QuoteAxis(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(snapshot), "QuoteAxis");
    }
    void ScriptCurveQuoteRiskSnapshot_Get_Diagnostics(const Handle_<StorableScriptCurveQuoteRiskSnapshot_>& snapshot, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(snapshot), "Diagnostics");
    }
    void ScriptCurveQuoteRiskResult_Get_Status(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, String_* value) {
        *value = Status(Value(result).Available());
    }
    void ScriptCurveQuoteRiskResult_Get_Summary(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(result), "Summary");
    }
    void ScriptCurveQuoteRiskResult_Get_QuoteAxis(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(result), "QuoteAxis");
    }
    void ScriptCurveQuoteRiskResult_Get_Diagnostics(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(result), "Diagnostics");
    }
    void ScriptCurveQuoteRiskResult_Get_Sensitivity(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Matrix_<Cell_>* value) {
        *value = ScriptCurveQuoteRiskTable(Value(result), "Sensitivity");
    }
    void ScriptCurveQuoteRiskResult_Get_PV(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, double* value) { *value = Value(result).PV(); }
    void ScriptCurveQuoteRiskResult_Get_RawSensitivity(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Vector_<>* value) {
        *value = Value(result).RawSensitivity();
    }
    void ScriptCurveQuoteRiskResult_Get_DV01(const Handle_<StorableScriptCurveQuoteRiskResult_>& result, Vector_<>* value) {
        *value = Value(result).DV01();
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrationInstrument_ValueWithQuoteRisk_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteIds_New_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskAccuracy_New_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_DV01_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_Diagnostics_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_PV_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_QuoteAxis_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_RawSensitivity_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_Sensitivity_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_Status_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskResult_Get_Summary_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskSnapshot_Get_Diagnostics_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskSnapshot_Get_QuoteAxis_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskSnapshot_Get_Status_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskSnapshot_Get_Summary_public.inc>
#include <dal-excel/auto/MG_ScriptCurveQuoteRiskSnapshot_New_public.inc>
#endif
} // namespace Dal
