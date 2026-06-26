//
// core.cpp — core DAL types (Date_, String_, Cell_, vectors, DoubleMatrix_)
//

#include "bindings.h"

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/string/strings.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>

#include <sstream>

using namespace Dal;

// Opaque vector types for pybind11 STL bindings
PYBIND11_MAKE_OPAQUE(std::vector<double>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Date_>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Cell_>);

void init_bindings_core(py::module_& m) {
    py::class_<Date_>(m, "Date_")
        .def(py::init<int, int, int>(),
             py::arg("yyyy"), py::arg("mm"), py::arg("dd"))
        .def("AddDays", &Date_::AddDays, py::arg("days"))
        .def("__repr__", [](const Date_& d) -> std::string {
            std::ostringstream out;
            out << Date::ToString(d);
            return out.str();
        })
        .def("__lt__", [](const Date_& a, const Date_& b) { return a < b; })
        .def("__le__", [](const Date_& a, const Date_& b) { return a <= b; })
        .def("__gt__", [](const Date_& a, const Date_& b) { return a > b; })
        .def("__ge__", [](const Date_& a, const Date_& b) { return a >= b; })
        .def("__eq__", [](const Date_& a, const Date_& b) { return a == b; })
        .def("__sub__", [](const Date_& a, const Date_& b) { return a - b; });

    // Module-level date accessor functions
    m.def("Year", [](const Date_& d) { return Date::Year(d); });
    m.def("Month", [](const Date_& d) { return Date::Month(d); });
    m.def("Day", [](const Date_& d) { return Date::Day(d); });

    py::class_<String_>(m, "String_")
        .def(py::init<const char*>(), py::arg("src"))
        .def(py::init<const std::string&>(), py::arg("src"))
        .def("__repr__", [](const String_& s) -> std::string {
            return s.c_str();
        });

    py::class_<Cell_>(m, "Cell_")
        .def(py::init<bool>(), py::arg("b"))
        .def(py::init<double>(), py::arg("d"))
        .def(py::init<const Date_&>(), py::arg("dt"))
        .def(py::init<const String_&>(), py::arg("s"))
        .def(py::init<const char*>(), py::arg("s"));

    py::bind_vector<std::vector<double>>(m, "DoubleVector");
    py::bind_vector<std::vector<std::string>>(m, "StrVector");
    py::bind_vector<std::vector<Date_>>(m, "DateVector");
    py::bind_vector<std::vector<Cell_>>(m, "CellVector");

    py::class_<Matrix_<>>(m, "DoubleMatrix_")
        .def(py::init<int, int, double>(),
             py::arg("rows"), py::arg("cols"), py::arg("fill") = 0.0)
        .def("__call__", [](const Matrix_<>& m, int i, int j) -> double {
            return m(i, j);
        })
        .def("Rows", &Matrix_<>::Rows)
        .def("Cols", &Matrix_<>::Cols);
}
