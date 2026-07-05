//
// global.cpp - global state bindings (evaluation date, opaque types)
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
    // Handle_<T> inherits std::shared_ptr<const T>.  These types are bound
    // with std::shared_ptr<T> as their holder (pybind11 requires a mutable
    // shared_ptr to manage the object lifetime; const-correctness is
    // irrelevant for opaque types with no exposed methods).
    py::class_<ModelData_, std::shared_ptr<ModelData_>>(m, "ModelData_");
    py::class_<ScriptProductData_, std::shared_ptr<ScriptProductData_>>(m, "ScriptProductData_");
    py::class_<PseudoRSG_, std::shared_ptr<PseudoRSG_>>(m, "PseudoRSG_");
    py::class_<SobolRSG_, std::shared_ptr<SobolRSG_>>(m, "SobolRSG_");
    py::class_<Storable_, std::shared_ptr<Storable_>>(m, "Storable_");

    m.def("EvaluationDate_Get", []() {
        return GetEvaluationDate();
    });

    m.def("EvaluationDate_Set", [](const Date_& d) {
        SetEvaluationDate(d);
    });
}
