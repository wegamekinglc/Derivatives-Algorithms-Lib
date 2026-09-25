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
    long long IntegerInput(const py::handle& value, const std::string& context, long long lowest, long long highest, const std::string& rangeError) {
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyIndex_Check(value.ptr()))
            throw py::type_error(context);
        const auto integer = py::reinterpret_steal<py::object>(PyNumber_Index(value.ptr()));
        if (!integer) {
            PyErr_Clear();
            throw py::type_error(context);
        }
        int overflow = 0;
        const long long result = PyLong_AsLongLongAndOverflow(integer.ptr(), &overflow);
        if (PyErr_Occurred())
            throw py::error_already_set();
        REQUIRE2(!overflow && result >= lowest && result <= highest, String_(context + rangeError), ScriptError_);
        return result;
    }

    int PathCount(const py::handle& value, const char* function) {
        const auto context = InputContext(value, std::string(function) + "; num_path / numPath",
                                          "a positive integer in 1.." + std::to_string(std::numeric_limits<int>::max()), "InvalidPathCount");
        return static_cast<int>(IntegerInput(value, context, 1, std::numeric_limits<int>::max(), "; number of Monte Carlo paths must be positive"));
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
        const auto name = SettingStringInput(value, "ScriptValuationSettings_; today_fixing / valuation.todayFixingPolicy_ (Model or RequireHistorical)",
                                             "InvalidSetting: InvalidTodayFixingPolicy");
        TodayFixingPolicy_ policy;
        //  explicit branch, not a macro argument: keeps the parse call unconditional
        if (!Script::TryParseTodayFixingPolicy(name, &policy))
            THROW2(String_(context), ScriptError_);
        return policy;
    }

    Handle_<MarketFixingSnapshot_> Fixings(const py::handle& value) {
        if (value.is_none())
            return {};
        if (!py::isinstance<MarketFixingSnapshot_>(value))
            throw py::type_error(InputContext(value, "ScriptValuationSettings_; fixings / valuation.fixings_", "MarketFixingSnapshot_ or None"));
        return Handle_<MarketFixingSnapshot_>(py::cast<std::shared_ptr<MarketFixingSnapshot_>>(value));
    }

    String_ Method(const py::handle& value) {
        const auto result = SettingStringInput(value, "MonteCarloSettings_; method / simulation.rsg_");
        try {
            Script::ValidateRNG(result);
        } catch (const Exception_& error) {
            THROW2(String_(error.what()) + "; MonteCarloSettings_; method", ScriptError_);
        }
        return result;
    }

    String_ LsmcPolicyRiskMode(const py::handle& value) {
        const auto context = "InvalidSetting: InvalidLsmcPolicyRiskMode; MonteCarloSettings_; lsmc_policy_risk_mode / simulation.lsmcPolicyRiskMode_";
        const auto result = SettingStringInput(value, context, "InvalidSetting: InvalidLsmcPolicyRiskMode");
        try {
            Script::ValidateLsmcPolicyRiskMode(result);
        } catch (const Exception_&) {
            THROW2(String_(context) + "; expected Frozen or RetrainedBump", ScriptError_);
        }
        return result;
    }

    double LsmcPolicyBumpRelative(const py::handle& value) {
        const auto context = InputContext(value, "MonteCarloSettings_; lsmc_policy_bump_relative / simulation.lsmcPolicyBumpRelative_",
                                          "a finite float in (0, 0.1], excluding bool", "InvalidSetting: InvalidLsmcPolicyBumpRelative");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double result = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            py::error_already_set error;
            if (error.matches(PyExc_OverflowError))
                THROW2(String_(context), ScriptError_);
            throw error;
        }
        try {
            Script::ValidateLsmcPolicyBumpRelative(result);
        } catch (const Exception_&) {
            THROW2(String_(context), ScriptError_);
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
        try {
            Script::ValidateSmoothing(result);
        } catch (const Exception_&) {
            THROW2(String_(context), ScriptError_);
        }
        return result;
    }

    double Smoothing(const py::handle& value) {
        const auto context = InputContext(value, "MonteCarloSettings_; smooth / simulation.smooth_", "a finite positive int or float, excluding bool",
                                          "InvalidSetting: InvalidSmoothing");
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        return PositiveFloat(value, context);
    }

    int BasisDegree(const py::handle& value) {
        const auto context = InputContext(value, "MonteCarloSettings_; lsmc_basis_degree / simulation.lsmcBasisDegree_",
                                          "an integer in 1..8, excluding bool", "InvalidSetting: InvalidLsmcBasisDegree");
        const auto degree = IntegerInput(value, context, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
                                         "; LSMC basis degree must be an integer between 1 and 8");
        try {
            Script::ValidateLsmcBasisDegree(static_cast<int>(degree));
        } catch (const Exception_&) {
            THROW2(String_(context + "; LSMC basis degree must be an integer between 1 and 8"), ScriptError_);
        }
        return static_cast<int>(degree);
    }

    std::optional<int> LsmcPathCount(const py::handle& value, const char* field, const char* member, const char* error, void (*validate)(int)) {
        if (value.is_none())
            return std::nullopt;
        const auto context = InputContext(value, "MonteCarloSettings_; " + std::string(field) + " / simulation." + member,
                                          "a positive integer in 1.." + std::to_string(std::numeric_limits<int>::max()) + " or None, excluding bool",
                                          "InvalidSetting: " + std::string(error));
        const auto count =
            IntegerInput(value, context, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), "; LSMC paths must be a positive integer");
        try {
            validate(static_cast<int>(count));
        } catch (const Exception_&) {
            THROW2(String_(context), ScriptError_);
        }
        return static_cast<int>(count);
    }

    std::optional<int> TrainingPaths(const py::handle& value) {
        return LsmcPathCount(value, "lsmc_training_paths", "lsmcTrainingPaths_", "InvalidLsmcTrainingPaths", Script::ValidateLsmcTrainingPaths);
    }

    std::optional<int> ValidationPaths(const py::handle& value) {
        return LsmcPathCount(value, "lsmc_validation_paths", "lsmcValidationPaths_", "InvalidLsmcValidationPaths",
                             Script::ValidateLsmcValidationPaths);
    }

    std::optional<int> RqmcReplicates(const py::handle& value) {
        if (value.is_none())
            return std::nullopt;
        const auto context = InputContext(value, "MonteCarloSettings_; lsmc_rqmc_replicates / simulation.lsmcRqmcReplicates_",
                                          "an integer in 2..2147483647 or None, excluding bool", "InvalidSetting: InvalidLsmcRqmcReplicates");
        const auto count = IntegerInput(value, context, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
                                        "; LSMC RQMC replicate count must be at least 2");
        try {
            Script::ValidateLsmcRqmcReplicates(static_cast<int>(count));
        } catch (const Exception_&) {
            THROW2(String_(context), ScriptError_);
        }
        return static_cast<int>(count);
    }

    std::optional<int> RqmcSeed(const py::handle& value, const char* field, const char* member) {
        if (value.is_none())
            return std::nullopt;
        const auto context = InputContext(value, "MonteCarloSettings_; " + std::string(field) + " / simulation." + member,
                                          "a nonnegative integer in 0..2147483647 or None, excluding bool", "InvalidSetting: InvalidLsmcSeed");
        const auto seed =
            IntegerInput(value, context, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), "; LSMC seed must be nonnegative");
        try {
            Script::ValidateLsmcSeed(static_cast<int>(seed), member);
        } catch (const Exception_&) {
            THROW2(String_(context), ScriptError_);
        }
        return static_cast<int>(seed);
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
            py::init([](const py::object& evaluationDate, const py::object& todayFixing, const py::object& fixings) {
                ScriptValuationSettings_ settings;
                settings.evaluationDate_ = EvaluationDate(evaluationDate);
                settings.todayFixingPolicy_ = TodayPolicy(todayFixing);
                settings.fixings_ = Fixings(fixings);
                return settings;
            }),
            py::kw_only(), py::arg("evaluation_date") = py::none(), py::arg("today_fixing") = "Model", py::arg("fixings") = py::none())
        .def_property(
            "evaluation_date", [](const ScriptValuationSettings_& settings) { return settings.evaluationDate_; },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->evaluationDate_ = EvaluationDate(value); })
        .def_property(
            "today_fixing", [](const ScriptValuationSettings_& settings) { return settings.todayFixingPolicy_.Switch(); },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->todayFixingPolicy_ = TodayPolicy(value); })
        .def_property(
            "fixings", [](const ScriptValuationSettings_& settings) { return std::const_pointer_cast<MarketFixingSnapshot_>(settings.fixings_); },
            [](ScriptValuationSettings_* settings, const py::object& value) { settings->fixings_ = Fixings(value); });

    WithCopies(py::class_<MonteCarloSettings_>(m, "MonteCarloSettings_"))
        .def(py::init([](const py::object& method, const py::object& useBb, const py::object& enableAad, const py::object& smooth,
                         const py::object& compiled, const py::object& lsmcBasisDegree, const py::object& lsmcTrainingPaths,
                         const py::object& lsmcValidationPaths, const py::object& lsmcRqmcReplicates, const py::object& lsmcTrainingSeed,
                         const py::object& lsmcPricingSeed, const py::object& lsmcPolicyRiskMode, const py::object& lsmcPolicyBumpRelative) {
                 return MonteCarloSettings_{Method(method),
                                            Boolean(useBb, "use_bb / simulation.useBb_"),
                                            Boolean(enableAad, "enable_aad / simulation.enableAad_"),
                                            Smoothing(smooth),
                                            Compiled(compiled),
                                            BasisDegree(lsmcBasisDegree),
                                            TrainingPaths(lsmcTrainingPaths),
                                            ValidationPaths(lsmcValidationPaths),
                                            RqmcReplicates(lsmcRqmcReplicates),
                                            RqmcSeed(lsmcTrainingSeed, "lsmc_training_seed", "lsmcTrainingSeed_"),
                                            RqmcSeed(lsmcPricingSeed, "lsmc_pricing_seed", "lsmcPricingSeed_"),
                                            LsmcPolicyRiskMode(lsmcPolicyRiskMode),
                                            LsmcPolicyBumpRelative(lsmcPolicyBumpRelative)};
             }),
             py::kw_only(), py::arg("method") = "sobol", py::arg("use_bb") = false, py::arg("enable_aad") = false,
             py::arg("smooth") = Script::DEFAULT_SMOOTH, py::arg("compiled") = py::none(),
             py::arg("lsmc_basis_degree") = Script::DEFAULT_LSMC_BASIS_DEGREE, py::arg("lsmc_training_paths") = py::none(),
             py::arg("lsmc_validation_paths") = py::none(), py::arg("lsmc_rqmc_replicates") = py::none(), py::arg("lsmc_training_seed") = py::none(),
             py::arg("lsmc_pricing_seed") = py::none(), py::arg("lsmc_policy_risk_mode") = "Frozen",
             py::arg("lsmc_policy_bump_relative") = Script::DEFAULT_LSMC_POLICY_BUMP_RELATIVE)
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
            [](MonteCarloSettings_* settings, const py::object& value) { settings->compiled_ = Compiled(value); })
        .def_property(
            "lsmc_basis_degree", [](const MonteCarloSettings_& settings) { return settings.lsmcBasisDegree_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcBasisDegree_ = BasisDegree(value); })
        .def_property(
            "lsmc_training_paths", [](const MonteCarloSettings_& settings) { return settings.lsmcTrainingPaths_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcTrainingPaths_ = TrainingPaths(value); })
        .def_property(
            "lsmc_validation_paths", [](const MonteCarloSettings_& settings) { return settings.lsmcValidationPaths_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcValidationPaths_ = ValidationPaths(value); })
        .def_property(
            "lsmc_rqmc_replicates", [](const MonteCarloSettings_& settings) { return settings.lsmcRqmcReplicates_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcRqmcReplicates_ = RqmcReplicates(value); })
        .def_property(
            "lsmc_training_seed", [](const MonteCarloSettings_& settings) { return settings.lsmcTrainingSeed_; },
            [](MonteCarloSettings_* settings, const py::object& value) {
                settings->lsmcTrainingSeed_ = RqmcSeed(value, "lsmc_training_seed", "lsmcTrainingSeed_");
            })
        .def_property(
            "lsmc_pricing_seed", [](const MonteCarloSettings_& settings) { return settings.lsmcPricingSeed_; },
            [](MonteCarloSettings_* settings, const py::object& value) {
                settings->lsmcPricingSeed_ = RqmcSeed(value, "lsmc_pricing_seed", "lsmcPricingSeed_");
            })
        .def_property(
            "lsmc_policy_risk_mode", [](const MonteCarloSettings_& settings) { return Text(settings.lsmcPolicyRiskMode_); },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcPolicyRiskMode_ = LsmcPolicyRiskMode(value); })
        .def_property(
            "lsmc_policy_bump_relative", [](const MonteCarloSettings_& settings) { return settings.lsmcPolicyBumpRelative_; },
            [](MonteCarloSettings_* settings, const py::object& value) { settings->lsmcPolicyBumpRelative_ = LsmcPolicyBumpRelative(value); });

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
        "ScriptSimulation_Explain",
        [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<ModelData_>& modelData, const py::object& numPath,
           const py::object& valuation, const py::object& simulation) {
            const int count = PathCount(numPath, "ScriptSimulation_Explain");
            const Handle_<ScriptProductData_> nativeProduct(product);
            const Handle_<ModelData_> nativeModel(modelData);
            const auto settings =
                SettingsInput<ScriptValuationSettings_>(valuation, "ScriptSimulation_Explain; valuation", "ScriptValuationSettings_");
            const auto execution = SettingsInput<MonteCarloSettings_>(simulation, "ScriptSimulation_Explain; simulation", "MonteCarloSettings_");
            py::gil_scoped_release release;
            const auto result = ExplainScriptSimulation(nativeProduct, nativeModel, count, settings, execution);
            return Text(result);
        },
        py::arg("product"), py::arg("modelData"), py::arg("num_path"), py::kw_only(), py::arg("valuation") = py::none(),
        py::arg("simulation") = py::none());

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
        py::arg("enable_aad") = false, py::arg("smooth") = Script::DEFAULT_SMOOTH, py::arg("compiled") = py::none());
}
