//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <optional>

#include <dal/curve/ratestructuraljacobian.hpp>

namespace Dal {
    struct RateJacobianExecutionSettings_ {
        bool vectorAdjoints_ = false;
        size_t adjointWidth_ = 1;
        std::optional<size_t> numericPayloadBudgetBytes_;
    };

    struct RateTradeParameterJacobianResult_ {
        Vector_<RatePricingTradeResult_> prices_;
        Matrix_<> jacobian_;
        Vector_<RateCurveParameterCoordinate_> inputAxis_;
        Vector_<String_> outputAxis_;
        size_t reverseDirections_ = 0;
        size_t reverseSweeps_ = 0;
    };

    [[nodiscard]] RateTradeParameterJacobianResult_ RateTradeParameterJacobian(const Vector_<RateTradeDefinition_>& trades,
                                                                               const RatePricingMarket_& market,
                                                                               const Vector_<RateCurveParameterCoordinate_>& inputAxis,
                                                                               const RateJacobianExecutionSettings_& settings = {});
    [[nodiscard]] RateTradeParameterJacobianResult_ ExecuteRateStructuralJacobian(const Vector_<RateTradeDefinition_>& trades,
                                                                                  const RatePricingMarket_& market,
                                                                                  const RateStructuralJacobianPlan_& plan,
                                                                                  const RateJacobianExecutionSettings_& settings = {});
} // namespace Dal
