//
// bindings_global.cpp — global state bindings (evaluation date, opaque types)
//

#include "bindings.h"

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/random.hpp>
#include <dal-public/src/script.hpp>

using namespace Dal;

void init_bindings_global(py::module_& m) {
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

    // =======================================================================
    // Global evaluation date
    // =======================================================================
    m.def("EvaluationDate_Get", []() {
        return GetEvaluationDate();
    });

    m.def("EvaluationDate_Set", [](const Date_& d) {
        SetEvaluationDate(d);
    });
}
