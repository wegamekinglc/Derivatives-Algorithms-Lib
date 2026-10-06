//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/model/dupirerisk.hpp>

namespace Dal::Test {
    inline DupireRiskInputs_ SmallRiskInputs() {
        return {{80.0, 100.0, 120.0}, {0.5, 1.0}, Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
    }
} // namespace Dal::Test
