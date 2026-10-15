//
// Created by Codex on 2026/10/16.
//

#if defined(DAL_NORMAL_AVX2)
#include <dal/math/normalcentral.hpp>
#include <dal/math/specialfunctions.hpp>
#include <immintrin.h>

namespace Dal::Math::Detail {
    namespace {
        struct Avx2_ {
            using Vector_ = __m256d;
            static Vector_ Broadcast(double value) { return _mm256_set1_pd(value); }
            static Vector_ Add(Vector_ a, Vector_ b) { return _mm256_add_pd(a, b); }
            static Vector_ Subtract(Vector_ a, Vector_ b) { return _mm256_sub_pd(a, b); }
            static Vector_ Multiply(Vector_ a, Vector_ b) { return _mm256_mul_pd(a, b); }
            static Vector_ Divide(Vector_ a, Vector_ b) { return _mm256_div_pd(a, b); }
        };
    } // namespace

    void InverseNormalAvx2(double* values, size_t n) {
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            const auto x = _mm256_loadu_pd(values + i);
            const auto central =
                _mm256_and_pd(_mm256_cmp_pd(x, _mm256_set1_pd(0.02425), _CMP_GE_OQ), _mm256_cmp_pd(x, _mm256_set1_pd(1.0 - 0.02425), _CMP_LE_OQ));
            if (_mm256_movemask_pd(central) == 15)
                _mm256_storeu_pd(values + i, CentralInverse<Avx2_>(x));
            else
                for (size_t lane = 0; lane < 4; ++lane)
                    values[i + lane] = InverseNCDF(values[i + lane], false, false);
        }
        for (; i < n; ++i)
            values[i] = InverseNCDF(values[i], false, false);
    }
} // namespace Dal::Math::Detail
#endif
