//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <cmath>

#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>

namespace Dal::RateQuoteRiskInternal {
    template <class C_>
    void ForEachQuoteAdjoint(const RateQuoteRiskProvenance_& provenance,
                             const Vector_<>& gradient,
                             C_ consume,
                             const char* nonFiniteReason = "QUOTE_RISK_NON_FINITE_OUTPUT") {
        Vector_<> sensitivities;
        Matrix::Multiply(gradient, provenance.EffectiveInverse(), &sensitivities);
        REQUIRE(sensitivities.size() == provenance.Axis().quotes_.size(), "QUOTE_RISK_TRANSFORM_WIDTH_MISMATCH");
        for (int i = 0; i < static_cast<int>(sensitivities.size()); ++i) {
            const double sensitivity = sensitivities[i] / provenance.Tolerance();
            const double dv01 = sensitivity * 1.0e-4;
            REQUIRE(std::isfinite(sensitivity) && std::isfinite(dv01), nonFiniteReason);
            consume(i, sensitivity, dv01);
        }
    }
} // namespace Dal::RateQuoteRiskInternal
