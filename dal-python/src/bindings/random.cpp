//
// random.cpp — random sequence generator bindings (PseudoRSG, SobolRSG)
//

#include "bindings.h"

#include <dal/math/matrix/matrixs.hpp>

#include <dal-public/src/random.hpp>

using namespace Dal;

void init_bindings_random(py::module_& m) {
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
}
