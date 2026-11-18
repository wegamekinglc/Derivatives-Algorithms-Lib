//
// Created by wegam on 2022/9/24.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/random.hpp>
#include <dal/math/random/validation_internal.hpp>

namespace Dal::RandomBinding {
    Handle_<PseudoRSG_> NewPseudoRSG(const String_& name, double seed, double ndim) {
        return Dal::NewPseudoRSG(name, RandomInput::Seed(seed), RandomInput::PublicDimension(ndim));
    }

    Handle_<SobolRSG_> NewSobolRSG(const String_& name, double iPath, double ndim, bool precise, bool polish) {
        RandomInput::Integer(iPath, 0., static_cast<double>(std::numeric_limits<int>::max()) + 1., "/i_path", "finite integer in [0,INT_MAX]");
        return Dal::NewSobolRSG(name, static_cast<int>(iPath), RandomInput::SobolDimension(ndim), precise, polish);
    }
} // namespace Dal::RandomBinding
