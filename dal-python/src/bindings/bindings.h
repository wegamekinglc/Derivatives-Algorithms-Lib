//
// bindings.h — shared header for DAL pybind11 bindings
//
// Declares init_bindings_<domain>() functions called from module.cpp.
//

#pragma once

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_bindings_calendar(py::module_& m);
void init_bindings_core(py::module_& m);
void init_bindings_curve(py::module_& m);
void init_bindings_global(py::module_& m);
void init_bindings_models(py::module_& m);
void init_bindings_random(py::module_& m);
void init_bindings_script(py::module_& m);
void init_bindings_value(py::module_& m);
