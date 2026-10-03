//
// Created by Codex on 2026/10/3.
//

#include <dal/math/simdkernels.hpp>

namespace Dal::TestSupport {
    double SimdSumSse2(const double* src, size_t n) { return Math::Sum(src, n); }
    double SimdDotSse2(const double* lhs, const double* rhs, size_t n) { return Math::Dot(lhs, rhs, n); }
} // namespace Dal::TestSupport
