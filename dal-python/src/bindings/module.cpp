//
// module.cpp - PYBIND11_MODULE entry point
//
// Initializes the DAL runtime and calls all domain init functions.
//

#include "bindings.h"

#include <dal-public/src/global.hpp>

using namespace Dal;

namespace {
    bool DecodesAsUtf8(const char* message) {
        try {
            static_cast<void>(py::str(message)); //  strict probe: raises on invalid UTF-8
            return true;
        } catch (const py::error_already_set&) {
            return false;
        }
    }

    //  Re-throw with the escaped text: pybind11's built-in translator then maps it to
    //  RuntimeError on every supported pybind11 version, without raw CPython calls
    void SetLenientRuntimeError(const std::exception& error) {
        try {
            const py::object message = py::module_::import("codecs").attr("decode")(py::bytes(error.what()), "utf-8", "backslashreplace");
            throw std::runtime_error(message.cast<std::string>());
        } catch (const py::error_already_set&) {
            throw std::runtime_error("DAL error with an undecodable message");
        }
    }
} // namespace

PYBIND11_MODULE(_dal, m) {
    //  pybind11's default translator decodes what() as strict UTF-8; a lone non-ASCII
    //  byte would surface as UnicodeDecodeError and hide the DAL error type. Rethrow
    //  decodable messages untouched so pybind11's own type mappings hold, and rethrow
    //  undecodable ones with backslash-escaped text as a plain runtime_error.
    py::register_exception_translator([](std::exception_ptr exception) {
        try {
            if (exception)
                std::rethrow_exception(exception);
        } catch (const py::builtin_exception&) {
            throw;
        } catch (const std::exception& error) {
            if (DecodesAsUtf8(error.what()))
                throw;
            SetLenientRuntimeError(error);
        }
    });

    // Initialize DAL runtime (calendars, currency conventions, index parsers)
    Dal::InitGlobalData();

    m.doc() = "DAL quantitative finance library -- Python bindings (pybind11)";

    //  Underscore-prefixed, so star imports keep it out of the public dal namespace
    m.def(
        "_test_throw_undecodable_error",
        []() {
            //  0xC3 alone is invalid UTF-8; pins the lenient translator above
            throw std::runtime_error(std::string("undecodable byte: '") + static_cast<char>(0xC3) + "'");
        },
        "Test hook: raise a std::exception whose message is not valid UTF-8.");

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

    init_bindings_dupirecurvature(m);
    init_bindings_ratecurvature(m);
    init_bindings_montecarlocurvature(m);
    init_bindings_lsmccurvature(m);
}
