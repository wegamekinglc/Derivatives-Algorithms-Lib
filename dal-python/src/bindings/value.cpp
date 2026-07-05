//
// value.cpp - Monte Carlo valuation bindings (MonteCarlo_Value)
//

#include "bindings.h"

#include <pybind11/stl.h>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/string/strings.hpp>

#include <dal-public/src/value.hpp>

using namespace Dal;

void init_bindings_value(py::module_& m) {
    m.def("MonteCarlo_Value",
        [](const std::shared_ptr<ScriptProductData_>& product,
           const std::shared_ptr<ModelData_>& modelData,
           int num_path,
           const std::string& method,
           bool use_bb,
           bool enable_aad,
           double smooth,
           std::optional<bool> compiled) {

            auto res = ValueByMonteCarlo(
                Handle_<ScriptProductData_>(
                    std::const_pointer_cast<const ScriptProductData_>(product)),
                Handle_<ModelData_>(
                    std::const_pointer_cast<const ModelData_>(modelData)),
                num_path, String_(method), use_bb, enable_aad, smooth, compiled);
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
        py::arg("smooth") = 0.01,
        py::arg("compiled") = py::none()
    );
}
