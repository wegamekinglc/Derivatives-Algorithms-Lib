//
// Created by wegam on 2022/11/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
    namespace Detail {
        SimulationObserver_*& SimulationObserver() {
            static thread_local SimulationObserver_* observer = nullptr;
            return observer;
        }
    } // namespace Detail

    std::unique_ptr<Random_> CreateRNG(const String_& method, size_t nDim, bool useBb) {
        ValidateRNG(method);
        if (nDim == 0)
            return nullptr;
        std::unique_ptr<Random_> rsg;
        if (method == "sobol")
            rsg = NewSobol(static_cast<int>(nDim), 2048);
        else if (method == "mrg32")
            rsg = New(RNGType_("MRG32"), 1024, nDim);
        else if (method == "irn")
            rsg = New(RNGType_("IRN"), 1024, nDim);
        else
            THROW("rng method is not known");

        if (useBb)
            return std::make_unique<BrownianBridge_>(std::move(rsg));
        return rsg;
    }
} // namespace Dal::Script
