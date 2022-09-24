//
// Created by wegam on 2022/9/24.
//


#include <public/src/random.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/random/sobol.hpp>

namespace Dal {
    Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, double seed, double ndim) {
        return Handle_<PseudoRSG_>(new PseudoRSG_(name, seed, ndim));
    }

    Handle_<SobolRSG_> NewSobolRSG(const String_& name, double i_path, double ndim) {
        return Handle_<SobolRSG_>(new SobolRSG_(name, i_path, ndim));
    }
}