//
// script.cpp - script product bindings (Product_New, Product_Debug)
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/string/strings.hpp>

#include <dal-public/src/script.hpp>

#include "scriptsettings.hpp"

using namespace Dal;
using namespace Dal::Python;

void init_bindings_script(py::module_& m) {
    WithCopies(py::class_<ScriptProductSettings_>(m, "ScriptProductSettings_"))
        .def(py::init([](const py::object& defaultIndex) {
                 return ScriptProductSettings_{SettingStringInput(defaultIndex, "ScriptProductSettings_; default_index / product.defaultIndex_")};
             }),
             py::kw_only(), py::arg("default_index") = "")
        .def_property(
            "default_index", [](const ScriptProductSettings_& settings) { return Text(settings.defaultIndex_); },
            [](ScriptProductSettings_* settings, const py::object& value) {
                settings->defaultIndex_ = SettingStringInput(value, "ScriptProductSettings_; default_index / product.defaultIndex_");
            });

    m.def(
        "Product_New",
        [](const py::iterable& dates, const py::iterable& events, const py::object& settings) -> std::shared_ptr<ScriptProductData_> {
            Vector_<Cell_> newDates;
            for (auto item : dates) {
                const auto field = "Product_New; dates/events row=" + std::to_string(newDates.size() + 1) + "; dates";
                if (!py::isinstance<Cell_>(item))
                    throw py::type_error(InputContext(item, field, "Cell_"));
                auto cell = py::cast<Cell_>(item);
                const auto* text = std::get_if<String_>(&cell.val_);
                REQUIRE2(!text || text->find('\0') == String_::npos, String_("InvalidSetting: " + field + "; expected text without NUL"),
                         ScriptError_);
                newDates.push_back(std::move(cell));
            }

            Vector_<String_> newEvents;
            for (auto item : events)
                newEvents.push_back(StringInput(item, "Product_New; dates/events row=" + std::to_string(newEvents.size() + 1) + "; events"));

            const auto contract = SettingsInput<ScriptProductSettings_>(settings, "Product_New; settings", "ScriptProductSettings_");
            py::gil_scoped_release release;
            return std::const_pointer_cast<ScriptProductData_>(NewScriptProduct("ScriptProductData_", newDates, newEvents, contract));
        },
        py::arg("dates"), py::arg("events"), py::kw_only(), py::arg("settings") = py::none());

    m.def(
        "Product_Describe",
        [](const std::shared_ptr<ScriptProductData_>& product) {
            const Handle_<ScriptProductData_> nativeProduct(product);
            py::gil_scoped_release release;
            const auto result = DescribeScriptProduct(nativeProduct);
            return std::string(result.data(), result.size());
        },
        py::arg("product"));

    m.def("Product_Debug", [](const std::shared_ptr<ScriptProductData_>& product) -> std::string {
        return DebugScriptProduct(Handle_<ScriptProductData_>(std::const_pointer_cast<const ScriptProductData_>(product))).c_str();
    });

    m.def("Product_DebugJson", [](const std::shared_ptr<ScriptProductData_>& product) -> std::string {
        return DebugScriptProductJson(Handle_<ScriptProductData_>(std::const_pointer_cast<const ScriptProductData_>(product))).c_str();
    });

    m.def(
        "Product_DebugTree",
        [](const std::shared_ptr<ScriptProductData_>& product, bool ascii, int width) -> std::string {
            return DebugScriptProductTree(Handle_<ScriptProductData_>(std::const_pointer_cast<const ScriptProductData_>(product)), ascii, width)
                .c_str();
        },
        py::arg("product"), py::arg("ascii") = false, py::arg("width") = 125);
}
