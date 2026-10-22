#include "bindings.h"

#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include <dal-public/src/gsr.hpp>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

namespace Dal::Python::Calibration {
    struct GsrSettingInteger_ {
        int value_;
    };
    template <> struct Input_<GsrSettingInteger_> {
        static GsrSettingInteger_ Read(const py::handle& value, const std::string& field) { return {Input_<int>::Read(value, "/settings" + field)}; }
    };
    struct GsrSettingNumber_ {
        double value_;
    };
    template <> struct Input_<GsrSettingNumber_> {
        static GsrSettingNumber_ Read(const py::handle& value, const std::string& field) {
            return {Number(value, "/settings" + field, "InvalidCalibrationPlan", false)};
        }
    };
} // namespace Dal::Python::Calibration

namespace {
    thread_local bool testBarrierArmed = false;
    GilTestBarrier_ testBarrier;
    constexpr const char* GIL_TIMEOUT = "GSRCalibration: Python GIL test barrier timed out";

    void GilHooks(py::module_& m) {
        m.def("_GsrCalibrationGilBarrier_ArmForTesting", [] { testBarrier.Arm(&testBarrierArmed); });
        m.def("_GsrCalibrationGilBarrier_AwaitEntryForTesting", [] {
            py::gil_scoped_release release;
            return testBarrier.AwaitEntry();
        });
        m.def("_GsrCalibrationGilBarrier_ReleaseForTesting", [] { return testBarrier.Release(); });
        m.def("_GsrCalibrationGilBarrier_BlockWithGilForTesting", [] { testBarrier.Wait(&testBarrierArmed, GIL_TIMEOUT); });
    }
    void Settings(py::module_& m) {
        auto cls = Struct<GSRInstrumentCalibrationSettings_>(m, "GSRInstrumentCalibrationSettings_", [](Fields_& f) {
            GSRInstrumentCalibrationSettings_ s;
            s.valuation_ = f.Required<Script::ScriptValuationSettings_>("valuation");
            s.solver_ = f.Default<GSRCalibrationSettings_>("solver", s.solver_);
            s.evaluation_ = f.Default<CalibrationEvaluationOptions_>("evaluation", s.evaluation_);
            s.fitSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("fit_sampling");
            s.validationSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("validation_sampling");
            s.heldOutSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("held_out_sampling");
            s.heldOutObservations_ = f.Default<Vector_<Handle_<CalibrationObservation_>>>("held_out_observations", s.heldOutObservations_);
            s.maxPathsPerView_ = f.Default<GsrSettingInteger_>("max_paths_per_view", {s.maxPathsPerView_}).value_;
            s.maxBasketEvaluations_ = f.Default<GsrSettingInteger_>("max_basket_evaluations", {s.maxBasketEvaluations_}).value_;
            s.validationSigma_ = f.Default<GsrSettingNumber_>("validation_sigma", {s.validationSigma_}).value_;
            s.epsilonValue_ = f.Default<GsrSettingNumber_>("epsilon_value", {s.epsilonValue_}).value_;
            s.epsilonGradient_ = f.Default<GsrSettingNumber_>("epsilon_gradient", {s.epsilonGradient_}).value_;
            s.compiled_ = f.Default<bool>("compiled", s.compiled_);
            f.Finish();
            ValidateGSRInstrumentCalibrationSettings(s);
            return s;
        });
        Property(cls, "valuation", &GSRInstrumentCalibrationSettings_::valuation_);
        Property(cls, "solver", &GSRInstrumentCalibrationSettings_::solver_);
        Property(cls, "evaluation", &GSRInstrumentCalibrationSettings_::evaluation_);
        Property(cls, "fit_sampling", &GSRInstrumentCalibrationSettings_::fitSampling_);
        Property(cls, "validation_sampling", &GSRInstrumentCalibrationSettings_::validationSampling_);
        Property(cls, "held_out_sampling", &GSRInstrumentCalibrationSettings_::heldOutSampling_);
        Property(cls, "held_out_observations", &GSRInstrumentCalibrationSettings_::heldOutObservations_);
        Property(cls, "max_paths_per_view", &GSRInstrumentCalibrationSettings_::maxPathsPerView_);
        Property(cls, "max_basket_evaluations", &GSRInstrumentCalibrationSettings_::maxBasketEvaluations_);
        Property(cls, "validation_sigma", &GSRInstrumentCalibrationSettings_::validationSigma_);
        Property(cls, "epsilon_value", &GSRInstrumentCalibrationSettings_::epsilonValue_);
        Property(cls, "epsilon_gradient", &GSRInstrumentCalibrationSettings_::epsilonGradient_);
        Property(cls, "compiled", &GSRInstrumentCalibrationSettings_::compiled_);
    }
    py::list Estimates(const Vector_<CalibrationMcResult_>& values) {
        py::list result;
        for (const auto& value : values) {
            py::dict row;
            row["mean"] = value.mean_;
            row["standard_error"] = value.standardError_;
            row["plan_identity"] = Output(value.planIdentity_);
            row["mapping_identity"] = Output(value.mappingIdentity_);
            result.append(row);
        }
        return result;
    }
    void Result(py::module_& m) {
        py::class_<GSRInstrumentCalibrationResult_, GSRCalibrationResult_>(m, "GSRInstrumentCalibrationResult_")
            .def_property_readonly("report", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.report_); })
            .def_property_readonly("backend", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.backend_); })
            .def_property_readonly("derivative_method", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.derivativeMethod_); })
            .def_property_readonly("parameter_labels", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.parameterLabels_); })
            .def_property_readonly("scaled_residuals", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.scaledResiduals_); })
            .def_property_readonly("quote_jacobian",
                                   [](const GSRInstrumentCalibrationResult_& r) { return CalibrationReportQuoteJacobian(r.report_); })
            .def_property_readonly("observations", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.axes_.observations_); })
            .def_property_readonly("quotes", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.axes_.quotes_); })
            .def_property_readonly("quote_map", [](const GSRInstrumentCalibrationResult_& r) { return r.axes_.quoteMap_; })
            .def_property_readonly("held_out_observations",
                                   [](const GSRInstrumentCalibrationResult_& r) { return Output(r.heldOutAxes_.observations_); })
            .def_property_readonly("held_out_quotes", [](const GSRInstrumentCalibrationResult_& r) { return Output(r.heldOutAxes_.quotes_); })
            .def_property_readonly("held_out_quote_map", [](const GSRInstrumentCalibrationResult_& r) { return r.heldOutAxes_.quoteMap_; })
            .def_property_readonly("fit", [](const GSRInstrumentCalibrationResult_& r) { return Estimates(r.fit_); })
            .def_property_readonly("validation", [](const GSRInstrumentCalibrationResult_& r) { return Estimates(r.validation_); })
            .def_property_readonly("held_out", [](const GSRInstrumentCalibrationResult_& r) { return Estimates(r.heldOut_); })
            .def_property_readonly("held_out_requested", [](const GSRInstrumentCalibrationResult_& r) { return r.heldOutRequested_; })
            .def_property_readonly("held_out_passed", [](const GSRInstrumentCalibrationResult_& r) { return r.heldOutPassed_; })
            .def_property_readonly("quote_risk_available", [](const GSRInstrumentCalibrationResult_& r) { return r.quoteRiskAvailable_; })
            .def_property_readonly("native_jacobian_available", [](const GSRInstrumentCalibrationResult_& r) { return r.nativeJacobianAvailable_; });
    }
} // namespace

void init_bindings_gsrinstrument(py::module_& m) {
    Settings(m);
    Result(m);
    GilHooks(m);
    m.def("GSRCalibrationParameter_New", [](py::kwargs values) {
        Fields_ f(std::move(values));
        const GSRCalibrationParameter_ p{f.Required<int>("factor"), f.Required<int>("knot"), f.Required<double>("lower"),
                                         f.Required<double>("upper")};
        f.Finish();
        if (p.factor_ < 0 || p.knot_ < 0 || p.lower_ < 0. || p.upper_ <= p.lower_)
            CalibrationInputFailure("InvalidCalibrationPlan", "/parameters", "nonnegative coordinates and ordered nonnegative bounds");
        return p;
    });
    m.attr("GSRInstrumentCalibrationSettings_New") = m.attr("GSRInstrumentCalibrationSettings_");
    m.def(
        "Calibrate_GSRInstruments",
        [](const py::object& initial, const py::object& observations, const py::object& parameters, const py::object& settings) {
            const auto model = Input_<Handle_<ModelData_>>::Read(initial, "/initial");
            const auto rows = Input_<Vector_<Handle_<CalibrationObservation_>>>::Read(observations, "/observations");
            const auto coordinates = Input_<Vector_<GSRCalibrationParameter_>>::Read(parameters, "/parameters");
            const auto configuration = Input_<GSRInstrumentCalibrationSettings_>::Read(settings, "/settings");
            py::gil_scoped_release release;
            testBarrier.Wait(&testBarrierArmed, GIL_TIMEOUT);
            return CalibrateGSRInstruments(model, rows, coordinates, configuration);
        },
        py::arg("initial"), py::arg("observations"), py::arg("parameters"), py::kw_only(), py::arg("settings"));
}
