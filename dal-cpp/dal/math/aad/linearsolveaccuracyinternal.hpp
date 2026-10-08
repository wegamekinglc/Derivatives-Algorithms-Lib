//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <dal/math/aad/linearsolveaccuracy.hpp>

namespace Dal::AAD {
    [[nodiscard]] SolveAccuracyEvent_ NewSolveAccuracyEvent(std::uint64_t recording);
    [[nodiscard]] SolveAccuracyReport_* PrepareSolveAccuracyReport(const SolveAccuracyEvent_& event, int rhsColumns, bool multi, size_t width);
    void CopySolveAccuracyErrors(const Vector_<>& errors, SolveAccuracyReport_* report, size_t channel);
} // namespace Dal::AAD
