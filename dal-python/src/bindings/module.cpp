//
// module.cpp - PYBIND11_MODULE entry point
//
// Initializes the DAL runtime and calls all domain init functions.
//

#include "bindings.h"

#include <dal-public/src/global.hpp>

using namespace Dal;

PYBIND11_MODULE(_dal, m) {
    // Initialize DAL runtime (calendars, currency conventions, index parsers)
    Dal::InitGlobalData();

    m.doc() = "DAL quantitative finance library -- Python bindings (pybind11)";

    init_bindings_calendar(m);

    init_bindings_core(m);

    init_bindings_global(m);

    // Alias Dictionary to Python's built-in dict type so hasattr(dal, "Dictionary")
    // and isinstance(result, dal.Dictionary) both pass (the latter works because
    // MonteCarlo_Value returns a dict via pybind11's std::map auto-conversion).
    m.attr("Dictionary") = py::module_::import("builtins").attr("dict");

    init_bindings_curve(m);

    init_bindings_models(m);

    init_bindings_random(m);

    init_bindings_script(m);

    init_bindings_value(m);
}
