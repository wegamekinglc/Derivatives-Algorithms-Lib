//
// module.cpp — PYBIND11_MODULE entry point
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

    // Calendar types (Holidays_, BizDayConvention_, CountBusDays_)
    init_bindings_calendar(m);

    // Core types (Date_, String_, Cell_, vectors, DoubleMatrix_)
    init_bindings_core(m);

    // Global state (Handle_<T> opaque types, evaluation date)
    init_bindings_global(m);

    // Alias Dictionary to Python's built-in dict type so hasattr(dal, "Dictionary")
    // and isinstance(result, dal.Dictionary) both pass (the latter works because
    // MonteCarlo_Value returns a dict via pybind11's std::map auto-conversion).
    m.attr("Dictionary") = py::module_::import("builtins").attr("dict");

    // Curve calibration (instruments, curves, calibration)
    init_bindings_curve(m);

    // Models (BSModelData_New, DupireModelData_New)
    init_bindings_models(m);

    // Random sequence generators (PseudoRSG, SobolRSG)
    init_bindings_random(m);

    // Script product (Product_New, Product_Debug)
    init_bindings_script(m);

    // Monte Carlo valuation (MonteCarlo_Value)
    init_bindings_value(m);
}
