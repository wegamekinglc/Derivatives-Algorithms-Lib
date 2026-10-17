//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <cstddef>

#include <dal/string/strings.hpp>

namespace Dal {
    class DupireCalibrationSnapshot_;
    struct HybridModelData_;

    namespace Detail {
        struct DupireSurfaceLayout_ {
            size_t offset_;
            int rows_;
            int columns_;
        };

        [[nodiscard]] DupireSurfaceLayout_
        DupireSurfaceLayout(const HybridModelData_& model, const DupireCalibrationSnapshot_& calibration, const String_& component);
        [[nodiscard]] size_t DupireRiskPayloadBytes(size_t surfaceNodes, size_t directBindings, size_t quoteRows, size_t quoteColumns);
    } // namespace Detail
} // namespace Dal
