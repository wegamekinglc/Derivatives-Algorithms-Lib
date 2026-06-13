//
// pybind11 bindings for the DAL public API.
// Replaces the SWIG-based bindings previously in dal-python/swig/.
//
// All bindings are in a single compilation unit to avoid linker issues
// with template instantiations across translation units.
//

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/time/date.hpp>
#include <dal/string/strings.hpp>
#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/random.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

#include <sstream>

namespace py = pybind11;
using namespace Dal;

// ---------------------------------------------------------------------------
// Helper: convert Python iterable to std::vector<T>
// ---------------------------------------------------------------------------
template <typename T>
static std::vector<T> pyListToVector(const py::iterable& src) {
    std::vector<T> result;
    for (auto item : src) {
        result.push_back(py::cast<T>(item));
    }
    return result;
}

// Opaque vector types for pybind11 STL bindings
PYBIND11_MAKE_OPAQUE(std::vector<double>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Date_>);
PYBIND11_MAKE_OPAQUE(std::vector<Dal::Cell_>);

// Stub type for hasattr(dal, "Dictionary") in test_import.py
struct _Dictionary {};

PYBIND11_MODULE(_dal, m) {
    // Initialize DAL runtime (equivalent to SWIG's %init block)
    Dal::RegisterAll_::Init();

    m.doc() = "DAL quantitative finance library -- Python bindings (pybind11)";

    // =======================================================================
    // Core types
    // =======================================================================

    // -- Date_ --------------------------------------------------------------
    py::class_<Date_>(m, "Date_")
        .def(py::init<int, int, int>(),
             py::arg("yyyy"), py::arg("mm"), py::arg("dd"))
        .def("AddDays", &Date_::AddDays, py::arg("days"))
        .def("__repr__", [](const Date_& d) -> std::string {
            std::ostringstream out;
            out << Date::ToString(d);
            return out.str();
        })
        .def("__lt__", [](const Date_& a, const Date_& b) { return a < b; })
        .def("__le__", [](const Date_& a, const Date_& b) { return a <= b; })
        .def("__gt__", [](const Date_& a, const Date_& b) { return a > b; })
        .def("__ge__", [](const Date_& a, const Date_& b) { return a >= b; })
        .def("__eq__", [](const Date_& a, const Date_& b) { return a == b; })
        .def("__sub__", [](const Date_& a, const Date_& b) { return a - b; });

    // Module-level date accessor functions
    m.def("Year", [](const Date_& d) { return Date::Year(d); });
    m.def("Month", [](const Date_& d) { return Date::Month(d); });
    m.def("Day", [](const Date_& d) { return Date::Day(d); });

    // -- String_ ------------------------------------------------------------
    py::class_<String_>(m, "String_")
        .def(py::init<const char*>(), py::arg("src"))
        .def(py::init<const std::string&>(), py::arg("src"))
        .def("__repr__", [](const String_& s) -> std::string {
            return s.c_str();
        });

    // -- Cell_ --------------------------------------------------------------
    py::class_<Cell_>(m, "Cell_")
        .def(py::init<bool>(), py::arg("b"))
        .def(py::init<double>(), py::arg("d"))
        .def(py::init<const Date_&>(), py::arg("dt"))
        .def(py::init<const String_&>(), py::arg("s"))
        .def(py::init<const char*>(), py::arg("s"));

    // -- STL vector bindings ------------------------------------------------
    py::bind_vector<std::vector<double>>(m, "DoubleVector");
    py::bind_vector<std::vector<std::string>>(m, "StrVector");
    py::bind_vector<std::vector<Date_>>(m, "DateVector");
    py::bind_vector<std::vector<Cell_>>(m, "CellVector");

    // -- DoubleMatrix_ ------------------------------------------------------
    py::class_<Matrix_<>>(m, "DoubleMatrix_")
        .def(py::init<int, int, double>(),
             py::arg("rows"), py::arg("cols"), py::arg("fill") = 0.0)
        .def("__call__", [](const Matrix_<>& m, int i, int j) {
            return m(i, j);
        });

    // =======================================================================
    // Handle_<T> opaque types
    //
    // Handle_<T> inherits std::shared_ptr<const T>.  We bind each concrete
    // type with std::shared_ptr<const T> as its holder so that pybind11's
    // built-in shared_ptr machinery handles creation, lifetime, and
    // extraction.  No methods are exposed — these are opaque handles that
    // flow between factory functions and valuation functions.
    // =======================================================================
    // Bind Handle_<T> types as opaque classes with std::shared_ptr holder.
    // Handle_<T> inherits std::shared_ptr<const T>, which we const_cast to
    // std::shared_ptr<T> below — safe for opaque types with no method access.
    // This is the standard pybind11 pattern for shared_ptr-held types.
    py::class_<ModelData_, std::shared_ptr<ModelData_>>(m, "ModelData_");
    py::class_<ScriptProductData_, std::shared_ptr<ScriptProductData_>>(m, "ScriptProductData_");
    py::class_<PseudoRSG_, std::shared_ptr<PseudoRSG_>>(m, "PseudoRSG_");
    py::class_<SobolRSG_, std::shared_ptr<SobolRSG_>>(m, "SobolRSG_");
    py::class_<Storable_, std::shared_ptr<Storable_>>(m, "Storable_");

    // Satisfy hasattr(dal, "Dictionary") in test_import.py
    py::class_<_Dictionary>(m, "Dictionary");

    // =======================================================================
    // Global evaluation date
    // =======================================================================
    m.def("EvaluationDate_Get", []() {
        return GetEvaluationDate();
    });

    m.def("EvaluationDate_Set", [](const Date_& d) {
        SetEvaluationDate(d);
    });

    // =======================================================================
    // Models
    // =======================================================================

    m.def("BSModelData_New",
        [](double spot, double vol, double rate, double div)
            -> std::shared_ptr<ModelData_> {
            return std::const_pointer_cast<ModelData_>(
                NewBSModelData(String_("BSModelData_"), spot, vol, rate, div));
        },
        py::arg("spot"), py::arg("vol"), py::arg("rate"), py::arg("div"));

    m.def("DupireModelData_New",
        [](double spot, double rate, double repo,
           const py::iterable& spots,
           const py::iterable& times,
           const Matrix_<>& vols) -> std::shared_ptr<ModelData_> {

            // Convert Python iterables to Vector_<> (replicating SWIG's copy)
            Vector_<> new_spots;
            for (auto item : spots)
                new_spots.push_back(py::cast<double>(item));

            Vector_<> new_times;
            for (auto item : times)
                new_times.push_back(py::cast<double>(item));

            return std::const_pointer_cast<ModelData_>(
                NewDupireModelData(
                    String_("DupireModelData_"), spot, rate, repo,
                    new_spots, new_times, vols));
        },
        py::arg("spot"), py::arg("rate"), py::arg("repo"),
        py::arg("spots"), py::arg("times"), py::arg("vols"));

    // =======================================================================
    // Random sequence generators
    // =======================================================================

    m.def("PseudoRSG_New",
        [](int seed, int ndim) -> std::shared_ptr<PseudoRSG_> {
            return std::const_pointer_cast<PseudoRSG_>(
                NewPseudoRSG(String_("MRG32k32a"), seed, ndim));
        },
        py::arg("seed"), py::arg("ndim") = 1);

    m.def("PseudoRSG_Get_Uniform",
        [](const std::shared_ptr<PseudoRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetPseudoRSGUniform(
                Handle_<PseudoRSG_>(std::const_pointer_cast<const PseudoRSG_>(rsg)),
                num_path, &m);
            return m;
        });

    m.def("PseudoRSG_Get_Normal",
        [](const std::shared_ptr<PseudoRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetPseudoRSGNormal(
                Handle_<PseudoRSG_>(std::const_pointer_cast<const PseudoRSG_>(rsg)),
                num_path, &m);
            return m;
        });

    m.def("SobolRSG_New",
        [](int i_path, int ndim) -> std::shared_ptr<SobolRSG_> {
            return std::const_pointer_cast<SobolRSG_>(
                NewSobolRSG(String_("SobolRSG_"), i_path, ndim));
        },
        py::arg("i_path"), py::arg("ndim") = 1);

    m.def("SobolRSG_Get_Uniform",
        [](const std::shared_ptr<SobolRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetSobolRSGUniform(
                Handle_<SobolRSG_>(std::const_pointer_cast<const SobolRSG_>(rsg)),
                num_path, &m);
            return m;
        });

    m.def("SobolRSG_Get_Normal",
        [](const std::shared_ptr<SobolRSG_>& rsg, int num_path) {
            Matrix_<> m;
            GetSobolRSGNormal(
                Handle_<SobolRSG_>(std::const_pointer_cast<const SobolRSG_>(rsg)),
                num_path, &m);
            return m;
        });

    // =======================================================================
    // Script product
    // =======================================================================

    m.def("Product_New",
        [](const py::iterable& dates, const py::iterable& events)
            -> std::shared_ptr<ScriptProductData_> {

            // Convert Python iterables to Vector_<>, replicating SWIG's copy
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

    // =======================================================================
    // Monte Carlo valuation
    // =======================================================================

    m.def("MonteCarlo_Value",
        [](const std::shared_ptr<ScriptProductData_>& product,
           const std::shared_ptr<ModelData_>& modelData,
           int num_path,
           const std::string& method,
           bool use_bb,
           bool enable_aad,
           double smooth) {

            auto res = ValueByMonteCarlo(
                Handle_<ScriptProductData_>(
                    std::const_pointer_cast<const ScriptProductData_>(product)),
                Handle_<ModelData_>(
                    std::const_pointer_cast<const ModelData_>(modelData)),
                num_path, String_(method), use_bb, enable_aad, smooth);
            std::map<std::string, double> rtn;
            for (auto& d : res)
                rtn[d.first.c_str()] = d.second;
            return rtn;
        },
        py::arg("product"),
        py::arg("modelData"),
        py::arg("num_path"),
        py::arg("method") = "sobol",
        py::arg("use_bb") = false,
        py::arg("enable_aad") = false,
        py::arg("smooth") = 0.01
    );
}
