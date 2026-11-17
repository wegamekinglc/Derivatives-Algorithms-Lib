#include "bindings.h"

#include <cmath>
#include <pybind11/stl.h>

#include <dal-public/src/marketivlocalvol.hpp>

#include "giltestbarrier.hpp"
#include "riskrequest.hpp"

using namespace Dal;
using namespace Dal::Python;

namespace {
    constexpr const char* INVALID = "InvalidMarketIvLocalVolRequest";
    PyObject* errorType = nullptr;
    thread_local bool barrierArmed = false;
    GilTestBarrier_ barrier;

    void Translate(std::exception_ptr exception) {
        try {
            if (exception)
                std::rethrow_exception(exception);
        } catch (const MarketIvLocalVolError_& error) {
            const auto type = py::reinterpret_borrow<py::object>(errorType);
            const auto instance = type(error.what());
            instance.attr("code") = Text(error.code_);
            instance.attr("stage") = Text(error.stage_);
            instance.attr("field") = Text(error.field_);
            instance.attr("expected") = Text(error.expected_);
            instance.attr("actual") = Text(error.actual_);
            PyErr_SetObject(errorType, instance.ptr());
        }
    }
    String_ TextInput(const py::handle& value, const char* field) {
        if (IsEnum(value) || (!py::isinstance<py::str>(value) && !py::isinstance<String_>(value)))
            throw py::type_error(InputContext(value, field, "nonempty str or DAL String_ without NUL", INVALID));
        const auto text = py::isinstance<String_>(value) ? py::cast<String_>(value) : String_(py::cast<std::string>(value));
        if (text.empty() || text.find('\0') != String_::npos)
            throw py::value_error(std::string(INVALID) + ": " + field + "; expected nonempty text without NUL");
        return text;
    }
    double Real(const py::handle& value, const char* field) {
        const auto context = InputContext(value, field, "finite int/float excluding bool and enums", INVALID);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || (!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr())))
            throw py::type_error(context);
        const double x = PyFloat_AsDouble(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(context);
        }
        if (!std::isfinite(x))
            throw py::value_error(context);
        return x;
    }
    size_t Count(const py::handle& value, const char* field) {
        const auto context = InputContext(value, field, "non-bool int in size_t range", INVALID);
        if (PyBool_Check(value.ptr()) || IsEnum(value) || !PyLong_Check(value.ptr()))
            throw py::type_error(context);
        const size_t x = PyLong_AsSize_t(value.ptr());
        if (PyErr_Occurred()) {
            PyErr_Clear();
            throw py::value_error(context);
        }
        return x;
    }
    template <class T_, class F_>
    bool SetField(const Vector_<MarketIvSettings::Field_<MarketIvLocalVolModelSettings_, T_>>& fields,
                  const std::string& key,
                  const py::handle& value,
                  F_ convert,
                  MarketIvLocalVolModelSettings_* settings) {
        for (const auto& field : fields)
            if (key == field.name_) {
                settings->*field.member_ = convert(value, field.name_);
                return true;
            }
        return false;
    }
    MarketIvLocalVolModelSettings_
    NewSettings(const py::object& index, const py::object& currency, const py::object& factor, const py::object& horizon, const py::kwargs& kwargs) {
        MarketIvLocalVolModelSettings_ settings;
        settings.index_ = TextInput(index, "index");
        settings.currency_ = TextInput(currency, "currency");
        settings.factor_ = TextInput(factor, "factor");
        settings.horizon_ = Real(horizon, "horizon");
        for (const auto& entry : kwargs) {
            const auto key = py::cast<std::string>(entry.first);
            if (key == "horizon" || (!SetField(MarketIvLocalVolFields::RealFields(), key, entry.second, Real, &settings) &&
                                     !SetField(MarketIvLocalVolFields::CountFields(), key, entry.second, Count, &settings)))
                throw py::value_error(std::string(INVALID) + ": /settings/" + key + "; expected known optional key");
        }
        try {
            ValidateMarketIvLocalVolModelSettings(settings);
        } catch (const MarketIvLocalVolError_& error) {
            throw py::value_error(error.what());
        }
        return settings;
    }
    template <class T_> py::object Output(const T_& x) { return py::cast(x); }
    py::object Output(const String_& x) { return py::str(Text(x)); }
    template <class T_> py::dict Report(const T_& value) {
        py::dict result;
        MarketIvLocalVolFields::Visit(value, [&](const char* key, const auto& x) { result[key] = Output(x); });
        return result;
    }
    template <class T_> py::list Rows(const Vector_<T_>& values) {
        py::list result;
        for (const auto& value : values)
            result.append(Report(value));
        return result;
    }
    template <class T_> T_ Input(const py::handle& value, const char* field) {
        return RequiredRiskInput<T_>(value, field, "matching DAL value type", INVALID);
    }
    void RegisterSettings(py::module_& m) {
        auto type = WithCopies(py::class_<MarketIvLocalVolModelSettings_>(m, "MarketIvLocalVolModelSettings_"));
        type.def(py::init(&NewSettings), py::arg("index"), py::arg("currency"), py::arg("factor"), py::arg("horizon"));
        for (const auto& field : MarketIvLocalVolFields::TextFields())
            type.def_property_readonly(field.name_, [member = field.member_](const MarketIvLocalVolModelSettings_& x) { return Text(x.*member); });
        for (const auto& field : MarketIvLocalVolFields::RealFields())
            type.def_property_readonly(field.name_, [member = field.member_](const MarketIvLocalVolModelSettings_& x) { return x.*member; });
        for (const auto& field : MarketIvLocalVolFields::CountFields())
            type.def_property_readonly(field.name_, [member = field.member_](const MarketIvLocalVolModelSettings_& x) { return x.*member; });
        type.def("__reduce_ex__", [](const MarketIvLocalVolModelSettings_&, int) -> py::object {
            throw MarketIvLocalVolError_("PickleUnsupported", "binding", "/settings", "in-process value", "pickle");
        });
    }
    void RegisterModel(py::module_& m) {
        using Model_ = MarketIvLocalVolModelData_;
        py::class_<Model_, ModelData_, Storable_, std::shared_ptr<Model_>>(m, "MarketIvLocalVolModelData_")
            .def_property_readonly("source_identity", [](const Model_& x) { return Text(x.SourceIdentity()); })
            .def_property_readonly("model_identity", [](const Model_& x) { return Text(x.ModelIdentity()); })
            .def_property_readonly("configuration", &Model_::Configuration)
            .def_property_readonly("summary", [](const Model_& x) { return Report(x.Summary()); })
            .def_property_readonly("carry_terms", [](const Model_& x) { return Rows(x.CarryTerms()); })
            .def_property_readonly("capabilities", [](const Model_& x) { return Report(x.Capabilities()); })
            .def_property_readonly("warnings", [](const Model_& x) { return Rows(x.Warnings()); })
            .def_property_readonly("construction_execution", [](const Model_& x) { return Report(x.ConstructionExecution()); })
            .def("__copy__", [](const std::shared_ptr<Model_>& x) { return x; })
            .def("__deepcopy__", [](const std::shared_ptr<Model_>& x, const py::dict&) { return x; })
            .def("__reduce_ex__", [](const Model_&, int) -> py::object {
                throw MarketIvLocalVolError_("PickleUnsupported", "binding", "/model", "in-process handle", "pickle");
            });
        m.def(
            "MarketIvLocalVolModelData_New",
            [](const py::object& name, const py::object& grid, const py::object& settings) {
                const auto label = TextInput(name, "name");
                const auto source = Input<MarketIvDupireResult_>(grid, "grid_result");
                const auto config = Input<MarketIvLocalVolModelSettings_>(settings, "settings");
                py::gil_scoped_release release;
                barrier.Wait(&barrierArmed, "MarketIvLocalVol: GIL test barrier timed out");
                return std::const_pointer_cast<Model_>(NewMarketIvLocalVolModelData(label, source, config));
            },
            py::arg("name"), py::arg("grid_result"), py::arg("settings"));
        m.def(
            "MarketIvLocalVolModelData_Get_Execution",
            [](const std::shared_ptr<ScriptProductData_>& product, const std::shared_ptr<Model_>& model, const py::object& valuation,
               const py::object& simulation) {
                if (!product || !model)
                    throw py::type_error("InvalidMarketIvLocalVolRequest: product/model; expected nonempty typed handles");
                const auto nativeProduct = product;
                const auto nativeModel = model;
                const auto v = SettingsInput<ScriptValuationSettings_>(valuation, "valuation", "ScriptValuationSettings_");
                const auto s = SettingsInput<MonteCarloSettings_>(simulation, "simulation", "MonteCarloSettings_");
                MarketIvLocalVolExecution_ execution;
                {
                    py::gil_scoped_release release;
                    execution = InspectMarketIvLocalVolExecution(*nativeProduct, *nativeModel, v, s);
                }
                return Report(execution);
            },
            py::arg("product"), py::arg("model"), py::kw_only(), py::arg("valuation") = py::none(), py::arg("simulation") = py::none());
    }
} // namespace

void init_bindings_marketivlocalvol(py::module_& m) {
    py::exception<MarketIvLocalVolError_> error(m, "MarketIvLocalVolError", PyExc_RuntimeError);
    errorType = error.ptr();
    py::register_exception_translator(Translate);
    RegisterSettings(m);
    RegisterModel(m);
    m.def("_MarketIvLocalVolGilBarrier_ArmForTesting", [] { barrier.Arm(&barrierArmed); });
    m.def("_MarketIvLocalVolGilBarrier_AwaitEntryForTesting", [] {
        py::gil_scoped_release release;
        return barrier.AwaitEntry();
    });
    m.def("_MarketIvLocalVolGilBarrier_ReleaseForTesting", [] { return barrier.Release(); });
}
