#include "bindings.h"
#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include <dal-public/src/calibrationfieldsinternal.hpp>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

namespace {
    GilTestBarrier_ barrier;
    thread_local bool barrierArmed = false;
} // namespace

void init_compounded_ois(py::module_& m) {
    {
        auto cls = Struct<CompoundedOisFixedRow_>(m, "CompoundedOisFixedRow_", [](Fields_& f) {
            return CompoundedOisFixedRow_{f.Required<Date_>("start"), f.Required<Date_>("end"), f.Required<Date_>("payment"),
                                          f.Required<double>("accrual")};
        });
        Property(cls, "start", &CompoundedOisFixedRow_::start_);
        Property(cls, "end", &CompoundedOisFixedRow_::end_);
        Property(cls, "payment", &CompoundedOisFixedRow_::payment_);
        Property(cls, "accrual", &CompoundedOisFixedRow_::accrual_);
    }
    {
        auto cls = Struct<CompoundedOisCouponRow_>(m, "CompoundedOisCouponRow_", [](Fields_& f) {
            return CompoundedOisCouponRow_{f.Required<String_>("coupon_id"), f.Required<Date_>("start"), f.Required<Date_>("end"),
                                           f.Required<Date_>("payment")};
        });
        Property(cls, "coupon_id", &CompoundedOisCouponRow_::couponId_);
        Property(cls, "start", &CompoundedOisCouponRow_::start_);
        Property(cls, "end", &CompoundedOisCouponRow_::end_);
        Property(cls, "payment", &CompoundedOisCouponRow_::payment_);
    }
    {
        auto cls = Struct<CompoundedOisObservationRow_>(m, "CompoundedOisObservationRow_", [](Fields_& f) {
            return CompoundedOisObservationRow_{f.Required<Date_>("start"),    f.Required<Date_>("end"),         f.Required<DateTime_>("fixing_time"),
                                                f.Required<double>("accrual"), f.Required<String_>("coupon_id"), f.Required<Date_>("rate_start"),
                                                f.Required<Date_>("rate_end")};
        });
        Property(cls, "start", &CompoundedOisObservationRow_::start_);
        Property(cls, "end", &CompoundedOisObservationRow_::end_);
        Property(cls, "fixing_time", &CompoundedOisObservationRow_::fixingTime_);
        Property(cls, "accrual", &CompoundedOisObservationRow_::accrual_);
        Property(cls, "coupon_id", &CompoundedOisObservationRow_::couponId_);
        Property(cls, "rate_start", &CompoundedOisObservationRow_::rateStart_);
        Property(cls, "rate_end", &CompoundedOisObservationRow_::rateEnd_);
    }
    {
        auto cls = Struct<HolidayCenterData_>(m, "HolidayCenterData_", [](Fields_& f) {
            return HolidayCenterData_(f.Required<String_>("center"), f.Required<Vector_<Date_>>("holidays"),
                                      f.Default<Vector_<Date_>>("work_weekends", {}));
        });
        Property(cls, "center", &HolidayCenterData_::center_);
        Property(cls, "holidays", &HolidayCenterData_::holidays_);
        Property(cls, "work_weekends", &HolidayCenterData_::workWeekends_);
    }
    {
        auto cls =
            Struct<CompoundedOisScriptSpec_>(m, "CompoundedOisScriptSpec_", [](Fields_& f) { return BindingInternal::ReadCompoundedOisSpec(f); });
        Property(cls, "instrument_id", &CompoundedOisScriptSpec_::instrumentId_);
        Property(cls, "direction", &CompoundedOisScriptSpec_::direction_);
        Property(cls, "discount_route", &CompoundedOisScriptSpec_::discountRoute_);
        Property(cls, "projection_route", &CompoundedOisScriptSpec_::projectionRoute_);
        Property(cls, "fixing_index", &CompoundedOisScriptSpec_::fixingIndex_);
        Property(cls, "notional", &CompoundedOisScriptSpec_::notional_);
        Property(cls, "coupon", &CompoundedOisScriptSpec_::coupon_);
        Property(cls, "currency", &CompoundedOisScriptSpec_::currency_);
        Property(cls, "collateral_currency", &CompoundedOisScriptSpec_::collateralCurrency_);
        Property(cls, "collateral_type", &CompoundedOisScriptSpec_::collateralType_);
        Property(cls, "observation_basis", &CompoundedOisScriptSpec_::observationBasis_);
        Property(cls, "fixed_basis", &CompoundedOisScriptSpec_::fixedBasis_);
        Property(cls, "calendar", &CompoundedOisScriptSpec_::calendar_);
        Property(cls, "fixed_leg", &CompoundedOisScriptSpec_::fixedLeg_);
        Property(cls, "coupons", &CompoundedOisScriptSpec_::coupons_);
        Property(cls, "observations", &CompoundedOisScriptSpec_::observations_);
        Property(cls, "min_abs_annuity", &CompoundedOisScriptSpec_::minAbsAnnuity_);
        Property(cls, "rule_id", &CompoundedOisScriptSpec_::ruleId_);
    }
    {
        auto cls = Struct<CurveScriptEvaluationRequest_>(m, "CurveScriptEvaluationRequest_",
                                                         [](Fields_& f) { return BindingInternal::ReadCurveScriptEvaluationRequest(f); });
        Property(cls, "geometry", &CurveScriptEvaluationRequest_::geometry_);
        Property(cls, "market", &CurveScriptEvaluationRequest_::market_);
        Property(cls, "valuation", &CurveScriptEvaluationRequest_::valuation_);
        Property(cls, "parameters", &CurveScriptEvaluationRequest_::parameters_);
        Property(cls, "compounded_ois", &CurveScriptEvaluationRequest_::compoundedOis_);
        Property(cls, "measure_ids", &CurveScriptEvaluationRequest_::measureIds_);
        Property(cls, "evaluation", &CurveScriptEvaluationRequest_::evaluation_);
        Property(cls, "derivative_method", &CurveScriptEvaluationRequest_::derivativeMethod_);
        Property(cls, "difference_step", &CurveScriptEvaluationRequest_::differenceStep_);
    }
    py::class_<PreparedCompoundedOis_, std::shared_ptr<PreparedCompoundedOis_>>(m, "PreparedCompoundedOis_")
        .def_property_readonly("spec", [](const PreparedCompoundedOis_& v) { return v.Spec(); })
        .def_property_readonly("valuation", [](const PreparedCompoundedOis_& v) { return v.Valuation(); })
        .def_property_readonly("contract_identity", [](const PreparedCompoundedOis_& v) { return Text(v.ContractIdentity()); })
        .def_property_readonly("preparation_identity", [](const PreparedCompoundedOis_& v) { return Text(v.PreparationIdentity()); })
        .def_property_readonly("calendar_identity", [](const PreparedCompoundedOis_& v) { return Text(v.CalendarIdentity()); });
    {
        auto cls = py::class_<CurveScriptCashflowResult_>(m, "CurveScriptCashflowResult_");
        Property(cls, "table", &CurveScriptCashflowResult_::table_);
        Property(cls, "coupon_id", &CurveScriptCashflowResult_::couponId_);
        Property(cls, "row", &CurveScriptCashflowResult_::row_);
        Property(cls, "source_row", &CurveScriptCashflowResult_::sourceRow_);
        Property(cls, "statement", &CurveScriptCashflowResult_::statement_);
        Property(cls, "payment", &CurveScriptCashflowResult_::payment_);
        Property(cls, "included", &CurveScriptCashflowResult_::included_);
        Property(cls, "settled", &CurveScriptCashflowResult_::settled_);
        Property(cls, "amount", &CurveScriptCashflowResult_::amount_);
        Property(cls, "discount", &CurveScriptCashflowResult_::discount_);
        Property(cls, "pv", &CurveScriptCashflowResult_::pv_);
    }
    {
        auto cls = py::class_<CurveScriptObservationResult_>(m, "CurveScriptObservationResult_");
        Property(cls, "row", &CurveScriptObservationResult_::row_);
        Property(cls, "source_row", &CurveScriptObservationResult_::sourceRow_);
        Property(cls, "statement", &CurveScriptObservationResult_::statement_);
        Property(cls, "coupon_id", &CurveScriptObservationResult_::couponId_);
        Property(cls, "fixing_index", &CurveScriptObservationResult_::fixingIndex_);
        Property(cls, "projection_route", &CurveScriptObservationResult_::projectionRoute_);
        Property(cls, "source", &CurveScriptObservationResult_::source_);
        Property(cls, "fixing_time", &CurveScriptObservationResult_::fixingTime_);
        Property(cls, "start", &CurveScriptObservationResult_::start_);
        Property(cls, "end", &CurveScriptObservationResult_::end_);
        Property(cls, "rate_start", &CurveScriptObservationResult_::rateStart_);
        Property(cls, "rate_end", &CurveScriptObservationResult_::rateEnd_);
        Property(cls, "accrual", &CurveScriptObservationResult_::accrual_);
        Property(cls, "rate_accrual", &CurveScriptObservationResult_::rateAccrual_);
        Property(cls, "rate", &CurveScriptObservationResult_::rate_);
    }
    {
        auto cls = py::class_<CurveScriptEvaluationResult_>(m, "CurveScriptEvaluationResult_");
        Property(cls, "instrument", &CurveScriptEvaluationResult_::instrument_);
        Property(cls, "curve", &CurveScriptEvaluationResult_::curve_);
        Property(cls, "report", &CurveScriptEvaluationResult_::report_);
        Property(cls, "measure_ids", &CurveScriptEvaluationResult_::measureIds_);
        Property(cls, "measure_units", &CurveScriptEvaluationResult_::measureUnits_);
        Property(cls, "parameters", &CurveScriptEvaluationResult_::parameters_);
        Property(cls, "values", &CurveScriptEvaluationResult_::values_);
        Property(cls, "free_parameters", &CurveScriptEvaluationResult_::freeParameters_);
        Property(cls, "native_partials", &CurveScriptEvaluationResult_::nativePartials_);
        Property(cls, "cashflows", &CurveScriptEvaluationResult_::cashflows_);
        Property(cls, "observations", &CurveScriptEvaluationResult_::observations_);
        Property(cls, "derivative_method", &CurveScriptEvaluationResult_::derivativeMethod_);
        Property(cls, "context_identity", &CurveScriptEvaluationResult_::contextIdentity_);
        Property(cls, "candidate_identity", &CurveScriptEvaluationResult_::candidateIdentity_);
    }
    m.def(
        "CompoundedOis_Prepare",
        [](const py::object& spec, const py::object& valuation) {
            const auto s = Input_<CompoundedOisScriptSpec_>::Read(spec, "/spec");
            const auto v = Input_<Script::ScriptValuationSettings_>::Read(valuation, "/valuation");
            py::gil_scoped_release release;
            return std::const_pointer_cast<PreparedCompoundedOis_>(PrepareCompoundedOis(s, v));
        },
        py::arg("spec"), py::arg("valuation"));
    m.def(
        "CompoundedOisScriptInstrument_New",
        [](const py::object& tables) {
            const auto input = Input_<Handle_<PreparedCompoundedOis_>>::Read(tables, "/tables");
            py::gil_scoped_release release;
            return std::const_pointer_cast<CalibrationInstrument_>(NewCompoundedOisScriptInstrument(input));
        },
        py::arg("tables"));
    m.def(
        "CurveScriptInstrument_Evaluate",
        [](const py::object& instrument, const py::object& request) {
            const auto input = Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "/instrument");
            const auto owned = Input_<CurveScriptEvaluationRequest_>::Read(request, "/request");
            py::gil_scoped_release release;
            barrier.Wait(&barrierArmed, "CurveScript: GIL test barrier timed out");
            return EvaluateCurveScriptInstrument(input, owned);
        },
        py::arg("instrument"), py::arg("request"));
    m.def(
        "CurveScriptEvaluation_Get_Value",
        [](const CurveScriptEvaluationResult_& result, const py::object& measure) {
            return CurveScriptEvaluationValue(result, Input_<String_>::Read(measure, "/measure_id"));
        },
        py::arg("result"), py::arg("measure_id"));
    m.def("CurveScriptEvaluation_Get_NativePartials", &CurveScriptEvaluationPartials, py::arg("result"));
    m.def("_CurveScriptGilBarrier_ArmForTesting", [] { barrier.Arm(&barrierArmed); });
    m.def("_CurveScriptGilBarrier_AwaitEntryForTesting", [] {
        py::gil_scoped_release release;
        return barrier.AwaitEntry();
    });
    m.def("_CurveScriptGilBarrier_ReleaseForTesting", [] { return barrier.Release(); });
}
