//
// script.cpp — script product bindings (Product_New, Product_Debug)
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/string/strings.hpp>

#include <dal-public/src/script.hpp>

using namespace Dal;

void init_bindings_script(py::module_& m) {
    // =======================================================================
    // Script product
    // =======================================================================

    m.def("Product_New",
        [](const py::iterable& dates, const py::iterable& events)
            -> std::shared_ptr<ScriptProductData_> {

            // Convert Python iterables to Vector_<> for the script engine
            Vector_<Cell_> new_dates;
            for (auto item : dates)
                new_dates.push_back(py::cast<Cell_>(item));

            Vector_<String_> new_events;
            for (auto item : events)
                new_events.push_back(String_(py::cast<std::string>(item)));

            return std::const_pointer_cast<ScriptProductData_>(
                NewScriptProduct(
                    String_("ScriptProductData_"), new_dates, new_events));
        },
        py::arg("dates"), py::arg("events"));

    m.def("Product_Debug",
        [](const std::shared_ptr<ScriptProductData_>& product) -> std::string {
            return DebugScriptProduct(
                Handle_<ScriptProductData_>(
                    std::const_pointer_cast<const ScriptProductData_>(product))
                ).c_str();
        });
}
