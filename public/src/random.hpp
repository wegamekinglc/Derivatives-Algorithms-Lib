//
// Created by wegam on 2022/9/24.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    class PseudoRSG_;
    class SobolRSG_;
    Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, int seed, int ndim = 1);
    Handle_<SobolRSG_> NewSobolRSG(const String_& name, int i_path, int ndim = 1);

    void GetPseudoRSGUniform(const Handle_<PseudoRSG_>& f, int num_path, Matrix_<>* y);
}