//
// Created by Codex on 2026/10/5.
//

#include "bindings.h"

#include <cmath>
#include <optional>
#include <type_traits>
#include <variant>

#include <pybind11/stl.h>

#include <dal-public/src/calibrationrisk.hpp>

#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    template <class T_> T_ RequiredInput(const py::handle& value, const std::string& field, const char* type) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, "InvalidCalibrationPullback"));
        return py::cast<T_>(value);
    }

    CalibrationPullback_ BoundaryInput(const py::object& calibration) {
        if (py::isinstance<DupireCalibrationSnapshot_>(calibration))
            return NewCalibrationPullback(py::cast<const DupireCalibrationSnapshot_&>(calibration));
        if (py::isinstance<RateQuoteRiskProvenance_>(calibration))
            return NewCalibrationPullback(py::cast<const RateQuoteRiskProvenance_&>(calibration));
        throw py::type_error(InputContext(calibration, "CalibrationPullback_New; calibration",
                                          "DupireCalibrationSnapshot_ or RateQuoteRiskProvenance_", "InvalidCalibrationPullback"));
    }

    bool IsSequence(const py::handle& value) { return py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value); }

    double NumericSeed(const py::handle& value, const std::string& field, int row, int column) {
        const auto error = [&] {
            return InputContext(value, field + "; row=" + std::to_string(row) + "; column=" + std::to_string(column),
                                "finite int or float, excluding bool and enums", "InvalidCalibrationPullback");
        };
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(error());
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(error());
        }
        REQUIRE(std::isfinite(result), String_(error()));
        return result;
    }

    Matrix_<> SeedMatrixInput(const py::object& value, const std::string& field, int rows, int columns) {
        if (py::isinstance<Matrix_<>>(value))
            return py::cast<Matrix_<>>(value);
        if (!IsSequence(value))
            throw py::type_error(InputContext(value, field, "DoubleMatrix_ or rectangular list/tuple of numeric rows", "InvalidCalibrationPullback"));
        const auto outer = py::reinterpret_borrow<py::sequence>(value);
        REQUIRE(py::len(outer) == static_cast<size_t>(rows), "InvalidCalibrationPullback: seed dimensions disagree; field=" + String_(field));
        Matrix_<> result(rows, columns);
        for (int row = 0; row < rows; ++row) {
            const auto cells = outer[row];
            if (!IsSequence(cells))
                throw py::type_error(
                    InputContext(cells, field + "; row=" + std::to_string(row), "list/tuple of numeric cells", "InvalidCalibrationPullback"));
            const auto inner = py::reinterpret_borrow<py::sequence>(cells);
            REQUIRE(py::len(inner) == static_cast<size_t>(columns),
                    "InvalidCalibrationPullback: seed dimensions disagree; field=" + String_(field) + "; row=" + String::FromInt(row));
            for (int column = 0; column < columns; ++column)
                result(row, column) = NumericSeed(inner[column], field, row, column);
        }
        return result;
    }

    template <class T_> void BindSeeds(py::module_& m, const char* type, const char* factory) {
        WithCopies(py::class_<T_>(m, type))
            .def_property_readonly("calibration", [](const T_& value) { return value.Calibration(); })
            .def_property_readonly("adjoints", [](const T_& value) { return Matrix_<>(value.Adjoints()); });
        m.def(
            factory,
            [factory](const py::object& calibration, const py::object& adjoints) {
                const auto source = RequiredInput<CalibrationPullback_>(calibration, std::string(factory) + "; calibration", "CalibrationPullback_");
                constexpr bool parameters = std::is_same_v<T_, CalibrationParameterAdjoints_>;
                const int rows = parameters ? source.ParameterRows() : source.QuoteRows();
                const int columns = parameters ? source.ParameterCols() : source.QuoteCols();
                return T_(source, SeedMatrixInput(adjoints, std::string(factory) + "; adjoints", rows, columns));
            },
            py::arg("calibration"), py::arg("adjoints"));
    }

    std::optional<CalibrationDirectQuoteAdjoints_> DirectInput(const py::object& direct) {
        return direct.is_none() ? std::nullopt
                                : std::optional<CalibrationDirectQuoteAdjoints_>(RequiredInput<CalibrationDirectQuoteAdjoints_>(
                                      direct, "PullbackCalibration; direct", "CalibrationDirectQuoteAdjoints_ or None"));
    }
} // namespace

void init_bindings_calibrationrisk(py::module_& m) {
    WithCopies(py::class_<CalibrationPullback_>(m, "CalibrationPullback_"))
        .def_property_readonly("source",
                               [](const CalibrationPullback_& value) {
                                   return std::visit([](const auto& source) { return py::cast(source, py::return_value_policy::copy); },
                                                     value.Source());
                               })
        .def_property_readonly("domain", [](const CalibrationPullback_& value) { return Text(value.Domain()); })
        .def_property_readonly("parameter_rows", &CalibrationPullback_::ParameterRows)
        .def_property_readonly("parameter_cols", &CalibrationPullback_::ParameterCols)
        .def_property_readonly("quote_rows", &CalibrationPullback_::QuoteRows)
        .def_property_readonly("quote_cols", &CalibrationPullback_::QuoteCols)
        .def_property_readonly("method", [](const CalibrationPullback_& value) { return Text(value.Method()); })
        .def_property_readonly("unit", [](const CalibrationPullback_& value) { return Text(value.Unit()); })
        .def_property_readonly("boundary", [](const CalibrationPullback_& value) { return Text(value.Boundary()); })
        .def("matches", &CalibrationPullback_::Matches, py::arg("other"));
    m.def("CalibrationPullback_New", &BoundaryInput, py::arg("calibration"));
    BindSeeds<CalibrationParameterAdjoints_>(m, "CalibrationParameterAdjoints_", "CalibrationParameterAdjoints_New");
    BindSeeds<CalibrationDirectQuoteAdjoints_>(m, "CalibrationDirectQuoteAdjoints_", "CalibrationDirectQuoteAdjoints_New");
    WithCopies(py::class_<CalibrationQuoteRisk_>(m, "CalibrationQuoteRisk_"))
        .def_property_readonly("calibration", [](const CalibrationQuoteRisk_& value) { return value.Calibration(); })
        .def_property_readonly("calibration_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.CalibrationAdjoints()); })
        .def_property_readonly("direct_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.DirectAdjoints()); })
        .def_property_readonly("total_adjoints", [](const CalibrationQuoteRisk_& value) { return Matrix_<>(value.TotalAdjoints()); })
        .def_property_readonly("method", [](const CalibrationQuoteRisk_& value) { return Text(value.Method()); })
        .def_property_readonly("unit", [](const CalibrationQuoteRisk_& value) { return Text(value.Unit()); })
        .def_property_readonly("boundary", [](const CalibrationQuoteRisk_& value) { return Text(value.Boundary()); });
    m.def(
        "PullbackCalibration",
        [](const py::object& calibration, const py::object& parameters, const py::object& direct) {
            const auto source = RequiredInput<CalibrationPullback_>(calibration, "PullbackCalibration; calibration", "CalibrationPullback_");
            const auto seeds =
                RequiredInput<CalibrationParameterAdjoints_>(parameters, "PullbackCalibration; parameter_adjoints", "CalibrationParameterAdjoints_");
            const auto contribution = DirectInput(direct);
            py::gil_scoped_release release;
            return Dal::PullbackCalibration(source, seeds, contribution);
        },
        py::arg("calibration"), py::arg("parameter_adjoints"), py::kw_only(), py::arg("direct") = py::none());
}
