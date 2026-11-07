#include "__compoundedoisinput.hpp"
#include "__platform.hpp"
#include <dal-public/src/calibrationfieldsinternal.hpp>

/*IF--------------------------------------------------------------------------
public CompoundedOisFixedLeg_New
    Create immutable typed compounded OIS input
&inputs
rows is cell[][]+ or Xccy->Excel::XccyTableInput_("CompoundedOisFixedLeg_New", "rows", 4)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCompoundedOisFixedLeg
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisCoupons_New
    Create immutable typed compounded OIS input
&inputs
rows is cell[][]+ or Xccy->Excel::XccyTableInput_("CompoundedOisCoupons_New", "rows", 4)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCompoundedOisCoupons
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisObservations_New
    Create immutable typed compounded OIS input
&inputs
rows is cell[][]+ or Xccy->Excel::XccyTableInput_("CompoundedOisObservations_New", "rows", 7)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCompoundedOisObservations
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptOvernightRoutes_New
    Create immutable typed compounded OIS input
&inputs
routes is cell[][]+ or Xccy->Excel::XccyTableInput_("CurveScriptOvernightRoutes_New", "routes", 6)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCurveScriptRoutes
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOvernightCurves_New
    Create immutable typed compounded OIS input
&inputs
bindings is cell[][]+ or Xccy->Excel::XccyTableInput_("CalibrationOvernightCurves_New", "bindings", 2)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCalibrationOvernightCurves
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationStrings_New
    Create immutable typed compounded OIS input
&inputs
rows is cell[][]+ or Xccy->Excel::XccyTableInput_("CalibrationStrings_New", "rows", 1)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCalibrationStrings
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisCalendar_New
    Create immutable typed compounded OIS input
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("CompoundedOisCalendar_New", "settings", 2)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCompoundedOisCalendar
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisScriptSpec_New
    Create immutable typed compounded OIS input
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("CompoundedOisScriptSpec_New", "settings", 2)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCompoundedOisScriptSpec
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptEvaluationRequest_New
    Create immutable typed compounded OIS input
&inputs
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("CurveScriptEvaluationRequest_New", "settings", 2)
    Required named columns or exact field/value rows
&outputs
value is handle StorableCurveScriptEvaluationRequest
    Owning session input
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOis_Prepare
    Same core compounded OIS interface
&inputs
spec is handle StorableCompoundedOisScriptSpec
    Explicit contract
valuation is handle StorableScriptValuationSettings
    Explicit date policy and fixing snapshot
&outputs
value is handle StorablePreparedCompoundedOis
    Frozen validated tables
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisPreparations_New
    Same core compounded OIS interface
&inputs
tables is handle[]
    Prepared table handles
&outputs
value is handle StorableCompoundedOisPreparations
    Immutable calibration table list
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CompoundedOisScriptInstrument_New
    Same core compounded OIS interface
&inputs
tables is handle StorablePreparedCompoundedOis
    Frozen validated tables
&outputs
instrument is handle CalibrationInstrument
    Same core source instrument
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptInstrument_Evaluate
    Same core compounded OIS interface
&inputs
instrument is handle CalibrationInstrument
    Immutable source instrument
request is handle StorableCurveScriptEvaluationRequest
    Explicit native request
&outputs
outcome is handle StorableCalibrationOutcome
    Evaluated or Failed with owning report
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptEvaluation_Get_Value
    Same core compounded OIS interface
&inputs
outcome is handle StorableCalibrationOutcome
    Evaluation outcome
measureId is string
    Requested measure ID
&outputs
value is number
    Raw requested value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptEvaluation_Get_NativePartials
    Same core compounded OIS interface
&inputs
outcome is handle StorableCalibrationOutcome
    Evaluation outcome
&outputs
value is number[][]
    Raw native partials with no basis point scaling
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CurveScriptEvaluation_Get_Table
    Same core compounded OIS interface
&inputs
outcome is handle StorableCalibrationOutcome
    Evaluation outcome
table is string
    measures parameters cashflows observations or summary
&outputs
value is cell[][]
    Owning result table
-IF-------------------------------------------------------------------------*/

namespace Dal {
    using Excel::Calibration::OisRows_;
    using Excel::Calibration::XccyFields_;

    void CompoundedOisFixedLeg_New(const Matrix_<Cell_>& cells, Handle_<StorableCompoundedOisFixedLeg_>* value) {
        const OisRows_ rows(cells, "CompoundedOisFixedLeg_New", {"start", "end", "payment", "accrual"});
        value->reset(new StorableCompoundedOisFixedLeg_(rows.Read<CompoundedOisFixedRow_>([&](int i) {
            return CompoundedOisFixedRow_{rows.Get<Date_>(i, "start"), rows.Get<Date_>(i, "end"), rows.Get<Date_>(i, "payment"),
                                          rows.Get<double>(i, "accrual")};
        })));
    }
    void CompoundedOisCoupons_New(const Matrix_<Cell_>& cells, Handle_<StorableCompoundedOisCoupons_>* value) {
        const OisRows_ rows(cells, "CompoundedOisCoupons_New", {"coupon_id", "start", "end", "payment"});
        value->reset(new StorableCompoundedOisCoupons_(rows.Read<CompoundedOisCouponRow_>([&](int i) {
            return CompoundedOisCouponRow_{rows.Get<String_>(i, "coupon_id"), rows.Get<Date_>(i, "start"), rows.Get<Date_>(i, "end"),
                                           rows.Get<Date_>(i, "payment")};
        })));
    }
    void CompoundedOisObservations_New(const Matrix_<Cell_>& cells, Handle_<StorableCompoundedOisObservations_>* value) {
        const OisRows_ rows(cells, "CompoundedOisObservations_New",
                            {"start", "end", "fixing_time", "accrual", "coupon_id", "rate_start", "rate_end"});
        value->reset(new StorableCompoundedOisObservations_(rows.Read<CompoundedOisObservationRow_>([&](int i) {
            return CompoundedOisObservationRow_{rows.Get<Date_>(i, "start"),           rows.Get<Date_>(i, "end"),
                                                rows.Get<DateTime_>(i, "fixing_time"), rows.Get<double>(i, "accrual"),
                                                rows.Get<String_>(i, "coupon_id"),     rows.Get<Date_>(i, "rate_start"),
                                                rows.Get<Date_>(i, "rate_end")};
        })));
    }
    void CurveScriptOvernightRoutes_New(const Matrix_<Cell_>& cells, Handle_<StorableCurveScriptRoutes_>* value) {
        const OisRows_ rows(cells, "CurveScriptOvernightRoutes_New", {"id", "projection", "collateral", "tenor", "overnight", "use_target_curve"});
        value->reset(new StorableCurveScriptRoutes_(rows.Read<CurveScriptRoute_>([&](int i) {
            const auto tenor = rows.Maybe<PeriodLength_>(i, "tenor");
            return CurveScriptRoute_{rows.Get<String_>(i, "id"),      rows.Get<bool>(i, "projection"), rows.Get<CollateralType_>(i, "collateral"),
                                     tenor.value_or(PeriodLength_()), rows.Get<bool>(i, "overnight"),  rows.Get<bool>(i, "use_target_curve")};
        })));
    }
    void CalibrationOvernightCurves_New(const Matrix_<Cell_>& cells, Handle_<StorableCalibrationOvernightCurves_>* value) {
        const OisRows_ rows(cells, "CalibrationOvernightCurves_New", {"key", "curve"});
        const auto pairs = rows.Read<std::pair<std::string, Handle_<DiscountCurve_>>>([&](int i) {
            return std::make_pair(Excel::Calibration::Bytes(rows.Get<String_>(i, "key")), rows.Get<Handle_<DiscountCurve_>>(i, "curve"));
        });
        std::map<std::string, Handle_<DiscountCurve_>> result;
        for (const auto& pair : pairs)
            REQUIRE(result.emplace(pair).second, "InvalidCalibrationInput: duplicate overnight curve key");
        value->reset(new StorableCalibrationOvernightCurves_(result));
    }
    void CalibrationStrings_New(const Matrix_<Cell_>& cells, Handle_<StorableCalibrationStrings_>* value) {
        const OisRows_ rows(cells, "CalibrationStrings_New", {"value"});
        value->reset(new StorableCalibrationStrings_(rows.Read<String_>([&](int i) { return rows.Get<String_>(i, "value"); })));
    }
    void CompoundedOisCalendar_New(const Matrix_<Cell_>& settings, Handle_<StorableCompoundedOisCalendar_>* value) {
        XccyFields_ f(settings, "CompoundedOisCalendar_New", {"center", "holidays", "work_weekends"}, true);
        const auto center = f.Required<String_>("center");
        const auto holidays = f.Required<Vector_<Date_>>("holidays");
        const auto weekends = f.Default<Vector_<Date_>>("work_weekends", {});
        f.Finish();
        value->reset(new StorableCompoundedOisCalendar_(HolidayCenterData_(center, holidays, weekends)));
    }
    void CompoundedOisScriptSpec_New(const Matrix_<Cell_>& settings, Handle_<StorableCompoundedOisScriptSpec_>* value) {
        XccyFields_ f(settings, "CompoundedOisScriptSpec_New",
                      {"instrument_id", "direction", "discount_route", "projection_route", "fixing_index", "notional", "coupon", "currency",
                       "collateral_currency", "collateral_type", "observation_basis", "fixed_basis", "calendar", "fixed_leg", "coupons",
                       "observations", "min_abs_annuity", "rule_id"},
                      true);
        const auto spec = BindingInternal::ReadCompoundedOisSpec(f);
        f.Finish();
        value->reset(new StorableCompoundedOisScriptSpec_(spec));
    }
    void CurveScriptEvaluationRequest_New(const Matrix_<Cell_>& settings, Handle_<StorableCurveScriptEvaluationRequest_>* value) {
        XccyFields_ f(
            settings, "CurveScriptEvaluationRequest_New",
            {"geometry", "market", "valuation", "parameters", "compounded_ois", "measure_ids", "evaluation", "derivative_method", "difference_step"},
            true);
        const auto request = BindingInternal::ReadCurveScriptEvaluationRequest(f);
        f.Finish();
        value->reset(new StorableCurveScriptEvaluationRequest_(request));
    }
    void CompoundedOis_Prepare(const Handle_<StorableCompoundedOisScriptSpec_>& spec,
                               const Handle_<StorableScriptValuationSettings_>& valuation,
                               Handle_<StorablePreparedCompoundedOis_>* value) {
        REQUIRE(spec && valuation, "InvalidCalibrationInput: expected typed contract and valuation");
        value->reset(new StorablePreparedCompoundedOis_(PrepareCompoundedOis(spec->val_, valuation->val_)));
    }
    void CompoundedOisPreparations_New(const Vector_<Handle_<Storable_>>& tables, Handle_<StorableCompoundedOisPreparations_>* value) {
        Vector_<Handle_<PreparedCompoundedOis_>> result;
        for (const auto& table : tables) {
            const auto wrapper = handle_cast<StorablePreparedCompoundedOis_>(table);
            REQUIRE(wrapper && wrapper->val_, "InvalidCalibrationInput: expected prepared OIS tables");
            result.push_back(wrapper->val_);
        }
        value->reset(new StorableCompoundedOisPreparations_(result));
    }
    void CompoundedOisScriptInstrument_New(const Handle_<StorablePreparedCompoundedOis_>& tables, Handle_<CalibrationInstrument_>* instrument) {
        REQUIRE(tables && tables->val_, "InvalidCalibrationInput: expected prepared OIS tables");
        *instrument = NewCompoundedOisScriptInstrument(tables->val_);
    }
    void CurveScriptInstrument_Evaluate(const Handle_<CalibrationInstrument_>& instrument,
                                        const Handle_<StorableCurveScriptEvaluationRequest_>& request,
                                        Handle_<StorableCalibrationOutcome_>* outcome) {
        REQUIRE(instrument && request, "InvalidCalibrationInput: expected source instrument and typed request");
        try {
            const auto result = std::make_shared<const CurveScriptEvaluationResult_>(EvaluateCurveScriptInstrument(instrument, request->val_));
            outcome->reset(new StorableCalibrationOutcome_("Evaluated", result->report_, result->curve_, {}, {}, {}, result));
        } catch (const CalibrationError_& error) {
            const auto& failure = error.Failure();
            const auto& code = failure.diagnostic_.code_;
            if (code != "MissingFixing" && code != "CalibrationNumericalFailure" && failure.diagnostic_.stage_ != CalibrationStage_::Value_::Evaluate)
                throw;
            outcome->reset(new StorableCalibrationOutcome_("Failed", failure.report_));
        }
    }
    namespace {
        const CurveScriptEvaluationResult_& Evaluation(const Handle_<StorableCalibrationOutcome_>& outcome) {
            REQUIRE(outcome && outcome->curveScript_, "CurveScriptEvaluation: result unavailable");
            return *outcome->curveScript_;
        }
    } // namespace
    void CurveScriptEvaluation_Get_Value(const Handle_<StorableCalibrationOutcome_>& outcome, const String_& measureId, double* value) {
        *value = CurveScriptEvaluationValue(Evaluation(outcome), measureId);
    }
    void CurveScriptEvaluation_Get_NativePartials(const Handle_<StorableCalibrationOutcome_>& outcome, Matrix_<>* value) {
        *value = CurveScriptEvaluationPartials(Evaluation(outcome));
    }
    void CurveScriptEvaluation_Get_Table(const Handle_<StorableCalibrationOutcome_>& outcome, const String_& table, Matrix_<Cell_>* value) {
        *value = CurveScriptEvaluationTable(Evaluation(outcome), table);
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrationOvernightCurves_New_public.inc>
#include <dal-excel/auto/MG_CalibrationStrings_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisCalendar_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisCoupons_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisFixedLeg_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisObservations_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisPreparations_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisScriptInstrument_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOisScriptSpec_New_public.inc>
#include <dal-excel/auto/MG_CompoundedOis_Prepare_public.inc>
#include <dal-excel/auto/MG_CurveScriptEvaluationRequest_New_public.inc>
#include <dal-excel/auto/MG_CurveScriptEvaluation_Get_NativePartials_public.inc>
#include <dal-excel/auto/MG_CurveScriptEvaluation_Get_Table_public.inc>
#include <dal-excel/auto/MG_CurveScriptEvaluation_Get_Value_public.inc>
#include <dal-excel/auto/MG_CurveScriptInstrument_Evaluate_public.inc>
#include <dal-excel/auto/MG_CurveScriptOvernightRoutes_New_public.inc>
#endif
} // namespace Dal
