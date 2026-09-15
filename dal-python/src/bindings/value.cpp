//
// Created by Codex on 2026/9/15.
//

#include "bindings.h"

#include <limits>
#include <pybind11/stl.h>

#include <dal/platform/platform.hpp>

#include <dal-public/src/value.hpp>

#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    int PathCount(const py::handle& value, const char* function) {
        const auto context = InputContext(value, std::string(function) + "; num_path / numPath",
                                          "a positive integer in 1.." + std::to_string(std::numeric_limits<int>::max()), "InvalidPathCount");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyIndex_Check(value.ptr()))
            throw py::type_error(context);
        const auto integer = py::reinterpret_steal<py::object>(PyNumber_Index(value.ptr()));
        if (!integer) {
            PyErr_Clear();
            throw py::type_error(context);
        }
        int overflow = 0;
        const auto count = PyLong_AsLongLongAndOverflow(integer.ptr(), &overflow);
        if (PyErr_Occurred())
            throw py::error_already_set();
        REQUIRE2(!overflow && count > 0 && count <= std::numeric_limits<int>::max(),
                 String_(context + "; number of Monte Carlo paths must be positive"), ScriptError_);
        return static_cast<int>(count);
    }

    std::optional<Date_> EvaluationDate(const py::handle& value) {
        if (value.is_none())
            return std::nullopt;
        const auto context =
            InputContext(value, "ScriptValuationSettings_; evaluation_date / valuation.evaluationDate_", "a valid date (DAL Date_) or None");
        if (!py::isinstance<Date_>(value))
            throw py::type_error(context);
        const auto result = py::cast<Date_>(value);
        REQUIRE2(result.IsValid(), String_(context), ScriptError_);
        return result;
    }

    TodayFixingPolicy_ TodayPolicy(const py::handle& value) {
        using Policy_ = TodayFixingPolicy_::Value_;
        const auto context =
            InputContext(value, "ScriptValuationSettings_; today_fixing / valuation.todayFixingPolicy_",
                         "Model or RequireHistorical (exact string or TodayFixingPolicy_)", "InvalidSetting: InvalidTodayFixingPolicy");
        if (py::isinstance<Policy_>(value)) {
            const auto policy = py::cast<Policy_>(value);
            REQUIRE2(policy == Policy_::MODEL || policy == Policy_::REQUIREHISTORICAL, String_(context), ScriptError_);
            return TodayFixingPolicy_(policy);
        }
        if (!py::isinstance<py::str>(value) && !py::isinstance<String_>(value))
            throw py::type_error(context);
        const auto name =
            Text(StringInput(value, "ScriptValuationSettings_; today_fixing / valuation.todayFixingPolicy_ (Model or RequireHistorical)",
                             "InvalidSetting: InvalidTodayFixingPolicy"));
        REQUIRE2(name == "Model" || name == "RequireHistorical", String_(context), ScriptError_);
        return TodayFixingPolicy_(name == "Model" ? Policy_::MODEL : Policy_::REQUIREHISTORICAL);
    }

    Vector_<ModelIndexBinding_> ModelBindings(const py::handle& value) {
        Vector_<ModelIndexBinding_> result;
        if (value.is_none())
            return result;
        if (!py::isinstance<py::dict>(value))
            throw py::type_error(
                InputContext(value, "ScriptValuationSettings_; model_bindings / valuation.modelBindings_", "dict of str keys and values or None"));
        for (const auto& item : py::reinterpret_borrow<py::dict>(value)) {
            const auto field = "ScriptValuationSettings_; model_bindings / valuation.modelBindings_[" + std::to_string(result.size()) + "]";
            const auto asset = StringInput(item.first, field + ".assetName_ (key)");
            const auto index = StringInput(item.second, field + ".indexName_ (value for " + Text(asset) + ")");
            result.push_back({asset, index});
        }
        return result;
    }

    py::dict BindingDict(const ScriptValuationSettings_& settings) {
        py::dict result;
        for (const auto& entry : settings.modelBindings_)
            result[py::str(Text(entry.assetName_))] = py::str(Text(entry.indexName_));
        return result;
    }

    Handle_<MarketFixingSnapshot_> Fixings(const py::handle& value) {
        if (value.is_none())
            return {};
        if (!py::isinstance<MarketFixingSnapshot_>(value))
            throw py::type_error(InputContext(value, "ScriptValuationSettings_; fixings / valuation.fixings_", "MarketFixingSnapshot_ or None"));
        return Handle_<MarketFixingSnapshot_>(py::cast<std::shared_ptr<MarketFixingSnapshot_>>(value));
    }

    String_ Method(const py::handle& value) {
        const auto result = StringInput(value, "MonteCarloSettings_; method / simulation.rsg_");
        try {
            Script::ValidateRNG(result);
        } catch (const Exception_& error) {
            THROW2(String_(error.what()) + "; MonteCarloSettings_; method", ScriptError_);
        }
        return result;
    }

    bool Boolean(const py::handle& value, const char* field) {
        if (!PyBool_Check(value.ptr()))
            throw py::type_error(InputContext(value, std::string("MonteCarloSettings_; ") + field, "bool"));
        return value.ptr() == Py_True;
    }

    std::optional<bool> Compiled(const py::handle& value) {
        return value.is_none() ? std::nullopt : std::optional<bool>(Boolean(value, "compiled / simulation.compiled_"));
    }

    double PositiveFloat(const py::handle& value, const std::string& context) {
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            py::error_already_set error;
            if (error.matches(PyExc_OverflowError))
                THROW2(String_(context), ScriptError_);
            if (error.matches(PyExc_TypeError))
                throw py::type_error(context);
            throw error;
        }
        REQUIRE2(std::isfinite(result) && result > 0.0, String_(context), ScriptError_);
        return result;
    }

    double Smoothing(const py::handle& value) {
        const auto context = InputContext(value, "MonteCarloSettings_; smooth / simulation.smooth_", "a finite positive int or float, excluding bool",
                                          "InvalidSetting: InvalidSmoothing");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        return PositiveFloat(value, context);
    }

    double LegacySmoothing(const py::handle& value) {
        return PositiveFloat(value, InputContext(value, "MonteCarlo_Value; smooth / simulation.smooth_", "a finite positive floating-point value",
                                                 "InvalidSetting: InvalidSmoothing"));
    }

    std::map<std::string, double> Value(const Handle_<ScriptProductData_>& product,
                                        const Handle_<ModelData_>& model,
                                        int numPath,
                                        const ScriptValuationSettings_& valuation,
                                        const MonteCarloSettings_& simulation) {
        std::map<String_, double> result;
        {
            py::gil_scoped_release release;
            result = ValueByMonteCarlo(product, model, numPath, valuation, simulation);
        }
        std::map<std::string, double> values;
        for (const auto& entry : result)
            values[Text(entry.first)] = entry.second;
        return values;
    }
} // namespace

void init_bindings_value(py::module_& m) {
    py::enum_<TodayFixingPolicy_::Value_>(m, "TodayFixingPolicy_")
        .value("MODEL", TodayFixingPolicy_::Value_::MODEL)
        .value("REQUIREHISTORICAL", TodayFixingPolicy_::Value_::REQUIREHISTORICAL);

    WithCopies(py::class_<ScriptValuationSettings_>(m, "ScriptValuationSettings_"))
        .def(
            py::init([](const py::object& evaluationDate, const py::object& todayFixing, const py::object& modelBindings, const py::object& fixings) {
                ScriptValuationSettings_ settings;
                settings.evaluationDate_ = EvaluationDate(evaluationDate);
                settings.todayFixingPolicy_ = TodayPolicy(todayFixing);
                settings.modelBindings_ = ModelBindings(modelBindings);
                settings.fixings_ = Fixings(fixings);
                return settings;
            }),
            py::kw_only(), py::arg("evaluation_date") = py::none(), py::arg("today_fixing") = "Model", py::arg("model_bindings") = py::none(),
            py::arg("fixings") = py::none())
        .def_property(
            "evaluation_date", [](const ScriptValuationSettings_& settings) { return settings.evaluationDate_; },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->evaluationDate_ = EvaluationDate(value); })
        .def_property(
            "today_fixing", [](const ScriptValuationSettings_& settings) { return settings.todayFixingPolicy_.Switch(); },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->todayFixingPolicy_ = TodayPolicy(value); })
        .def_property("model_bindings", &BindingDict,
                      [](ScriptValuationSettings_* settings, const py::object& value) { settings->modelBindings_ = ModelBindings(value); })
        .def_property(
            "fixings", [](const ScriptValuationSettings_& settings) { return std::const_pointer_cast<MarketFixingSnapshot_>(settings.fixings_); },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->fixings_ = Fixings(value); });

    WithCopies(py::class_<MonteCarloSettings_>(m, "MonteCarloSettings_"))
        .def(py::init([](const py::object& method, const py::object& useBb, const py::object& enableAad, const py::object& smooth,
                         const py::object& compiled) {
                 return MonteCarloSettings_{Method(method), Boolean(useBb, "use_bb / simulation.useBb_"),
                                            Boolean(enableAad, "enable_aad / simulation.enableAad_"), Smoothing(smooth), Compiled(compiled)};
             }),
             py::kw_only(), py::arg("method") = "sobol", py::arg("use_bb") = false, py::arg("enable_aad") = false, py::arg("smooth") = 0.01,
             py::arg("compiled") = py::none())
        .def_property(
            "method", [](const MonteCarloSettings_& settings) { return Text(settings.rsg_); },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->rsg_ = Method(value); })
        .def_property(
            "use_bb", [](const MonteCarloSettings_& settings) { return settings.useBb_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->useBb_ = Boolean(value, "use_bb / simulation.useBb_"); })
        .def_property(
            "enable_aad", [](const MonteCarloSettings_& settings) { return settings.enableAad_; },
            [](MonteCarloSettings_* settings, const py::object& value) {
                settings->enableAad_ = Boolean(value, "enable_aad / simulation.enableAad_");
            })
        .def_property(
            "smooth", [](const MonteCarloSettings_& settings) { return settings.smooth_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->smooth_ = Smoothing(value); })
        .def_property(
            "compiled", [](const MonteCarloSettings_& settings) { return settings.compiled_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->compiled_ = Compiled(value); });

    m.def(
        "MonteCarlo_ValueWithSettings",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& modelData, const py::object& numPath,
           const py::object& valuation, const py::object& simulation) {
            const int count = PathCount(numPath, "MonteCarlo_ValueWithSettings");
            const Handle_<ScriptProductData_> nativeProduct(product);
            const Handle_<ModelData_> nativeModel(modelData);
            const auto settings =
                SettingsInput<ScriptValuationSettings_>(valuation, "MonteCarlo_ValueWithSettings; valuation", "ScriptValuationSettings_");
            const auto execution = SettingsInput<MonteCarloSettings_>(simulation, "MonteCarlo_ValueWithSettings; simulation", "MonteCarloSettings_");
            return Value(nativeProduct, nativeModel, count, settings, execution);
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::kw_only(), py::arg("valuation") = py::none(),
        py::arg("simulation") = py::none());

    m.def(
        "ScriptValuation_Explain",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& modelData, const py::object& valuation) {
            const Handle_<ScriptProductData_> nativeProduct(product);
            const Handle_<ModelData_> nativeModel(modelData);
            const auto settings =
                SettingsInput<ScriptValuationSettings_>(valuation, "ScriptValuation_Explain; valuation", "ScriptValuationSettings_");
            py::gil_scoped_release release;
            const auto result = ExplainScriptValuation(nativeProduct, nativeModel, settings);
            return Text(result);
        },
        py::arg("product"), py::arg("modelData"), py::kw_only(), py::arg("valuation") = py::none());

    m.def(
        "MonteCarlo_Value",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& modelData, const py::object& numPath,
           const std::string& method, bool useBb, bool enableAad, const py::object& smooth, std::optional<bool> compiled) {
            const int count = PathCount(numPath, "MonteCarlo_Value");
            const Handle_<ScriptProductData_> nativeProduct(product);
            const Handle_<ModelData_> nativeModel(modelData);
            const MonteCarloSettings_ simulation{String_(method), useBb, enableAad, LegacySmoothing(smooth), compiled};
            return Value(nativeProduct, nativeModel, count, ScriptValuationSettings_(), simulation);
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::arg("method") = "sobol", py::arg("use_bb") = false,
        py::arg("enable_aad") = false, py::arg("smooth") = 0.01, py::arg("compiled") = py::none());
}
