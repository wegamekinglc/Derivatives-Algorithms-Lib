//
// Created by Codex on 2026-10-06.
//

#pragma once

#include <cmath>
#include <string>

#include <dal/math/aad/aad.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::AAD {
    [[nodiscard]] inline Number_ WeightedPayoffRoot(const Vector_<Number_>& outputs, const Vector_<double>& weights) {
        REQUIRE(!outputs.empty() && outputs.size() == weights.size(), "InvalidWeightedPayoff: nonempty outputs and matching weights required");
        for (size_t component = 0; component < outputs.size(); ++component) {
            REQUIRE(std::isfinite(weights[component]),
                    "InvalidWeightedPayoff: weight must be finite; component=" + String_(std::to_string(component)));
            REQUIRE(std::isfinite(Value(outputs[component])),
                    "InvalidWeightedPayoff: output must be finite; component=" + String_(std::to_string(component)));
        }
        Number_ root = outputs.front() * weights.front();
        for (size_t component = 1; component < outputs.size(); ++component)
            root = root + outputs[component] * weights[component];
        REQUIRE(std::isfinite(Value(root)), "InvalidWeightedPayoff: weighted sum must be finite");
        return root;
    }
} // namespace Dal::AAD
