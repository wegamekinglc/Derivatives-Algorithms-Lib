//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <limits>
#include <optional>
#include <vector>

#include "scriptsettings.hpp"

namespace Dal::Python {
    struct RiskRequestContext_ {
        const char* field_;
        const char* identifier_;
    };

    template <class T_> std::vector<T_> CopyRiskVector(const Vector_<T_>& values) { return {values.begin(), values.end()}; }

    inline std::vector<std::string> RiskTextVector(const Vector_<String_>& values) {
        std::vector<std::string> result;
        result.reserve(values.size());
        for (const auto& value : values)
            result.push_back(Text(value));
        return result;
    }

    inline std::optional<std::vector<std::string>> RiskTextVector(const std::optional<Vector_<String_>>& values) {
        return values ? std::optional<std::vector<std::string>>(RiskTextVector(*values)) : std::nullopt;
    }

    inline py::sequence RequestSequence(const py::handle& value, const RiskRequestContext_& context) {
        if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
            throw py::type_error(InputContext(value, context.field_, "list or tuple, or None", context.identifier_));
        return py::reinterpret_borrow<py::sequence>(value);
    }

    inline std::optional<Vector_<String_>> RequestIds(const py::handle& value, const RiskRequestContext_& context) {
        if (value.is_none())
            return std::nullopt;
        Vector_<String_> ids;
        for (const auto& id : RequestSequence(value, context))
            ids.push_back(SettingStringInput(id, context.field_, context.identifier_));
        return ids;
    }

    inline std::optional<Vector_<>> RequestFactors(const py::handle& value, const RiskRequestContext_& context) {
        if (value.is_none())
            return std::nullopt;
        Vector_<> factors;
        for (const auto& item : RequestSequence(value, context)) {
            if (PyBool_Check(item.ptr()) || IsEnum(item) || (!PyLong_Check(item.ptr()) && !PyFloat_Check(item.ptr())))
                throw py::type_error(InputContext(item, context.field_, "int or float, excluding bool", context.identifier_));
            const double factor = PyFloat_AsDouble(item.ptr());
            if (PyErr_Occurred()) {
                PyErr_Clear();
                THROW2(String_(std::string(context.identifier_) + ": report_factors; expected representable floating-point values"), ScriptError_);
            }
            factors.push_back(factor);
        }
        return factors;
    }

    inline size_t RequestUnsigned(const py::handle& value,
                                  const RiskRequestContext_& context,
                                  const char* expected = "nonnegative size_t integer or None, excluding bool") {
        const auto error = InputContext(value, context.field_, expected, context.identifier_);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyIndex_Check(value.ptr()))
            throw py::type_error(error);
        const auto integer = py::reinterpret_steal<py::object>(PyNumber_Index(value.ptr()));
        if (!integer)
            throw py::error_already_set();
        const auto result = PyLong_AsUnsignedLongLong(integer.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            THROW2(String_(error), ScriptError_);
        }
        REQUIRE2(result <= (std::numeric_limits<size_t>::max)(), String_(error), ScriptError_);
        return static_cast<size_t>(result);
    }

    inline std::optional<size_t> RequestBudget(const py::handle& value, const RiskRequestContext_& context) {
        return value.is_none() ? std::nullopt : std::optional<size_t>(RequestUnsigned(value, context));
    }

    template <class T_> T_ RequiredRiskInput(const py::handle& value, const std::string& field, const char* type, const char* identifier) {
        if (!py::isinstance<T_>(value))
            throw py::type_error(InputContext(value, field, type, identifier));
        return py::cast<T_>(value);
    }
} // namespace Dal::Python
