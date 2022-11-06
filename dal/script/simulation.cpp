//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t n_dim, bool use_bb) {
        std::unique_ptr<Random_> rsg;
        if (method == "sobol")
            rsg = std::unique_ptr<Random_>(NewSobol(n_dim, 2048));
        else if (method == "mrg32")
            rsg = std::unique_ptr<Random_>(New(RNGType_("MRG32"), 1024, n_dim));
        else if (method == "irn")
            rsg = std::unique_ptr<Random_>(New(RNGType_("IRN"), 1024, n_dim));
        else
            THROW("rng method is not known");

        if (use_bb)
            return std::make_unique<BrownianBridge_>(std::move(rsg));
        else
            return rsg;
    }
}
