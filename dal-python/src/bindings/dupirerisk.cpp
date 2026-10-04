//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <cmath>
#include <pybind11/stl.h>

#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/models.hpp>

#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    double NumericInput(const py::handle& value, const std::string& field) {
        const auto context = InputContext(value, field, "finite int or float, excluding bool and enums", "InvalidDupireCalibration");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            THROW(String_(context));
        }
        REQUIRE(std::isfinite(result), String_(context));
        return result;
    }

    Vector_<> AxisInput(const py::handle& value, const char* field) {
        const std::string context = std::string("DupireRiskInputs_; ") + field;
        if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value) && !py::isinstance<std::vector<double>>(value))
            throw py::type_error(InputContext(value, context, "list, tuple or DoubleVector", "InvalidDupireQuote"));
        Vector_<> result;
        for (const auto item : py::reinterpret_borrow<py::iterable>(value))
            result.push_back(NumericInput(item, context));
        return result;
    }

    template <class T_> T_ RequiredInput(const py::handle& value, const char* field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, "InvalidDupirePullback"));
        return py::cast<T_>(value);
    }

    std::vector<double> CopyAxis(const Vector_<>& values) { return {values.begin(), values.end()}; }

    class PythonIVS_ : public AAD::IVS_ {
    public:
        using IVS_::IVS_;
        [[nodiscard]] double ImpliedVol(double strike, double maturity) const override {
            py::gil_scoped_acquire acquire;
            const auto implementation = py::get_override(static_cast<const AAD::IVS_*>(this), "implied_vol");
            if (!implementation)
                throw py::type_error("InvalidDupireCalibration: IVS_; implied_vol override is required");
            return NumericInput(implementation(strike, maturity), "IVS_; implied_vol");
        }
    };

    std::unique_ptr<PythonIVS_> NewIVS(const py::object& spot, const py::object& rate, const py::object& dividend) {
        const double actualSpot = NumericInput(spot, "IVS_; spot");
        REQUIRE(actualSpot > 0.0, "InvalidDupireCalibration: IVS_; spot must be positive");
        return std::make_unique<PythonIVS_>(actualSpot, NumericInput(rate, "IVS_; rate"), NumericInput(dividend, "IVS_; dividend_yield"));
    }

    AAD::MertonIVS_
    NewMerton(const py::object& spot, const py::object& vol, const py::object& intensity, const py::object& jump, const py::object& deviation) {
        const double actualSpot = NumericInput(spot, "MertonIVS_; spot");
        const double actualVol = NumericInput(vol, "MertonIVS_; vol");
        const double actualIntensity = NumericInput(intensity, "MertonIVS_; intensity");
        const double actualJump = NumericInput(jump, "MertonIVS_; average_jump");
        const double actualDeviation = NumericInput(deviation, "MertonIVS_; jump_std");
        REQUIRE(actualSpot > 0.0, "InvalidDupireCalibration: MertonIVS_; spot must be positive");
        REQUIRE(actualVol >= 0.0, "InvalidDupireCalibration: MertonIVS_; vol must be nonnegative");
        REQUIRE(actualIntensity >= 0.0, "InvalidDupireCalibration: MertonIVS_; intensity must be nonnegative");
        REQUIRE(actualDeviation >= 0.0, "InvalidDupireCalibration: MertonIVS_; jump_std must be nonnegative");
        return {actualSpot, actualVol, actualIntensity, actualJump, actualDeviation};
    }

    DupireCalibrationSnapshot_ Freeze(const py::object& base, const py::object& inputs, const py::object& name) {
        const auto config = RequiredInput<DupireRiskInputs_>(inputs, "DupireCalibration_New; inputs", "DupireRiskInputs_");
        const auto label = SettingStringInput(name, "DupireCalibration_New; name", "InvalidDupireCalibration");
        if (py::isinstance<AAD::IVS_>(base))
            return CalibrateDupireWithRisk(py::cast<const AAD::IVS_&>(base), config, label);
        if (py::isinstance<ModelData_>(base)) {
            const auto model = py::cast<std::shared_ptr<ModelData_>>(base);
            const auto* flat = dynamic_cast<const BSModelData_*>(model.get());
            REQUIRE(flat, "InvalidDupireCalibration: DupireCalibration_New; base must be BSModelData_ or IVS_");
            return CalibrateDupireWithRisk(*flat, config, label);
        }
        throw py::type_error(InputContext(base, "DupireCalibration_New; base", "BSModelData_ or IVS_", "InvalidDupireCalibration"));
    }

    std::optional<DupireDirectQuoteAdjoints_> DirectInput(const py::object& direct) {
        return direct.is_none() ? std::nullopt
                                : std::optional<DupireDirectQuoteAdjoints_>(RequiredInput<DupireDirectQuoteAdjoints_>(
                                      direct, "DupireQuoteRisk; direct", "DupireDirectQuoteAdjoints_ or None"));
    }

    template <class T_> void BindSeeds(py::module_& m, const char* name) {
        WithCopies(py::class_<T_>(m, name))
            .def(py::init([](const py::object& calibration, const py::object& adjoints) {
                     return T_{RequiredInput<DupireCalibrationSnapshot_>(calibration, "DupireAdjoints; calibration", "DupireCalibrationSnapshot_"),
                               RequiredInput<Matrix_<>>(adjoints, "DupireAdjoints; adjoints", "DoubleMatrix_")};
                 }),
                 py::arg("calibration"), py::arg("adjoints"))
            .def_property_readonly("calibration", [](const T_& value) { return value.calibration_; })
            .def_property_readonly("adjoints", [](const T_& value) { return Matrix_<>(value.adjoints_); });
    }
} // namespace

void init_bindings_dupirerisk(py::module_& m) {
    py::class_<AAD::IVS_, PythonIVS_>(m, "IVS_")
        .def(py::init(&NewIVS), py::kw_only(), py::arg("spot"), py::arg("rate") = 0.0, py::arg("dividend_yield") = 0.0)
        .def_property_readonly("spot", &AAD::IVS_::Spot)
        .def_property_readonly("rate", &AAD::IVS_::Rate)
        .def_property_readonly("dividend_yield", &AAD::IVS_::DividendYield)
        .def("implied_vol", &AAD::IVS_::ImpliedVol, py::arg("strike"), py::arg("maturity"));
    py::class_<AAD::MertonIVS_, AAD::IVS_>(m, "MertonIVS_")
        .def(py::init(&NewMerton), py::kw_only(), py::arg("spot"), py::arg("vol"), py::arg("intensity"), py::arg("average_jump"),
             py::arg("jump_std"));

    WithCopies(py::class_<DupireRiskInputs_>(m, "DupireRiskInputs_"))
        .def(py::init([](const py::object& strikes, const py::object& maturities, const py::object& spreads, const py::object& spots,
                         const py::object& spotSpacing, const py::object& times, const py::object& timeSpacing) {
                 return DupireRiskInputs_{AxisInput(strikes, "quote_strikes"),
                                          AxisInput(maturities, "quote_maturities"),
                                          RequiredInput<Matrix_<>>(spreads, "DupireRiskInputs_; quote_spreads", "DoubleMatrix_"),
                                          AxisInput(spots, "inclusion_spots"),
                                          NumericInput(spotSpacing, "DupireRiskInputs_; max_spot_spacing"),
                                          AxisInput(times, "inclusion_times"),
                                          NumericInput(timeSpacing, "DupireRiskInputs_; max_time_spacing")};
             }),
             py::kw_only(), py::arg("quote_strikes"), py::arg("quote_maturities"), py::arg("quote_spreads"), py::arg("inclusion_spots"),
             py::arg("max_spot_spacing"), py::arg("inclusion_times"), py::arg("max_time_spacing"))
        .def_property_readonly("quote_strikes", [](const DupireRiskInputs_& value) { return CopyAxis(value.quoteStrikes_); })
        .def_property_readonly("quote_maturities", [](const DupireRiskInputs_& value) { return CopyAxis(value.quoteMaturities_); })
        .def_property_readonly("quote_spreads", [](const DupireRiskInputs_& value) { return Matrix_<>(value.quoteSpreads_); })
        .def_property_readonly("inclusion_spots", [](const DupireRiskInputs_& value) { return CopyAxis(value.inclusionSpots_); })
        .def_property_readonly("max_spot_spacing", [](const DupireRiskInputs_& value) { return value.maxSpotSpacing_; })
        .def_property_readonly("inclusion_times", [](const DupireRiskInputs_& value) { return CopyAxis(value.inclusionTimes_); })
        .def_property_readonly("max_time_spacing", [](const DupireRiskInputs_& value) { return value.maxTimeSpacing_; });

    WithCopies(py::class_<DupireCalibrationSnapshot_>(m, "DupireCalibrationSnapshot_"))
        .def_property_readonly("inputs", [](const DupireCalibrationSnapshot_& value) { return value.Inputs(); })
        .def_property_readonly("surface",
                               [](const DupireCalibrationSnapshot_& value) {
                                   const auto& surface = *value.Surface();
                                   return std::make_shared<LocalVolSurfaceData_>(surface.Name(), surface.spots_, surface.times_, surface.vols_);
                               })
        .def_property_readonly("spots", [](const DupireCalibrationSnapshot_& value) { return CopyAxis(value.Surface()->spots_); })
        .def_property_readonly("times", [](const DupireCalibrationSnapshot_& value) { return CopyAxis(value.Surface()->times_); })
        .def_property_readonly("vols", [](const DupireCalibrationSnapshot_& value) { return Matrix_<>(value.Surface()->vols_); })
        .def_property_readonly("spot", &DupireCalibrationSnapshot_::Spot)
        .def_property_readonly("rate", &DupireCalibrationSnapshot_::Rate)
        .def_property_readonly("dividend_yield", &DupireCalibrationSnapshot_::DividendYield)
        .def_property_readonly("algorithm", [](const DupireCalibrationSnapshot_& value) { return Text(value.Algorithm()); })
        .def("matches", &DupireCalibrationSnapshot_::Matches, py::arg("other"));
    m.def("DupireCalibration_New", &Freeze, py::arg("base"), py::arg("inputs"), py::kw_only(), py::arg("name") = "");
    BindSeeds<DupireParameterAdjoints_>(m, "DupireParameterAdjoints_");
    BindSeeds<DupireDirectQuoteAdjoints_>(m, "DupireDirectQuoteAdjoints_");

    WithCopies(py::class_<DupireQuoteRisk_>(m, "DupireQuoteRisk_"))
        .def_property_readonly("calibration", [](const DupireQuoteRisk_& value) { return value.Calibration(); })
        .def_property_readonly("calibration_adjoints", [](const DupireQuoteRisk_& value) { return Matrix_<>(value.CalibrationAdjoints()); })
        .def_property_readonly("direct_adjoints", [](const DupireQuoteRisk_& value) { return Matrix_<>(value.DirectAdjoints()); })
        .def_property_readonly("total_adjoints", [](const DupireQuoteRisk_& value) { return Matrix_<>(value.TotalAdjoints()); })
        .def_property_readonly("method", [](const DupireQuoteRisk_& value) { return Text(value.Method()); })
        .def_property_readonly("unit", [](const DupireQuoteRisk_& value) { return Text(value.Unit()); })
        .def_property_readonly("boundary", [](const DupireQuoteRisk_& value) { return Text(value.Boundary()); });
    WithCopies(py::class_<DupireScriptQuoteRisk_>(m, "DupireScriptQuoteRisk_"))
        .def_property_readonly("valuation", [](const DupireScriptQuoteRisk_& value) { return value.Valuation(); })
        .def_property_readonly("quote_risk", [](const DupireScriptQuoteRisk_& value) { return value.QuoteRisk(); })
        .def_property_readonly("component", [](const DupireScriptQuoteRisk_& value) { return Text(value.Component()); })
        .def_property_readonly("method", [](const DupireScriptQuoteRisk_& value) { return Text(value.Method()); });

    m.def(
        "DupireQuoteRisk_New",
        [](const py::object& calibration, const py::object& parameters, const py::object& direct) {
            const auto snapshot =
                RequiredInput<DupireCalibrationSnapshot_>(calibration, "DupireQuoteRisk_New; calibration", "DupireCalibrationSnapshot_");
            const auto seeds =
                RequiredInput<DupireParameterAdjoints_>(parameters, "DupireQuoteRisk_New; parameter_adjoints", "DupireParameterAdjoints_");
            const auto contribution = DirectInput(direct);
            py::gil_scoped_release release;
            return PullbackDupireCalibration(snapshot, seeds, contribution);
        },
        py::arg("calibration"), py::arg("parameter_adjoints"), py::kw_only(), py::arg("direct") = py::none());
    m.def(
        "DupireParameterAdjoints_FromRisk",
        [](const py::object& valuation, const py::object& calibration, const py::object& component) {
            const auto source = RequiredInput<Script::RiskResult_>(valuation, "DupireParameterAdjoints_FromRisk; valuation", "RiskResult_");
            const auto snapshot =
                RequiredInput<DupireCalibrationSnapshot_>(calibration, "DupireParameterAdjoints_FromRisk; calibration", "DupireCalibrationSnapshot_");
            const auto name = SettingStringInput(component, "DupireParameterAdjoints_FromRisk; component", "InvalidDupirePullback");
            py::gil_scoped_release release;
            return ExtractDupireParameterAdjoints(source, snapshot, name);
        },
        py::arg("valuation"), py::arg("calibration"), py::arg("component"));
    m.def(
        "DupireScriptQuoteRisk_New",
        [](const py::object& valuation, const py::object& calibration, const py::object& component, const py::object& direct) {
            const auto source = RequiredInput<Script::RiskResult_>(valuation, "DupireScriptQuoteRisk_New; valuation", "RiskResult_");
            const auto snapshot =
                RequiredInput<DupireCalibrationSnapshot_>(calibration, "DupireScriptQuoteRisk_New; calibration", "DupireCalibrationSnapshot_");
            const auto name = SettingStringInput(component, "DupireScriptQuoteRisk_New; component", "InvalidDupirePullback");
            const auto contribution = DirectInput(direct);
            py::gil_scoped_release release;
            return PullbackDupireScriptRisk(source, snapshot, name, contribution);
        },
        py::arg("valuation"), py::arg("calibration"), py::arg("component"), py::kw_only(), py::arg("direct") = py::none());
}
