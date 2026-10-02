//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsreuropean.hpp>
#include <dal/model/gsrslvdata.hpp>

namespace Dal {
    struct GSRMonteCarloSettings_ {
        int paths_ = 4096;
        int seed_ = 1729;
        int conditionalPaths_ = 64;
    };

    struct GSRMonteCarloPrice_ {
        double price_ = 0.0;
        double standardError_ = 0.0;
        double conditionalError_ = 0.0;
    };

    Vector_<GSRMonteCarloPrice_> PriceGSRSLVEuropeanOptions(const GSRSLVModelData_& model,
                                                            const Vector_<GSREuropeanOption_>& options,
                                                            const GSRMonteCarloSettings_& settings = {});
} // namespace Dal
