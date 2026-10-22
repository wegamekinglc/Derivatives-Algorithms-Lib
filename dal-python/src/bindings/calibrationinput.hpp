#pragma once

#include "scriptsettings.hpp"
#include <climits>
#include <dal-public/src/calibrationinstrument.hpp>
#include <pybind11/stl.h>
#include <set>

namespace Dal::Python::Calibration {
    template <class T_, class = void> struct HasToken_ : std::false_type {};
    template <class T_> struct HasToken_<T_, std::void_t<decltype(std::declval<T_>().String())>> : std::true_type {};
    inline String_ RawText(const py::handle& value, const std::string& field) {
        if (IsEnum(value) || (!py::isinstance<py::str>(value) && !py::isinstance<String_>(value)))
            throw py::type_error(field + "; expected text");
        return py::isinstance<String_>(value) ? py::cast<String_>(value) : String_(py::cast<std::string>(value));
    }

    inline bool Numeric(const py::handle& value) {
        return !PyBool_Check(value.ptr()) && !IsEnum(value) && (PyFloat_Check(value.ptr()) || PyLong_Check(value.ptr()));
    }
    inline double Number(const py::handle& value, const std::string& field, const String_& code = "InvalidCalibrationPlan", bool finite = true) {
        if (!Numeric(value))
            throw py::type_error(field + "; expected a number, excluding bool");
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            if (!finite)
                return std::numeric_limits<double>::infinity();
            CalibrationInputFailure(code, String_(field), "finite binary64 number");
        }
        if (finite && !std::isfinite(result))
            CalibrationInputFailure(code, String_(field), "finite binary64 number");
        return result;
    }
    template <class T_> struct Input_ {
        static T_ Read(const py::handle& value, const std::string& field) {
            if constexpr (HasToken_<T_>::value) {
                if (py::isinstance<T_>(value))
                    return py::cast<T_>(value);
                const auto token = RawText(value, field);
                try {
                    const T_ result(token);
                    if (Text(String_(result.String())) != Text(token))
                        CalibrationInputFailure("InvalidCalibrationPlan", String_(field), "exact enum token", token);
                    return result;
                } catch (const CalibrationError_&) {
                    throw;
                } catch (const Exception_&) {
                    CalibrationInputFailure("InvalidCalibrationPlan", String_(field), "supported enum token", token);
                }
            } else {
                if (!py::isinstance<T_>(value))
                    throw py::type_error(field + "; expected the corresponding typed value");
                return py::cast<T_>(value);
            }
        }
    };
    template <> struct Input_<String_> {
        static String_ Read(const py::handle& value, const std::string& field) { return RawText(value, field); }
    };
    template <> struct Input_<double> {
        static double Read(const py::handle& value, const std::string& field) { return Number(value, field); }
    };
    template <> struct Input_<bool> {
        static bool Read(const py::handle& value, const std::string& field) {
            if (!PyBool_Check(value.ptr()))
                throw py::type_error(field + "; expected bool");
            return py::cast<bool>(value);
        }
    };
    template <> struct Input_<int> {
        static int Read(const py::handle& value, const std::string& field) {
            if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyLong_Check(value.ptr()))
                throw py::type_error(field + "; expected int, excluding bool");
            const double number = Number(value, field);
            if (number < INT_MIN || number > INT_MAX)
                CalibrationInputFailure("InvalidCalibrationPlan", String_(field), "representable 32-bit integer");
            return static_cast<int>(number);
        }
    };
    template <class T_> struct Input_<Handle_<T_>> {
        static Handle_<T_> Read(const py::handle& value, const std::string& field) {
            if (!py::isinstance<T_>(value))
                throw py::type_error(field + "; expected the corresponding typed handle");
            return Handle_<T_>(py::cast<std::shared_ptr<T_>>(value));
        }
    };
    template <class T_> struct Input_<Vector_<T_>> {
        static Vector_<T_> Read(const py::handle& value, const std::string& field) {
            if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
                throw py::type_error(field + "; expected list or tuple");
            Vector_<T_> result;
            for (const auto& row : py::reinterpret_borrow<py::iterable>(value))
                result.push_back(Input_<T_>::Read(row, field + "/" + std::to_string(result.size())));
            return result;
        }
    };
    template <class K_, class V_> struct Input_<std::map<K_, V_>> {
        static std::map<K_, V_> Read(const py::handle& value, const std::string& field) {
            if (!py::isinstance<py::dict>(value))
                throw py::type_error(field + "; expected dict");
            std::map<K_, V_> result;
            for (const auto& row : py::reinterpret_borrow<py::dict>(value))
                result.emplace(Input_<K_>::Read(row.first, field), Input_<V_>::Read(row.second, field));
            return result;
        }
    };
    class Fields_ {
        py::kwargs values_;
        std::set<std::string> used_;

    public:
        explicit Fields_(py::kwargs values) : values_(std::move(values)) {}
        template <class T_> T_ Required(const char* key) {
            used_.insert(key);
            if (!values_.contains(key))
                throw py::type_error(std::string("missing keyword: ") + key);
            return Input_<T_>::Read(values_[key], std::string("/") + key);
        }
        template <class T_> T_ Default(const char* key, T_ fallback) {
            used_.insert(key);
            return values_.contains(key) ? Input_<T_>::Read(values_[key], std::string("/") + key) : fallback;
        }
        template <class T_> std::optional<T_> Maybe(const char* key) {
            used_.insert(key);
            if (!values_.contains(key) || values_[key].is_none())
                return std::nullopt;
            return Input_<T_>::Read(values_[key], std::string("/") + key);
        }
        void Finish() const {
            for (const auto& row : values_) {
                const auto key = py::cast<std::string>(row.first);
                if (!used_.count(key))
                    throw py::type_error("unknown keyword: " + key);
            }
        }
    };
    template <class T_> py::object Output(const T_& value) {
        if constexpr (HasToken_<T_>::value)
            return py::str(value.String());
        else
            return py::cast(value, py::return_value_policy::copy);
    }
    inline py::object Output(const String_& value) { return py::str(Text(value)); }
    template <class T_> py::object Output(const Handle_<T_>& value) { return value ? py::cast(std::const_pointer_cast<T_>(value)) : py::none(); }
    template <class T_> py::object Output(const std::optional<T_>& value) { return value ? Output(*value) : py::none(); }
    template <class T_> py::object Output(const Vector_<T_>& values) {
        py::list result;
        for (const auto& value : values)
            result.append(Output(value));
        return result;
    }
    template <class K_, class V_> py::object Output(const std::map<K_, V_>& values) {
        py::dict result;
        for (const auto& row : values)
            result[Output(row.first)] = Output(row.second);
        return result;
    }
    template <class T_, class F_> py::class_<T_> Struct(py::module_& m, const char* name, F_ construct) {
        return py::class_<T_>(m, name).def(py::init([construct](py::kwargs values) {
            Fields_ fields(std::move(values));
            auto result = construct(fields);
            fields.Finish();
            return result;
        }));
    }
    template <class T_, class M_> void Property(py::class_<T_>& cls, const char* name, M_ T_::* member) {
        cls.def_property_readonly(name, [member](const T_& value) { return Output(value.*member); });
    }
} // namespace Dal::Python::Calibration
