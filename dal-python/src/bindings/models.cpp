//
// models.cpp - model data bindings (BSModelData_, DupireModelData_)
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/matrix/matrixs.hpp>

#include <dal-public/src/models.hpp>

using namespace Dal;

void init_bindings_models(py::module_& m) {
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

            // Convert Python iterables to Vector_<> for the factory function
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
}
