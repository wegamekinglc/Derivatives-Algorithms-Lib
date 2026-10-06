//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/script/preparation.hpp>

namespace Dal::Script::Detail {
    struct PreparedPortfolioTradeView_ {
        const PreparedScript_* prepared_ = nullptr;
        size_t modelOwner_ = 0;
        size_t randomDimension_ = 0;
        size_t factors_ = 0;
        bool deterministicNumeraire_ = false;
        bool canShareScenario_ = false;
    };

    struct PortfolioScenarioGroup_ {
        size_t modelOwner_;
        Vector_<size_t> tradePositions_;
    };

    [[nodiscard]] bool SamePortfolioSampleDefinition(const AAD::SampleDef_& lhs, const AAD::SampleDef_& rhs);

    // Views must come from one sealed owner registry and one absolute path range.
    [[nodiscard]] Vector_<PortfolioScenarioGroup_> GroupPreparedPortfolio(const Vector_<PreparedPortfolioTradeView_>& trades);
} // namespace Dal::Script::Detail
