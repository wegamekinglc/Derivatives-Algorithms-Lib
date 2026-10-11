//
// Created by Codex on 2026/10/12.
//

#pragma once

#include <cstddef>

#include <dal/math/aad/expr.hpp>

namespace Dal::AAD {
    template <size_t N_, class T_, class F_> [[nodiscard]] auto Accumulate(const T_& initial, const F_& term) {
        if constexpr (N_ == 0)
            return initial;
        else {
            auto prefix = Accumulate<N_ - 1>(initial, term);
            return prefix + term(N_ - 1);
        }
    }
} // namespace Dal::AAD
