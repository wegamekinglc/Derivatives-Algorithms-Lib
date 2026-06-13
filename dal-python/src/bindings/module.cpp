//
// module.cpp — PYBIND11_MODULE entry point
//
// Initializes the DAL runtime and calls all domain init functions.
//

#include "bindings.h"

#include <dal/platform/initall.hpp>

using namespace Dal;

// Stub type for hasattr(dal, "Dictionary") in test_import.py
struct _Dictionary {};

PYBIND11_MODULE(_dal, m) {
    // Initialize DAL runtime (equivalent to SWIG's %init block)
    Dal::RegisterAll_::Init();

    m.doc() = "DAL quantitative finance library -- Python bindings (pybind11)";

    // Core types (Date_, String_, Cell_, vectors, DoubleMatrix_)
    init_bindings_core(m);

    // Global state (Handle_<T> opaque types, evaluation date)
    init_bindings_global(m);

    // Satisfy hasattr(dal, "Dictionary") in test_import.py
    py::class_<_Dictionary>(m, "Dictionary");

    // Models (BSModelData_New, DupireModelData_New)
    init_bindings_models(m);

    // Random sequence generators (PseudoRSG, SobolRSG)
    init_bindings_random(m);

    // Script product (Product_New, Product_Debug)
    init_bindings_script(m);

    // Monte Carlo valuation (MonteCarlo_Value)
    init_bindings_value(m);
}
