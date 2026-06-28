//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t nDim, bool useBb) {
        std::unique_ptr<Random_> rsg;
        if (method == "sobol")
            rsg = std::unique_ptr<Random_>(NewSobol(static_cast<int>(nDim), 2048));
        else if (method == "mrg32")
            rsg = std::unique_ptr<Random_>(New(RNGType_("MRG32"), 1024, nDim));
        else if (method == "irn")
            rsg = std::unique_ptr<Random_>(New(RNGType_("IRN"), 1024, nDim));
        else
            THROW("rng method is not known");

        if (useBb)
            return std::make_unique<BrownianBridge_>(std::move(rsg));
        return rsg;
    }
} // namespace Dal::Script
