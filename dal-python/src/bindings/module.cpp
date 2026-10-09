//
// module.cpp - PYBIND11_MODULE entry point
//
// Initializes the DAL runtime and calls all domain init functions.
//

#include "bindings.h"

#include <cstring>

#include <dal-public/src/global.hpp>

using namespace Dal;

PYBIND11_MODULE(_dal, m) {
    //  pybind11's default translator decodes what() as strict UTF-8; a lone non-ASCII
    //  byte would surface as UnicodeDecodeError and hide the DAL error type. Handle only
    //  undecodable messages and rethrow the rest so pybind11's own type mappings hold.
    py::register_exception_translator([](std::exception_ptr exception) {
        try {
            if (exception)
                std::rethrow_exception(exception);
        } catch (const py::builtin_exception&) {
            throw;
        } catch (const std::exception& error) {
            const size_t length = std::strlen(error.what());
            PyObject* strict = PyUnicode_DecodeUTF8(error.what(), length, nullptr);
            if (strict) {
                Py_DECREF(strict);
                throw;
            }
            PyErr_Clear();
            PyObject* message = PyUnicode_DecodeUTF8(error.what(), length, "backslashreplace");
            if (!message) {
                PyErr_Clear();
                PyErr_SetString(PyExc_RuntimeError, "DAL error with an undecodable message");
                return;
            }
            PyErr_SetObject(PyExc_RuntimeError, message);
            Py_DECREF(message);
        }
    });

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
    init_bindings_gsr(m);

    init_bindings_random(m);

    init_bindings_script(m);

    init_bindings_value(m);
    init_bindings_risk(m);
    init_bindings_weightedrisk(m);
    init_bindings_jacobianrisk(m);
    init_bindings_portfoliorisk(m);
    init_bindings_dupirerisk(m);
    init_bindings_calibrationrisk(m);
    init_bindings_calibrationriskrequest(m);
    init_bindings_dupireriskrequest(m);
}
