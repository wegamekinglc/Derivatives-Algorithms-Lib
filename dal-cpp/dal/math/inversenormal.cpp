//
// Created by Codex on 2026/10/16.
//

#include <dal/math/normalcentral.hpp>
#include <dal/math/simdkernels.hpp>
#include <dal/math/specialfunctions.hpp>

#if defined(_MSC_VER) && defined(DAL_NORMAL_AVX2)
#include <intrin.h>
#endif

#if !defined(DAL_DISABLE_SIMD_NORMAL) && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
#include <emmintrin.h>
#define DAL_NORMAL_SSE2
#endif

namespace Dal::Math {
    namespace {
#if defined(DAL_NORMAL_SSE2)
        struct Sse2_ {
            using Vector_ = __m128d;
            static Vector_ Broadcast(double value) { return _mm_set1_pd(value); }
            static Vector_ Add(Vector_ a, Vector_ b) { return _mm_add_pd(a, b); }
            static Vector_ Subtract(Vector_ a, Vector_ b) { return _mm_sub_pd(a, b); }
            static Vector_ Multiply(Vector_ a, Vector_ b) { return _mm_mul_pd(a, b); }
            static Vector_ Divide(Vector_ a, Vector_ b) { return _mm_div_pd(a, b); }
        };
#endif
#if defined(DAL_NORMAL_AVX2)
        bool Avx2Available() {
#if defined(DAL_NORMAL_AVX2) && !defined(DAL_DISABLE_SIMD_NORMAL)
#if defined(_MSC_VER)
            int info[4];
            __cpuid(info, 1);
            if ((info[2] & 0x18000000) != 0x18000000 || (_xgetbv(0) & 6) != 6)
                return false;
            __cpuidex(info, 7, 0);
            return (info[1] & 0x20) != 0;
#else
            return __builtin_cpu_supports("avx2");
#endif
#else
            return false;
#endif
        }
#endif
    } // namespace

    void InverseNCDFBlock(double* values, size_t n, bool precise, bool polish) {
        size_t i = 0;
        if (!polish && n >= 32) {
#if defined(DAL_NORMAL_AVX2)
            static const bool USE_AVX2 = Avx2Available();
            if (USE_AVX2) {
                Detail::InverseNormalAvx2(values, n);
                return;
            }
#endif
#if defined(DAL_NORMAL_SSE2)
            for (; i + 2 <= n; i += 2) {
                const auto x = _mm_loadu_pd(values + i);
                const auto central = _mm_and_pd(_mm_cmpge_pd(x, _mm_set1_pd(0.02425)), _mm_cmple_pd(x, _mm_set1_pd(1.0 - 0.02425)));
                if (_mm_movemask_pd(central) == 3)
                    _mm_storeu_pd(values + i, Detail::CentralInverse<Sse2_>(x));
                else
                    for (size_t lane = 0; lane < 2; ++lane)
                        values[i + lane] = InverseNCDF(values[i + lane], precise, polish);
            }
#endif
        }
        for (; i < n; ++i)
            values[i] = InverseNCDF(values[i], precise, polish);
    }
} // namespace Dal::Math
