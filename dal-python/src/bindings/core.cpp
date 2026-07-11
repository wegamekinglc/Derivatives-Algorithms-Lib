//
// core.cpp - core DAL types (Date_, String_, Cell_, vectors, DoubleMatrix_)
//

#include "bindings.h"

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>

#include <limits>
#include <sstream>

using namespace Dal;

// Opaque vector types for pybind11 STL bindings
PYBIND11_MAKE_OPAQUE(std::vector<double>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Date_>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Cell_>);

namespace {

    Matrix_<> MatrixFromDimensions(int nRows, int nCols, double fill) {
        if (nRows < 0 || nCols < 0)
            throw py::value_error("DoubleMatrix_ dimensions must be non-negative");
        return Matrix_<>(nRows, nCols, fill);
    }

    Matrix_<> MatrixFromRows(const py::iterable& rows) {
        std::vector<std::vector<double>> values;
        size_t nCols = 0;
        for (const py::handle rowHandle : rows) {
            if (!py::isinstance<py::sequence>(rowHandle) || py::isinstance<py::str>(rowHandle) || py::isinstance<py::bytes>(rowHandle)) {
                throw py::value_error("DoubleMatrix_ rows must be numeric sequences");
            }
            const py::sequence row = py::reinterpret_borrow<py::sequence>(rowHandle);
            const size_t rowSize = static_cast<size_t>(py::len(row));
            if (!values.empty() && rowSize != nCols)
                throw py::value_error("DoubleMatrix_ rows must form a rectangular matrix");
            if (values.empty())
                nCols = rowSize;

            std::vector<double> converted;
            converted.reserve(rowSize);
            for (const py::handle value : row)
                converted.push_back(py::cast<double>(value));
            values.push_back(std::move(converted));
        }

        if (values.size() > static_cast<size_t>(std::numeric_limits<int>::max()) || nCols > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw py::value_error("DoubleMatrix_ dimensions exceed the native integer range");
        }

        Matrix_<> result(static_cast<int>(values.size()), static_cast<int>(nCols));
        for (size_t i = 0; i < values.size(); ++i) {
            for (size_t j = 0; j < nCols; ++j)
                result(static_cast<int>(i), static_cast<int>(j)) = values[i][j];
        }
        return result;
    }

    int NormalizeMatrixIndex(int index, int size) {
        if (index < 0)
            index += size;
        if (index < 0 || index >= size)
            throw py::index_error("DoubleMatrix_ index out of range");
        return index;
    }

    std::pair<int, int> NormalizeMatrixIndices(const Matrix_<>& matrix, std::pair<int, int> indices) {
        indices.first = NormalizeMatrixIndex(indices.first, matrix.Rows());
        indices.second = NormalizeMatrixIndex(indices.second, matrix.Cols());
        return indices;
    }

} // namespace

void init_bindings_core(py::module_& m) {
    py::class_<Date_>(m, "Date_")
        .def(py::init<int, int, int>(), py::arg("yyyy"), py::arg("mm"), py::arg("dd"))
        .def("AddDays", &Date_::AddDays, py::arg("days"))
        .def("__repr__",
             [](const Date_& d) -> std::string {
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
        .def("__repr__", [](const String_& s) -> std::string { return s.c_str(); });

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
        .def(py::init(&MatrixFromDimensions), py::arg("rows"), py::arg("cols"), py::arg("fill") = 0.0)
        .def(py::init(&MatrixFromRows), py::arg("rows"))
        .def("__call__",
             [](const Matrix_<>& m, int i, int j) -> double {
                 const auto indices = NormalizeMatrixIndices(m, {i, j});
                 return m(indices.first, indices.second);
             })
        .def("__getitem__",
             [](const Matrix_<>& m, std::pair<int, int> indices) -> double {
                 indices = NormalizeMatrixIndices(m, indices);
                 return m(indices.first, indices.second);
             })
        .def("__setitem__",
             [](Matrix_<>& m, std::pair<int, int> indices, double value) {
                 indices = NormalizeMatrixIndices(m, indices);
                 m(indices.first, indices.second) = value;
             })
        .def("Rows", &Matrix_<>::Rows)
        .def("Cols", &Matrix_<>::Cols);
}
