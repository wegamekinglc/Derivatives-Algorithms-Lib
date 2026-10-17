#if defined(_MSC_VER)
#pragma fp_contract(off)
#endif

#include <dal/math/normalcentral.hpp>
#include <dal/math/simdkernels.hpp>
#include <dal/math/specialfunctions.hpp>

#if defined(_MSC_VER) && defined(DAL_NORMAL_AVX2)
#include <intrin.h>
#endif

#if !defined(DAL_DISABLE_SIMD_NORMAL) && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
#include <immintrin.h>
#define DAL_NORMAL_SSE2
#endif

namespace Dal::Math {
    namespace {
#if defined(DAL_NORMAL_SSE2)
        struct Sse2_ {
            using Vector_ = __m128d;
            static constexpr size_t WIDTH = 2;
            static Vector_ Load(const double* values) { return _mm_loadu_pd(values); }
            static void Store(double* values, Vector_ x) { _mm_storeu_pd(values, x); }
            static bool Central(Vector_ x) {
                return _mm_movemask_pd(_mm_and_pd(_mm_cmpge_pd(x, _mm_set1_pd(0.02425)), _mm_cmple_pd(x, _mm_set1_pd(1.0 - 0.02425)))) == 3;
            }
            static Vector_ Broadcast(double value) { return _mm_set1_pd(value); }
            static Vector_ MultiplyAdd(Vector_ a, Vector_ b, Vector_ c) {
#if defined(DAL_NORMAL_FMA)
                return _mm_fmadd_pd(a, b, c);
#else
                return _mm_add_pd(_mm_mul_pd(a, b), c);
#endif
            }
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
#if defined(DAL_NORMAL_FMA)
            if ((info[2] & 0x1000) == 0)
                return false;
#endif
            __cpuidex(info, 7, 0);
            return (info[1] & 0x20) != 0;
#else
#if defined(DAL_NORMAL_FMA)
            return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
            return __builtin_cpu_supports("avx2");
#endif
#endif
#else
            return false;
#endif
        }
#endif
    } // namespace

    void InverseNCDFBlock(double* values, size_t n, bool precise, bool polish) {
        if (!polish && n >= 32) {
#if defined(DAL_NORMAL_AVX2)
            static const bool USE_AVX2 = Avx2Available();
            if (USE_AVX2) {
#if defined(DAL_NORMAL_FMA)
                Detail::InverseNormalAvx2Fma(values, n);
#else
                Detail::InverseNormalAvx2(values, n);
#endif
                return;
            }
#endif
#if defined(DAL_NORMAL_SSE2)
            Detail::InverseNormalVector<Sse2_>(values, n);
            return;
#endif
        }
        for (size_t i = 0; i < n; ++i)
            values[i] = InverseNCDF(values[i], precise, polish);
    }
} // namespace Dal::Math
