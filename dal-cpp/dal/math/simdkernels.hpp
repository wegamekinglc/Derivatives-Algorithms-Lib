//
// Created by wegamekinglc on 2026/10/3.
//

#pragma once

#include <cstddef>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__AVX2__) || defined(__SSE2__)
#include <immintrin.h>
#endif

namespace Dal::Math {
    // Pointer into contiguous double storage; nullptr for empty ranges so kernels never dereference.
    template <class C_> const double* DoubleData(const C_& src) { return src.size() == 0 ? nullptr : &*src.begin(); }

    namespace Detail {
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        inline double HorizontalSum(__m128d v) { return _mm_cvtsd_f64(_mm_add_sd(v, _mm_unpackhi_pd(v, v))); }
#endif
#if defined(__AVX2__)
        inline double HorizontalSum(__m256d v) {
            const __m128d folded = _mm_add_pd(_mm256_castpd256_pd128(v), _mm256_extractf128_pd(v, 1));
            return HorizontalSum(folded);
        }
#endif

        inline double SumScalar(const double* src, size_t n) {
            double acc0 = 0.0, acc1 = 0.0, acc2 = 0.0, acc3 = 0.0;
            size_t ii = 0;
            for (; ii + 4 <= n; ii += 4) {
                acc0 += src[ii];
                acc1 += src[ii + 1];
                acc2 += src[ii + 2];
                acc3 += src[ii + 3];
            }
            double ret_val = (acc0 + acc1) + (acc2 + acc3);
            for (; ii < n; ++ii)
                ret_val += src[ii];
            return ret_val;
        }

        inline double DotScalar(const double* lhs, const double* rhs, size_t n) {
            double acc0 = 0.0, acc1 = 0.0, acc2 = 0.0, acc3 = 0.0;
            size_t ii = 0;
            for (; ii + 4 <= n; ii += 4) {
                acc0 += lhs[ii] * rhs[ii];
                acc1 += lhs[ii + 1] * rhs[ii + 1];
                acc2 += lhs[ii + 2] * rhs[ii + 2];
                acc3 += lhs[ii + 3] * rhs[ii + 3];
            }
            double ret_val = (acc0 + acc1) + (acc2 + acc3);
            for (; ii < n; ++ii)
                ret_val += lhs[ii] * rhs[ii];
            return ret_val;
        }

        inline void AxpyScalar(double scale, const double* src, double* dst, size_t n) {
            for (size_t ii = 0; ii < n; ++ii)
                dst[ii] += scale * src[ii];
        }

#if defined(__AVX2__)
        inline double SumAvx2(const double* src, size_t n) {
            __m256d acc0 = _mm256_setzero_pd();
            __m256d acc1 = _mm256_setzero_pd();
            size_t ii = 0;
            for (; ii + 8 <= n; ii += 8) {
                acc0 = _mm256_add_pd(acc0, _mm256_loadu_pd(src + ii));
                acc1 = _mm256_add_pd(acc1, _mm256_loadu_pd(src + ii + 4));
            }
            for (; ii + 4 <= n; ii += 4)
                acc0 = _mm256_add_pd(acc0, _mm256_loadu_pd(src + ii));
            double ret_val = HorizontalSum(acc0) + HorizontalSum(acc1);
            for (; ii < n; ++ii)
                ret_val += src[ii];
            return ret_val;
        }

        inline double DotAvx2(const double* lhs, const double* rhs, size_t n) {
            __m256d acc0 = _mm256_setzero_pd();
            __m256d acc1 = _mm256_setzero_pd();
            size_t ii = 0;
            for (; ii + 8 <= n; ii += 8) {
                acc0 = _mm256_add_pd(acc0, _mm256_mul_pd(_mm256_loadu_pd(lhs + ii), _mm256_loadu_pd(rhs + ii)));
                acc1 = _mm256_add_pd(acc1, _mm256_mul_pd(_mm256_loadu_pd(lhs + ii + 4), _mm256_loadu_pd(rhs + ii + 4)));
            }
            for (; ii + 4 <= n; ii += 4)
                acc0 = _mm256_add_pd(acc0, _mm256_mul_pd(_mm256_loadu_pd(lhs + ii), _mm256_loadu_pd(rhs + ii)));
            double ret_val = HorizontalSum(acc0) + HorizontalSum(acc1);
            for (; ii < n; ++ii)
                ret_val += lhs[ii] * rhs[ii];
            return ret_val;
        }

        inline void AxpyAvx2(double scale, const double* src, double* dst, size_t n) {
            const __m256d a = _mm256_set1_pd(scale);
            size_t ii = 0;
            for (; ii + 8 <= n; ii += 8) {
                _mm256_storeu_pd(dst + ii, _mm256_add_pd(_mm256_loadu_pd(dst + ii), _mm256_mul_pd(a, _mm256_loadu_pd(src + ii))));
                _mm256_storeu_pd(dst + ii + 4, _mm256_add_pd(_mm256_loadu_pd(dst + ii + 4), _mm256_mul_pd(a, _mm256_loadu_pd(src + ii + 4))));
            }
            for (; ii + 4 <= n; ii += 4)
                _mm256_storeu_pd(dst + ii, _mm256_add_pd(_mm256_loadu_pd(dst + ii), _mm256_mul_pd(a, _mm256_loadu_pd(src + ii))));
            for (; ii < n; ++ii)
                dst[ii] += scale * src[ii];
        }
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        inline double SumSse2(const double* src, size_t n) {
            __m128d acc0 = _mm_setzero_pd();
            __m128d acc1 = _mm_setzero_pd();
            __m128d acc2 = _mm_setzero_pd();
            __m128d acc3 = _mm_setzero_pd();
            size_t ii = 0;
            for (; ii + 8 <= n; ii += 8) {
                acc0 = _mm_add_pd(acc0, _mm_loadu_pd(src + ii));
                acc1 = _mm_add_pd(acc1, _mm_loadu_pd(src + ii + 2));
                acc2 = _mm_add_pd(acc2, _mm_loadu_pd(src + ii + 4));
                acc3 = _mm_add_pd(acc3, _mm_loadu_pd(src + ii + 6));
            }
            for (; ii + 2 <= n; ii += 2)
                acc0 = _mm_add_pd(acc0, _mm_loadu_pd(src + ii));
            double ret_val = (HorizontalSum(acc0) + HorizontalSum(acc1)) + (HorizontalSum(acc2) + HorizontalSum(acc3));
            for (; ii < n; ++ii)
                ret_val += src[ii];
            return ret_val;
        }

        inline double DotSse2(const double* lhs, const double* rhs, size_t n) {
            __m128d acc0 = _mm_setzero_pd();
            __m128d acc1 = _mm_setzero_pd();
            __m128d acc2 = _mm_setzero_pd();
            __m128d acc3 = _mm_setzero_pd();
            size_t ii = 0;
            for (; ii + 8 <= n; ii += 8) {
                acc0 = _mm_add_pd(acc0, _mm_mul_pd(_mm_loadu_pd(lhs + ii), _mm_loadu_pd(rhs + ii)));
                acc1 = _mm_add_pd(acc1, _mm_mul_pd(_mm_loadu_pd(lhs + ii + 2), _mm_loadu_pd(rhs + ii + 2)));
                acc2 = _mm_add_pd(acc2, _mm_mul_pd(_mm_loadu_pd(lhs + ii + 4), _mm_loadu_pd(rhs + ii + 4)));
                acc3 = _mm_add_pd(acc3, _mm_mul_pd(_mm_loadu_pd(lhs + ii + 6), _mm_loadu_pd(rhs + ii + 6)));
            }
            for (; ii + 2 <= n; ii += 2)
                acc0 = _mm_add_pd(acc0, _mm_mul_pd(_mm_loadu_pd(lhs + ii), _mm_loadu_pd(rhs + ii)));
            double ret_val = (HorizontalSum(acc0) + HorizontalSum(acc1)) + (HorizontalSum(acc2) + HorizontalSum(acc3));
            for (; ii < n; ++ii)
                ret_val += lhs[ii] * rhs[ii];
            return ret_val;
        }

        inline void AxpySse2(double scale, const double* src, double* dst, size_t n) {
            const __m128d a = _mm_set1_pd(scale);
            size_t ii = 0;
            for (; ii + 4 <= n; ii += 4) {
                _mm_storeu_pd(dst + ii, _mm_add_pd(_mm_loadu_pd(dst + ii), _mm_mul_pd(a, _mm_loadu_pd(src + ii))));
                _mm_storeu_pd(dst + ii + 2, _mm_add_pd(_mm_loadu_pd(dst + ii + 2), _mm_mul_pd(a, _mm_loadu_pd(src + ii + 2))));
            }
            for (; ii + 2 <= n; ii += 2)
                _mm_storeu_pd(dst + ii, _mm_add_pd(_mm_loadu_pd(dst + ii), _mm_mul_pd(a, _mm_loadu_pd(src + ii))));
            for (; ii < n; ++ii)
                dst[ii] += scale * src[ii];
        }
#endif
    } // namespace Detail

    // Fixed lane decomposition per build: partial sums are deterministic for a given binary,
    // but the low-order bits differ between SSE2 and AVX2 builds (see also -ffp-contract=fast).
    inline double Sum(const double* src, size_t n) {
        if (n == 0)
            return 0.0;
#if defined(__AVX2__)
        return Detail::SumAvx2(src, n);
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        return Detail::SumSse2(src, n);
#else
        return Detail::SumScalar(src, n);
#endif
    }

    inline double Dot(const double* lhs, const double* rhs, size_t n) {
        if (n == 0)
            return 0.0;
#if defined(__AVX2__)
        return Detail::DotAvx2(lhs, rhs, n);
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        return Detail::DotSse2(lhs, rhs, n);
#else
        return Detail::DotScalar(lhs, rhs, n);
#endif
    }

    // dst[ii] += scale * src[ii]; element-wise, so no lane-order concerns beyond -ffp-contract=fast
    inline void Axpy(double scale, const double* src, double* dst, size_t n) {
        if (n == 0)
            return;
#if defined(__AVX2__)
        Detail::AxpyAvx2(scale, src, dst, n);
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
        Detail::AxpySse2(scale, src, dst, n);
#else
        Detail::AxpyScalar(scale, src, dst, n);
#endif
    }
} // namespace Dal::Math
