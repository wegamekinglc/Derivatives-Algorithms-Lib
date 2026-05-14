//
// Created by wegam on 2026/5/9.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/optimization/underdeterminedutils.hpp>

namespace Dal::Underdetermined {

    void SelfCouplePWC(Sparse::Square_* weights,
                       const Vector_<DateTime_>& knots,
                       double tau_smoothing,
                       int offset) {
        REQUIRE(weights, "Weights matrix must not be null");
        REQUIRE(offset >= 0, "Weights offset must not be negative");
        REQUIRE(tau_smoothing > 0.0, "Smoothing weight must be positive");
        REQUIRE(weights->Size() >= offset + static_cast<int>(knots.size()), "Weights matrix is too small for the requested coupling");

        for (int i = 0; i < static_cast<int>(knots.size()); ++i) {
            double diag = tau_smoothing;
            if (i > 0)
                diag += tau_smoothing;
            if (i + 1 < static_cast<int>(knots.size()))
                diag += tau_smoothing;
            weights->Add(offset + i, offset + i, diag);
        }
        for (int i = 0; i + 1 < static_cast<int>(knots.size()); ++i) {
            weights->Add(offset + i, offset + i + 1, -tau_smoothing);
            weights->Add(offset + i + 1, offset + i, -tau_smoothing);
        }
    }

} // namespace Dal::Underdetermined
