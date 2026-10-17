//
// Created by Codex on 2026/10/16.
//

#pragma once

#include <immintrin.h>

#include <dal/math/normalcentral.hpp>

namespace Dal::Math::Detail {
    template <bool FUSED_> struct Avx2_ {
        using Vector_ = __m256d;
        static constexpr size_t WIDTH = 4;
        static Vector_ Load(const double* values) { return _mm256_loadu_pd(values); }
        static void Store(double* values, Vector_ x) { _mm256_storeu_pd(values, x); }
        static bool Central(Vector_ x) {
            const auto central =
                _mm256_and_pd(_mm256_cmp_pd(x, _mm256_set1_pd(0.02425), _CMP_GE_OQ), _mm256_cmp_pd(x, _mm256_set1_pd(1.0 - 0.02425), _CMP_LE_OQ));
            return _mm256_movemask_pd(central) == 15;
        }
        static Vector_ Broadcast(double value) { return _mm256_set1_pd(value); }
        static Vector_ MultiplyAdd(Vector_ a, Vector_ b, Vector_ c) {
            if constexpr (FUSED_)
                return _mm256_fmadd_pd(a, b, c);
            else
                return _mm256_add_pd(_mm256_mul_pd(a, b), c);
        }
        static Vector_ Subtract(Vector_ a, Vector_ b) { return _mm256_sub_pd(a, b); }
        static Vector_ Multiply(Vector_ a, Vector_ b) { return _mm256_mul_pd(a, b); }
        static Vector_ Divide(Vector_ a, Vector_ b) { return _mm256_div_pd(a, b); }
    };
} // namespace Dal::Math::Detail
