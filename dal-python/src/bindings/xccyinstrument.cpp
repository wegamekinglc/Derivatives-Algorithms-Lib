#include "bindings.h"

#include "calibrationinput.hpp"
#include <dal-public/src/xccycalibration.hpp>
#include <dal-public/src/xccymontecarlo.hpp>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

namespace {
    void Context(py::module_& m) {
        auto cls = Struct<XccyScriptContext_>(m, "XccyScriptContext_", [](Fields_& f) {
            XccyScriptContext_ v;
            v.valuationTime_ = f.Required<DateTime_>("valuation_time");
            v.pair_ = f.Required<CurrencyPair_>("pair");
            v.collateralCurrency_ = f.Required<Ccy_>("collateral_currency");
            v.discountRoute_ = f.Required<String_>("discount_route");
            v.fixings_ = f.Maybe<Handle_<MarketFixingSnapshot_>>("fixings").value_or(Handle_<MarketFixingSnapshot_>());
            v.todayFixingOverride_ = f.Maybe<TodayFixingPolicy_>("today_fixing_override");
            return v;
        });
        Property(cls, "valuation_time", &XccyScriptContext_::valuationTime_);
        Property(cls, "pair", &XccyScriptContext_::pair_);
        Property(cls, "collateral_currency", &XccyScriptContext_::collateralCurrency_);
        Property(cls, "discount_route", &XccyScriptContext_::discountRoute_);
        Property(cls, "fixings", &XccyScriptContext_::fixings_);
        Property(cls, "today_fixing_override", &XccyScriptContext_::todayFixingOverride_);
    }
    void Types(py::module_& m) {
        auto settings = Struct<XccyScriptImportSettings_>(m, "XccyScriptImportSettings_", [](Fields_& f) {
            XccyScriptImportSettings_ v;
            v.instrumentId_ = f.Required<String_>("instrument_id");
            v.discountRoute_ = f.Required<String_>("discount_route");
            v.receiveNonSpreadPaySpread_ = f.Default<bool>("receive_nonspread_pay_spread", true);
            v.minAbsAnnuity_ = f.Default<double>("min_abs_annuity", 1e-12);
            return v;
        });
        Property(settings, "instrument_id", &XccyScriptImportSettings_::instrumentId_);
        Property(settings, "discount_route", &XccyScriptImportSettings_::discountRoute_);
        Property(settings, "receive_nonspread_pay_spread", &XccyScriptImportSettings_::receiveNonSpreadPaySpread_);
        Property(settings, "min_abs_annuity", &XccyScriptImportSettings_::minAbsAnnuity_);
        auto spec = Struct<XccyInstrumentCalibrationSpec_>(m, "XccyInstrumentCalibrationSpec_", [](Fields_& f) {
            XccyInstrumentCalibrationSpec_ v;
            v.context_ = f.Required<XccyScriptContext_>("context");
            v.domesticCurveBlock_ = f.Required<Handle_<CurveBlock_>>("domestic_curve_block");
            v.foreignCurveBlock_ = f.Required<Handle_<CurveBlock_>>("foreign_curve_block");
            v.fxSpot_ = f.Required<double>("fx_spot");
            v.knotDates_ = f.Required<Vector_<Date_>>("knot_dates");
            v.observations_ = f.Required<Vector_<Handle_<CalibrationObservation_>>>("observations");
            v.solver_ = f.Default<CurveInstrumentSolver_>("solver", v.solver_);
            v.gaussianDynamics_ = f.Maybe<Handle_<XccyGaussianDynamics_>>("gaussian_dynamics").value_or(Handle_<XccyGaussianDynamics_>());
            v.basisParameterization_ = f.Default<CurveParameterization_>("basis_parameterization", v.basisParameterization_);
            return v;
        });
        Property(spec, "context", &XccyInstrumentCalibrationSpec_::context_);
        Property(spec, "domestic_curve_block", &XccyInstrumentCalibrationSpec_::domesticCurveBlock_);
        Property(spec, "foreign_curve_block", &XccyInstrumentCalibrationSpec_::foreignCurveBlock_);
        Property(spec, "fx_spot", &XccyInstrumentCalibrationSpec_::fxSpot_);
        Property(spec, "knot_dates", &XccyInstrumentCalibrationSpec_::knotDates_);
        Property(spec, "observations", &XccyInstrumentCalibrationSpec_::observations_);
        Property(spec, "solver", &XccyInstrumentCalibrationSpec_::solver_);
        Property(spec, "gaussian_dynamics", &XccyInstrumentCalibrationSpec_::gaussianDynamics_);
        Property(spec, "basis_parameterization", &XccyInstrumentCalibrationSpec_::basisParameterization_);
        py::class_<XccyInstrumentCalibrationResult_, CurveInstrumentCalibrationResult_>(m, "XccyInstrumentCalibrationResult_")
            .def_property_readonly("basis_curve", [](const XccyInstrumentCalibrationResult_& r) { return Output(r.curve_); })
            .def_property_readonly("gaussian_model",
                                   [](const XccyInstrumentCalibrationResult_& r) { return Output(XccyInstrumentResultGaussianModel(r)); });
    }
} // namespace

void init_xccy_instruments(py::module_& m) {
    Context(m);
    Types(m);
    m.def(
        "XccyScriptInstrument_New",
        [](const py::object& native, const py::object& settings) {
            const auto trade = Input_<Handle_<CrossCurrencySwap_>>::Read(native, "/native");
            const auto input = Input_<XccyScriptImportSettings_>::Read(settings, "/settings");
            py::gil_scoped_release release;
            return std::const_pointer_cast<CalibrationInstrument_>(NewXccyScriptInstrument(trade, input));
        },
        py::arg("native"), py::arg("settings"));
    m.def(
        "Calibrate_XccyInstruments",
        [](const py::object& spec, const py::object& options) {
            const auto input = Input_<XccyInstrumentCalibrationSpec_>::Read(spec, "/spec");
            const auto settings =
                options.is_none() ? CurveInstrumentCalibrationOptions_() : Input_<CurveInstrumentCalibrationOptions_>::Read(options, "/options");
            py::gil_scoped_release release;
            return CalibrateXccyInstrumentMarket(input, settings);
        },
        py::arg("spec"), py::kw_only(), py::arg("options") = py::none());
    m.def(
        "Evaluate_XccyInstruments",
        [](const py::object& spec, const py::object& parameters, const py::object& options) {
            const auto input = Input_<XccyInstrumentCalibrationSpec_>::Read(spec, "/spec");
            const auto theta = Input_<Vector_<>>::Read(parameters, "/parameters");
            const auto settings =
                options.is_none() ? CurveInstrumentCalibrationOptions_() : Input_<CurveInstrumentCalibrationOptions_>::Read(options, "/options");
            py::gil_scoped_release release;
            return PreparedXccyInstruments_(input, settings).Evaluate(theta);
        },
        py::arg("spec"), py::arg("parameters"), py::kw_only(), py::arg("options") = py::none());
    m.def(
        "CalibrationInstrument_Get_XccyCapabilities",
        [](const py::object& instrument, const py::object& measure, const py::object& context) {
            const auto trade = Input_<Handle_<CalibrationInstrument_>>::Read(instrument, "/instrument");
            const auto selected = Input_<String_>::Read(measure, "/measure_id");
            const auto market = Input_<XccyScriptContext_>::Read(context, "/context");
            py::gil_scoped_release release;
            return std::const_pointer_cast<CalibrationReport_>(XccyInstrumentCapabilities(trade, selected, market));
        },
        py::arg("instrument"), py::arg("measure_id"), py::arg("context"));
    for (const char* type : {"XccyScriptContext", "XccyScriptImportSettings", "XccyInstrumentCalibrationSpec"})
        m.attr((std::string(type) + "_New").c_str()) = m.attr((std::string(type) + "_").c_str());
}
