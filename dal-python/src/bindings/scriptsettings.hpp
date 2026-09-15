//
// Created by Codex on 2026/9/15.
//

#pragma once

#include <pybind11/pybind11.h>

#include <dal/platform/platform.hpp>
#include <dal/script/settings.hpp>

namespace Dal::Python {
    namespace py = pybind11;

    inline std::string Text(const String_& value) { return std::string(value.data(), value.size()); }

    inline std::string InputRepr(const py::handle& value) {
        try {
            return py::cast<std::string>(py::repr(value));
        } catch (const py::error_already_set& error) {
            if (!PyLong_Check(value.ptr()) || !error.matches(PyExc_ValueError))
                throw;
            return "<integer exceeds Python decimal conversion limit>";
        }
    }

    inline std::string
    InputContext(const py::handle& value, const std::string& field, const std::string& expected, const std::string& identifier = "InvalidSetting") {
        std::string detail = "type=" + py::cast<std::string>(py::type::of(value).attr("__name__"));
        if (value.is_none() || PyUnicode_Check(value.ptr()) || PyLong_Check(value.ptr()) || PyFloat_Check(value.ptr()) || PyBytes_Check(value.ptr()))
            detail += ", value=" + InputRepr(value);
        else if (py::isinstance<String_>(value))
            detail += ", value=" + py::cast<std::string>(py::repr(py::str(Text(py::cast<String_>(value)))));
        else if (py::hasattr(py::type::of(value), "__entries"))
            detail += ", value=" + py::cast<std::string>(py::repr(value));
        return identifier + ": " + field + "; " + detail + "; expected " + expected;
    }

    inline bool IsEnum(const py::handle& value) {
        return py::hasattr(py::type::of(value), "__entries") || py::isinstance(value, py::module_::import("enum").attr("Enum"));
    }

    inline String_ StringInput(const py::handle& value, const std::string& field, const std::string& identifier = "InvalidSetting") {
        if (!py::isinstance<py::str>(value) && !py::isinstance<String_>(value))
            throw py::type_error(InputContext(value, field, "str or DAL String_ without NUL", identifier));
        const auto result = py::isinstance<String_>(value) ? py::cast<String_>(value) : String_(py::cast<std::string>(value));
        REQUIRE2(result.find('\0') == String_::npos, String_(InputContext(value, field, "str or DAL String_ without NUL", identifier)), ScriptError_);
        return result;
    }

    inline String_ SettingStringInput(const py::handle& value, const std::string& field, const std::string& identifier = "InvalidSetting") {
        if (IsEnum(value))
            throw py::type_error(InputContext(value, field, "non-enum str or DAL String_ without NUL", identifier));
        return StringInput(value, field, identifier);
    }

    template <class T_> T_ SettingsInput(const py::handle& value, const std::string& field, const char* typeName) {
        if (value.is_none())
            return T_();
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, std::string(typeName) + " or None"));
        return py::cast<T_>(value);
    }

    template <class T_> py::class_<T_> WithCopies(py::class_<T_> cls) {
        return cls.def("__copy__", [](const T_& value) { return T_(value); })
            .def("__deepcopy__", [](const T_& value, const py::dict&) { return T_(value); }, py::arg("memo"));
    }
} // namespace Dal::Python
