//
// global.cpp - global state bindings (evaluation date, opaque types)
//

#include "bindings.h"

#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
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
    py::class_<Storable_, std::shared_ptr<Storable_>>(m, "Storable_")
        .def_property_readonly(
            "name",
            [](const Storable_& value) {
                return std::string(value.Name().data(), value.Name().size());
            })
        .def_property_readonly(
            "type",
            [](const Storable_& value) {
                return std::string(value.Type().data(), value.Type().size());
            });

    m.def("EvaluationDate_Get", []() {
        py::gil_scoped_release release;
        return GetEvaluationDate();
    });

    m.def("EvaluationDate_Set", [](const Date_& d) {
        py::gil_scoped_release release;
        SetEvaluationDate(d);
    });

    m.def("_EvaluationDateBarrier_AvailableForTesting", []() {
        py::gil_scoped_release release;
        XGLOBAL::ValuationMutationGuard_ probe(std::try_to_lock);
        return probe.OwnsLock();
    });
}
