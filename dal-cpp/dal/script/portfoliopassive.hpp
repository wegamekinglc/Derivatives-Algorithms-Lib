//
// Created by Codex on 2026/10/7.
//

#pragma once

#include <dal/script/simulation.hpp>

namespace Dal::Script::Detail {
    struct PortfolioPassivePathState_ {
        std::unique_ptr<Random_> random_;
        Vector_<double> gauss_;
        AAD::Scenario_<double> path_;
        std::unique_ptr<LocalCheckedPaths_> bsPaths_;

        PortfolioPassivePathState_(const PreparedScript_& trade, const AAD::Model_<double>& model)
            : random_(CreateRNG(trade.Simulation().rsg_, model, trade.Simulation().useBb_, std::nullopt, trade.Simulation().normalPrecision_)),
              gauss_(model.SimDim()) {
            if (typeid(model) == typeid(AAD::BlackScholes_<double>))
                bsPaths_ = std::make_unique<LocalCheckedPaths_>(static_cast<const AAD::BlackScholes_<double>&>(model));
            else {
                AllocatePath(trade.DefLine(), path_);
                InitializePath(path_);
            }
        }
    };
} // namespace Dal::Script::Detail
