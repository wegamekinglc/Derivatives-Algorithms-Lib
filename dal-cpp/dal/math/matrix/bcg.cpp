//
// Created by wegamekinglc on 22-12-17.
//

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#if defined(__SSE2__) || defined(_M_X64)
#include <immintrin.h>
#endif
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/math/matrix/bcg_scaled_alpha.inc>

#if defined(DAL35_PROBE_ORDINARY_WORKSPACE_CONSTRUCTION) && !defined(DAL35_ENABLE_TEST_SEAM)
#error "DAL35_PROBE_ORDINARY_WORKSPACE_CONSTRUCTION requires DAL35_ENABLE_TEST_SEAM"
#endif

#if defined(DAL35_ENABLE_TEST_SEAM)
#if defined(__GNUC__) || defined(__clang__)
#define DAL35_TEST_HIDDEN_ __attribute__((visibility("hidden")))
#else
#define DAL35_TEST_HIDDEN_
#endif
extern "C" DAL35_TEST_HIDDEN_ void Dal35ObserveExactWorkspaceConstructionForTest_() noexcept;
#undef DAL35_TEST_HIDDEN_
#endif

namespace Dal {
    namespace {
        struct XPrecondition_ {
            const HasPreConditioner_* a_;
            explicit XPrecondition_(const Sparse::Square_& a) : a_(dynamic_cast<const HasPreConditioner_*>(&a)) {}
            bool Left(const Vector_<>& b, Vector_<>* x) const {
                if (a_) {
                    a_->PreConditionerSolveLeft(b, x);
                    return true;
                }
                return false;
            }
            bool Right(const Vector_<>& b, Vector_<>* x) const {
                if (a_) {
                    a_->PreConditionerSolveRight(b, x);
                    return true;
                }
                return false;
            }
            [[nodiscard]] bool IsIdentity() const { return a_ == nullptr; }
        };

        struct XSparseTransposed_ : public Sparse::Square_, public HasPreConditioner_ {
            const Sparse::Square_& a_;
            XPrecondition_ p_;
            explicit XSparseTransposed_(const Sparse::Square_& a) : a_(a), p_(a) {}

            [[nodiscard]] int Size() const override { return a_.Size(); }
            void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const { a_.MultiplyRight(x, b); }
            void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const { THROW("Unreachable: left-solve after transpose"); }
            void PreConditionerSolveLeft(const Vector_<>& x, Vector_<>* b) const override { p_.Right(x, b); }
        };
    } // namespace

    namespace {
        struct Scaled_ {
            double mantissa_;
            int exponent_;
            bool normalized_;
        };

        Scaled_ NormalizeScaled(double mantissa, int exponent) {
            if (mantissa == 0.0)
                return {0.0, 0, false};
            int adjustment = 0;
            const double normalized = std::frexp(mantissa, &adjustment);
            return {normalized, exponent + adjustment, true};
        }

        Scaled_ ScaledFromDouble(double value) { return {value, 0, false}; }

        Scaled_ ScaledProduct(double lhs, double rhs) {
            if (lhs == 0.0 || rhs == 0.0)
                return {0.0, 0, false};
            int lhsExponent = 0;
            int rhsExponent = 0;
            const double lhsMantissa = std::frexp(lhs, &lhsExponent);
            const double rhsMantissa = std::frexp(rhs, &rhsExponent);
            return NormalizeScaled(lhsMantissa * rhsMantissa, lhsExponent + rhsExponent);
        }

        Scaled_ MultiplyScaled(const Scaled_& value, double multiplier) {
            if (value.mantissa_ == 0.0 || multiplier == 0.0)
                return {0.0, 0, false};
            if (!value.normalized_) {
                const double product = value.mantissa_ * multiplier;
                if (std::isfinite(product) && product != 0.0)
                    return ScaledFromDouble(product);
            }
            int multiplierExponent = 0;
            const double multiplierMantissa = std::frexp(multiplier, &multiplierExponent);
            const Scaled_ normalizedValue = value.normalized_ ? value : NormalizeScaled(value.mantissa_, 0);
            return NormalizeScaled(normalizedValue.mantissa_ * multiplierMantissa, normalizedValue.exponent_ + multiplierExponent);
        }

        Scaled_ NormalizeScaledIfNeeded(const Scaled_& value) { return value.normalized_ ? value : NormalizeScaled(value.mantissa_, 0); }

        bool TryAddUnscaled(const Scaled_& lhs, const Scaled_& rhs, Scaled_* result) {
            if (lhs.normalized_ || rhs.normalized_)
                return false;
            const double sum = lhs.mantissa_ + rhs.mantissa_;
            if (!std::isfinite(sum) || sum == 0.0)
                return false;
            *result = ScaledFromDouble(sum);
            return true;
        }

        Scaled_ AddScaled(Scaled_ lhs, Scaled_ rhs) {
            if (lhs.mantissa_ == 0.0)
                return rhs;
            if (rhs.mantissa_ == 0.0)
                return lhs;
            Scaled_ unscaledSum = {0.0, 0, false};
            if (TryAddUnscaled(lhs, rhs, &unscaledSum))
                return unscaledSum;
            lhs = NormalizeScaledIfNeeded(lhs);
            rhs = NormalizeScaledIfNeeded(rhs);
            if (lhs.exponent_ < rhs.exponent_)
                std::swap(lhs, rhs);
            return NormalizeScaled(lhs.mantissa_ + std::ldexp(rhs.mantissa_, rhs.exponent_ - lhs.exponent_), lhs.exponent_);
        }

        Scaled_ SlowScaledNorm(const Vector_<>& values) {
            double scale = 0.0;
            double sumSquares = 1.0;
            for (const double value : values) {
                const double magnitude = std::fabs(value);
                if (magnitude == 0.0)
                    continue;
                if (scale < magnitude) {
                    const double ratio = scale / magnitude;
                    sumSquares = 1.0 + sumSquares * ratio * ratio;
                    scale = magnitude;
                } else {
                    const double ratio = magnitude / scale;
                    sumSquares += ratio * ratio;
                }
            }
            return scale == 0.0 ? Scaled_{0.0, 0, false} : MultiplyScaled(ScaledFromDouble(scale), std::sqrt(sumSquares));
        }

        Scaled_ ScaledNorm(const Vector_<>& values) {
            const double sumSquares = InnerProduct(values, values);
            return std::isfinite(sumSquares) && sumSquares >= std::numeric_limits<double>::min() ? ScaledFromDouble(std::sqrt(sumSquares))
                                                                                                 : SlowScaledNorm(values);
        }

        bool ScaledLessOrEqual(Scaled_ lhs, Scaled_ rhs) {
            lhs.mantissa_ = std::fabs(lhs.mantissa_);
            rhs.mantissa_ = std::fabs(rhs.mantissa_);
            if (lhs.mantissa_ == 0.0)
                return true;
            if (rhs.mantissa_ == 0.0)
                return false;
            if (!lhs.normalized_ && !rhs.normalized_)
                return lhs.mantissa_ <= rhs.mantissa_;
            if (!lhs.normalized_)
                lhs = NormalizeScaled(lhs.mantissa_, 0);
            if (!rhs.normalized_)
                rhs = NormalizeScaled(rhs.mantissa_, 0);
            if (lhs.exponent_ != rhs.exponent_)
                return lhs.exponent_ < rhs.exponent_;
            return lhs.mantissa_ <= rhs.mantissa_;
        }

        constexpr int EXACT_MIN_EXPONENT = -8608;
        constexpr int EXACT_MAX_EXPONENT = 8352;
        constexpr int EXACT_LIMB_BITS = 32;
        constexpr int EXACT_LIMB_COUNT = (EXACT_MAX_EXPONENT - EXACT_MIN_EXPONENT) / EXACT_LIMB_BITS;

#if defined(DAL35_PROBE_PRODUCTION_MAX_EXPONENT_DELTA)
        constexpr int DAL35_PRODUCTION_MAX_EXPONENT_DELTA_ = DAL35_PROBE_PRODUCTION_MAX_EXPONENT_DELTA;
#else
        constexpr int DAL35_PRODUCTION_MAX_EXPONENT_DELTA_ = 0;
#endif

        constexpr BcgScaledAlphaPrivate_::AccumulatorBounds_ ACTUAL_ACCUMULATOR_BOUNDS_{
            EXACT_MIN_EXPONENT, EXACT_MAX_EXPONENT + DAL35_PRODUCTION_MAX_EXPONENT_DELTA_, EXACT_LIMB_BITS, EXACT_LIMB_COUNT, 53, 1};

        static_assert(BcgScaledAlphaPrivate_::REVIEWED_BOUNDS_FINGERPRINT_MATCHES_, "DAL35_PRODUCTION_REVIEWED_BOUNDS_FINGERPRINT_MISMATCH");
        static_assert(BcgScaledAlphaPrivate_::SameAccumulatorBounds_(ACTUAL_ACCUMULATOR_BOUNDS_,
                                                                     BcgScaledAlphaPrivate_::REVIEWED_ACCUMULATOR_BOUNDS_),
                      "DAL35_PRODUCTION_ACCUMULATOR_INPUT_FINGERPRINT_MISMATCH");
        static_assert(BcgScaledAlphaPrivate_::SameBoundsFingerprint_(
                          BcgScaledAlphaPrivate_::MakeBoundsFingerprint_(ACTUAL_ACCUMULATOR_BOUNDS_,
                                                                         BcgScaledAlphaPrivate_::DeriveCandidateBounds_(ACTUAL_ACCUMULATOR_BOUNDS_)),
                          BcgScaledAlphaPrivate_::REVIEWED_BOUNDS_FINGERPRINT_),
                      "DAL35_PRODUCTION_DERIVED_BOUNDS_FINGERPRINT_MISMATCH");

        struct ExactPositive_ {
            std::array<std::uint32_t, EXACT_LIMB_COUNT> limbs_{};
            int first_ = EXACT_LIMB_COUNT;
            int last_ = -1;
        };

        void ExactAddAt(int index, std::uint32_t value, ExactPositive_* result) {
            std::uint64_t carry = value;
            while (carry != 0) {
                const std::uint64_t sum = static_cast<std::uint64_t>(result->limbs_[index]) + carry;
                result->limbs_[index] = static_cast<std::uint32_t>(sum);
                result->first_ = std::min(result->first_, index);
                result->last_ = std::max(result->last_, index);
                carry = sum >> EXACT_LIMB_BITS;
                ++index;
            }
        }

        void ExactAddShiftedLimb(std::uint32_t value, int exponent, ExactPositive_* result) {
            if (value == 0)
                return;
            const int offset = exponent - EXACT_MIN_EXPONENT;
            const int index = offset / EXACT_LIMB_BITS;
            const int shift = offset % EXACT_LIMB_BITS;
            const std::uint64_t shifted = static_cast<std::uint64_t>(value) << shift;
            ExactAddAt(index, static_cast<std::uint32_t>(shifted), result);
            ExactAddAt(index + 1, static_cast<std::uint32_t>(shifted >> EXACT_LIMB_BITS), result);
        }

        void ExactAddShiftedWord(std::uint64_t value, int exponent, ExactPositive_* result) {
            ExactAddShiftedLimb(static_cast<std::uint32_t>(value), exponent, result);
            ExactAddShiftedLimb(static_cast<std::uint32_t>(value >> EXACT_LIMB_BITS), exponent + EXACT_LIMB_BITS, result);
        }

        void ExactAddDoubleProduct(double lhs, double rhs, ExactPositive_* result) {
            if (lhs == 0.0 || rhs == 0.0)
                return;
            int lhsExponent = 0;
            int rhsExponent = 0;
            const std::uint64_t lhsSignificand = static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(lhs), &lhsExponent), 53));
            const std::uint64_t rhsSignificand = static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(rhs), &rhsExponent), 53));
            const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhsSignificand);
            const std::uint64_t lhsHigh = lhsSignificand >> EXACT_LIMB_BITS;
            const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhsSignificand);
            const std::uint64_t rhsHigh = rhsSignificand >> EXACT_LIMB_BITS;
            const int exponent = lhsExponent + rhsExponent - 106;
            ExactAddShiftedWord(lhsLow * rhsLow, exponent, result);
            ExactAddShiftedWord(lhsLow * rhsHigh + lhsHigh * rhsLow, exponent + EXACT_LIMB_BITS, result);
            ExactAddShiftedWord(lhsHigh * rhsHigh, exponent + 2 * EXACT_LIMB_BITS, result);
        }

        ExactPositive_ ExactNormSquare(const Vector_<>& values) {
            ExactPositive_ result;
            for (const double value : values)
                ExactAddDoubleProduct(value, value, &result);
            return result;
        }

        void ExactTrim(ExactPositive_* value) {
            while (value->first_ <= value->last_ && value->limbs_[value->first_] == 0)
                ++value->first_;
            while (value->last_ >= value->first_ && value->limbs_[value->last_] == 0)
                --value->last_;
            if (value->last_ < value->first_) {
                value->first_ = EXACT_LIMB_COUNT;
                value->last_ = -1;
            }
        }

        int ExactCompare(const ExactPositive_& lhs, const ExactPositive_& rhs) {
            if (lhs.last_ != rhs.last_)
                return lhs.last_ < rhs.last_ ? -1 : 1;
            for (int index = lhs.last_; index >= std::min(lhs.first_, rhs.first_); --index) {
                if (lhs.limbs_[index] != rhs.limbs_[index])
                    return lhs.limbs_[index] < rhs.limbs_[index] ? -1 : 1;
            }
            return 0;
        }

        ExactPositive_ ExactAdd(ExactPositive_ lhs, const ExactPositive_& rhs) {
            for (int index = rhs.first_; index <= rhs.last_; ++index)
                ExactAddAt(index, rhs.limbs_[index], &lhs);
            return lhs;
        }

        ExactPositive_ ExactSubtract(const ExactPositive_& lhs, const ExactPositive_& rhs) {
            ExactPositive_ result;
            std::uint64_t borrow = 0;
            for (int index = 0; index < EXACT_LIMB_COUNT; ++index) {
                const std::uint64_t subtrahend = static_cast<std::uint64_t>(rhs.limbs_[index]) + borrow;
                const std::uint64_t minuend = lhs.limbs_[index];
                if (minuend < subtrahend) {
                    result.limbs_[index] = static_cast<std::uint32_t>((1ULL << EXACT_LIMB_BITS) + minuend - subtrahend);
                    borrow = 1;
                } else {
                    result.limbs_[index] = static_cast<std::uint32_t>(minuend - subtrahend);
                    borrow = 0;
                }
            }
            result.first_ = 0;
            result.last_ = EXACT_LIMB_COUNT - 1;
            ExactTrim(&result);
            return result;
        }

        bool ExactBit(const ExactPositive_& value, int bit) { return (value.limbs_[bit / EXACT_LIMB_BITS] & (1U << (bit % EXACT_LIMB_BITS))) != 0; }

        bool ExactHasBitBelow(const ExactPositive_& value, int bit) {
            const int lastFullLimb = bit / EXACT_LIMB_BITS;
            for (int index = value.first_; index < lastFullLimb; ++index)
                if (value.limbs_[index] != 0)
                    return true;
            const int partialBits = bit % EXACT_LIMB_BITS;
            if (lastFullLimb < value.first_ || partialBits == 0)
                return false;
            const std::uint32_t mask = (1U << partialBits) - 1U;
            return (value.limbs_[lastFullLimb] & mask) != 0;
        }

        int ExactHighestBit(const ExactPositive_& value) {
            int highestLimbBit = 0;
            for (std::uint32_t top = value.limbs_[value.last_]; top > 1; top >>= 1)
                ++highestLimbBit;
            return EXACT_LIMB_BITS * value.last_ + highestLimbBit;
        }

        std::uint64_t ExactLeadingSignificand(const ExactPositive_& value, int highestBit) {
            std::uint64_t significand = 0;
            for (int bit = highestBit; bit > highestBit - 53; --bit)
                significand = (significand << 1U) | static_cast<std::uint64_t>(ExactBit(value, bit));
            return significand;
        }

        void RoundExactSignificand(const ExactPositive_& value, int highestBit, std::uint64_t* significand, int* exponent) {
            const int guardBit = highestBit - 53;
            if (ExactBit(value, guardBit) && (ExactHasBitBelow(value, guardBit) || (*significand & 1U) != 0))
                ++*significand;
            if (*significand == (1ULL << 53)) {
                *significand >>= 1U;
                ++*exponent;
            }
        }

        Scaled_ ScaledFromExact(const ExactPositive_& value, bool negative) {
            if (value.last_ < value.first_)
                return {0.0, 0, false};
            const int highestBit = ExactHighestBit(value);
            std::uint64_t significand = ExactLeadingSignificand(value, highestBit);
            int exponent = EXACT_MIN_EXPONENT + highestBit + 1;
            RoundExactSignificand(value, highestBit, &significand, &exponent);
            const double mantissa = std::ldexp(static_cast<double>(significand), -53);
            return {negative ? -mantissa : mantissa, exponent, true};
        }

        Scaled_ SlowScaledDot(const Vector_<>& lhs, const Vector_<>& rhs) {
            ExactPositive_ positive;
            ExactPositive_ negative;
            for (int i = 0; i < static_cast<int>(lhs.size()); ++i) {
                ExactPositive_* destination = std::signbit(lhs[i]) == std::signbit(rhs[i]) ? &positive : &negative;
                ExactAddDoubleProduct(lhs[i], rhs[i], destination);
            }
            const int comparison = ExactCompare(positive, negative);
            if (comparison == 0)
                return {0.0, 0, false};
            return comparison > 0 ? ScaledFromExact(ExactSubtract(positive, negative), false)
                                  : ScaledFromExact(ExactSubtract(negative, positive), true);
        }

        struct FastDotAccumulation_ {
            double sum_ = 0.0;
            double absoluteSum_ = 0.0;
            bool allProductsNormal_ = true;
            int firstScalar_ = 0;
        };

        FastDotAccumulation_ AccumulateFastDotPrefix(const Vector_<>& lhs, const Vector_<>& rhs) {
            FastDotAccumulation_ result;
#if defined(__AVX2__)
            const __m256d zero = _mm256_setzero_pd();
            const __m256d signMask = _mm256_set1_pd(-0.0);
            const __m256d minNormal = _mm256_set1_pd(std::numeric_limits<double>::min());
            const __m256d maxFinite = _mm256_set1_pd(std::numeric_limits<double>::max());
            __m256d sums = zero;
            __m256d absoluteSums = zero;
            int invalidProductMask = 0;
            for (; result.firstScalar_ + 3 < static_cast<int>(lhs.size()); result.firstScalar_ += 4) {
                const __m256d left = _mm256_loadu_pd(&lhs[result.firstScalar_]);
                const __m256d right = _mm256_loadu_pd(&rhs[result.firstScalar_]);
                const __m256d products = _mm256_mul_pd(left, right);
                const __m256d magnitudes = _mm256_andnot_pd(signMask, products);
                const __m256d bothNonzero = _mm256_and_pd(_mm256_cmp_pd(left, zero, _CMP_NEQ_OQ), _mm256_cmp_pd(right, zero, _CMP_NEQ_OQ));
                const __m256d normal =
                    _mm256_and_pd(_mm256_cmp_pd(magnitudes, minNormal, _CMP_GE_OQ), _mm256_cmp_pd(magnitudes, maxFinite, _CMP_LE_OQ));
                invalidProductMask |= _mm256_movemask_pd(_mm256_andnot_pd(normal, bothNonzero));
                sums = _mm256_add_pd(sums, products);
                absoluteSums = _mm256_add_pd(absoluteSums, magnitudes);
            }
            alignas(32) double sumLanes[4];
            alignas(32) double absoluteLanes[4];
            _mm256_store_pd(sumLanes, sums);
            _mm256_store_pd(absoluteLanes, absoluteSums);
            for (int lane = 0; lane < 4; ++lane) {
                result.sum_ += sumLanes[lane];
                result.absoluteSum_ += absoluteLanes[lane];
            }
            result.allProductsNormal_ = invalidProductMask == 0;
#endif
            return result;
        }

        void AccumulateFastDotTail(const Vector_<>& lhs, const Vector_<>& rhs, FastDotAccumulation_* result) {
            for (int i = result->firstScalar_; i < static_cast<int>(lhs.size()); ++i) {
                const double product = lhs[i] * rhs[i];
                const double magnitude = std::fabs(product);
                if (lhs[i] != 0.0 && rhs[i] != 0.0) {
                    if (!std::isfinite(product) || magnitude < std::numeric_limits<double>::min())
                        result->allProductsNormal_ = false;
                }
                result->sum_ += product;
                result->absoluteSum_ += magnitude;
            }
        }

        bool FastDotIsReliable(const FastDotAccumulation_& value, int size) {
            const double uncertainty = 4.0 * static_cast<double>(size + 4) * std::numeric_limits<double>::epsilon();
            if (!value.allProductsNormal_)
                return false;
            if (!std::isfinite(value.sum_))
                return false;
            if (value.sum_ == 0.0)
                return false;
            if (!std::isfinite(value.absoluteSum_))
                return false;
            if (uncertainty >= 0.5)
                return false;
            return std::fabs(value.sum_) > uncertainty * value.absoluteSum_;
        }

        bool FastScaledDot(const Vector_<>& lhs, const Vector_<>& rhs, Scaled_* result) {
            FastDotAccumulation_ accumulation = AccumulateFastDotPrefix(lhs, rhs);
            AccumulateFastDotTail(lhs, rhs, &accumulation);
            if (!FastDotIsReliable(accumulation, static_cast<int>(lhs.size())))
                return false;
            *result = ScaledFromDouble(accumulation.sum_);
            return true;
        }

        Scaled_ ScaledDot(const Vector_<>& lhs, const Vector_<>& rhs) {
            Scaled_ result = {0.0, 0, false};
            return FastScaledDot(lhs, rhs, &result) ? result : SlowScaledDot(lhs, rhs);
        }

        ExactPositive_ ExactMultiply(const ExactPositive_& lhs, const ExactPositive_& rhs) {
            ExactPositive_ result;
            for (int lhsIndex = lhs.first_; lhsIndex <= lhs.last_; ++lhsIndex) {
                if (lhs.limbs_[lhsIndex] == 0)
                    continue;
                for (int rhsIndex = rhs.first_; rhsIndex <= rhs.last_; ++rhsIndex) {
                    if (rhs.limbs_[rhsIndex] == 0)
                        continue;
                    const std::uint64_t product = static_cast<std::uint64_t>(lhs.limbs_[lhsIndex]) * static_cast<std::uint64_t>(rhs.limbs_[rhsIndex]);
                    const int exponent = 2 * EXACT_MIN_EXPONENT + EXACT_LIMB_BITS * (lhsIndex + rhsIndex);
                    ExactAddShiftedWord(product, exponent, &result);
                }
            }
            return result;
        }

        ExactPositive_ ExactShift(const ExactPositive_& value, int shift) {
            ExactPositive_ result;
            for (int index = value.first_; index <= value.last_; ++index)
                ExactAddShiftedLimb(value.limbs_[index], EXACT_MIN_EXPONENT + EXACT_LIMB_BITS * index + shift, &result);
            return result;
        }

        struct Convergence_ {
            const Vector_<>& b_;
            double tolRel_;
            double tolAbs_;
            double uncertainty_;
            Scaled_ certainThreshold_;
            bool certainThresholdIsStrict_;
            Scaled_ uncertainThreshold_;

            Convergence_(const Vector_<>& b, double tolRel, double tolAbs, const Scaled_& rhsNorm)
                : b_(b), tolRel_(tolRel), tolAbs_(tolAbs),
                  uncertainty_(std::min(0.25, 8.0 * static_cast<double>(b.size() + 2) * std::numeric_limits<double>::epsilon())) {
                const Scaled_ threshold = AddScaled(MultiplyScaled(rhsNorm, tolRel), ScaledFromDouble(tolAbs));
                certainThreshold_ = MultiplyScaled(threshold, (1.0 - uncertainty_) / (1.0 + uncertainty_));
                certainThresholdIsStrict_ = !ScaledLessOrEqual(threshold, certainThreshold_);
                uncertainThreshold_ = MultiplyScaled(threshold, (1.0 + uncertainty_) / (1.0 - uncertainty_));
            }

            bool ExactConverged(const Vector_<>& residual) const {
                const ExactPositive_ residualSquare = ExactNormSquare(residual);
                const ExactPositive_ rhsSquare = ExactNormSquare(b_);
                ExactPositive_ relativeMultiplierSquare;
                ExactAddDoubleProduct(tolRel_, tolRel_, &relativeMultiplierSquare);
                const ExactPositive_ relativeSquare = ExactMultiply(rhsSquare, relativeMultiplierSquare);
                if (ExactCompare(residualSquare, relativeSquare) <= 0)
                    return true;

                ExactPositive_ absoluteSquare;
                ExactAddDoubleProduct(tolAbs_, tolAbs_, &absoluteSquare);
                const ExactPositive_ baseToleranceSquare = ExactAdd(relativeSquare, absoluteSquare);
                if (ExactCompare(residualSquare, baseToleranceSquare) <= 0)
                    return true;

                const ExactPositive_ difference = ExactSubtract(residualSquare, baseToleranceSquare);
                const ExactPositive_ crossTermSquare = ExactShift(ExactMultiply(absoluteSquare, relativeSquare), 2);
                return ExactCompare(ExactMultiply(difference, difference), crossTermSquare) <= 0;
            }

            bool IsConverged(const Vector_<>& residual, const Scaled_& residualNorm) const {
                if (residualNorm.mantissa_ == 0.0)
                    return true;
                if (!ScaledLessOrEqual(residualNorm, uncertainThreshold_))
                    return false;
                if (certainThresholdIsStrict_ && ScaledLessOrEqual(residualNorm, certainThreshold_))
                    return true;
                return ExactConverged(residual);
            }

            bool IsConverged(const Vector_<>& residual) const { return IsConverged(residual, ScaledNorm(residual)); }
        };

        [[noreturn]] void ThrowFailure(const char* solver, const char* category, const char* subject) {
            THROW(std::string(solver) + ": " + category + ": " + subject);
        }

        [[noreturn]] void ThrowFailureAtIndex(const char* solver, const char* category, const char* subject, int index) {
            THROW(std::string(solver) + ": " + category + ": " + subject + " at index " + std::to_string(index));
        }

        void ValidateFiniteInput(const Vector_<>& values, const char* solver, const char* subject) {
            for (int i = 0; i < static_cast<int>(values.size()); ++i)
                if (!std::isfinite(values[i]))
                    ThrowFailureAtIndex(solver, "non-finite input", subject, i);
        }

        bool HasOnlyFiniteValuesAvx2(const Vector_<>& values) {
#if defined(__AVX2__)
            const __m256d signMask = _mm256_set1_pd(-0.0);
            const __m256d maxFinite = _mm256_set1_pd(std::numeric_limits<double>::max());
            int nonFiniteMask = 0;
            int i = 0;
            for (; i + 3 < static_cast<int>(values.size()); i += 4) {
                const __m256d items = _mm256_loadu_pd(&values[i]);
                const __m256d magnitudes = _mm256_andnot_pd(signMask, items);
                nonFiniteMask |= _mm256_movemask_pd(_mm256_cmp_pd(magnitudes, maxFinite, _CMP_NLE_UQ));
            }
            for (; i < static_cast<int>(values.size()); ++i)
                nonFiniteMask |= static_cast<int>(!std::isfinite(values[i]));
            return nonFiniteMask == 0;
#else
            (void)values;
            return true;
#endif
        }

        bool HasOnlyFiniteValuesSse2(const Vector_<>& values) {
#if defined(__SSE2__) || defined(_M_X64)
            const __m128d signMask = _mm_set1_pd(-0.0);
            const __m128d maxFinite = _mm_set1_pd(std::numeric_limits<double>::max());
            int i = 0;
            for (; i + 1 < static_cast<int>(values.size()); i += 2) {
                const __m128d pair = _mm_loadu_pd(&values[i]);
                const __m128d magnitudes = _mm_andnot_pd(signMask, pair);
                if (_mm_movemask_pd(_mm_cmple_pd(magnitudes, maxFinite)) != 3)
                    return false;
            }
            return i == static_cast<int>(values.size()) || std::isfinite(values[i]);
#else
            (void)values;
            return true;
#endif
        }

        bool HasOnlyFiniteValuesScalar(const Vector_<>& values) {
            unsigned allFinite = 1;
            for (const double value : values)
                allFinite &= static_cast<unsigned>(std::isfinite(value));
            return allFinite != 0;
        }

        bool HasOnlyFiniteValues(const Vector_<>& values) {
#if defined(__AVX2__)
            return HasOnlyFiniteValuesAvx2(values);
#elif defined(__SSE2__) || defined(_M_X64)
            return HasOnlyFiniteValuesSse2(values);
#else
            return HasOnlyFiniteValuesScalar(values);
#endif
        }

        [[noreturn]] void ThrowInvalidCallbackSize(int actualSize, int expectedSize, const char* solver, const char* callback, bool preconditioner) {
            const char* invalidCategory = preconditioner ? "invalid preconditioner result" : "invalid operator result";
            THROW(std::string(solver) + ": " + invalidCategory + ": " + callback + " expected size " + std::to_string(expectedSize) +
                  ", actual size " + std::to_string(actualSize));
        }

        FORCE_INLINE void
        ValidateCallbackResult(const Vector_<>& values, int expectedSize, const char* solver, const char* callback, bool preconditioner) {
            const char* nonFiniteCategory = preconditioner ? "non-finite preconditioner result" : "non-finite operator result";
            if (static_cast<int>(values.size()) != expectedSize)
                ThrowInvalidCallbackSize(static_cast<int>(values.size()), expectedSize, solver, callback, preconditioner);
            if (HasOnlyFiniteValues(values))
                return;
            for (int i = 0; i < expectedSize; ++i)
                if (!std::isfinite(values[i]))
                    ThrowFailureAtIndex(solver, nonFiniteCategory, callback, i);
        }

        void ValidateCallbackShape(const Vector_<>& values, int expectedSize, const char* solver, const char* callback, bool preconditioner) {
            if (static_cast<int>(values.size()) == expectedSize)
                return;
            ThrowInvalidCallbackSize(static_cast<int>(values.size()), expectedSize, solver, callback, preconditioner);
        }

        double NormalizedScaledRatio(const Scaled_& numerator, const Scaled_& denominator) {
            const Scaled_ scaledNumerator = NormalizeScaledIfNeeded(numerator);
            const Scaled_ scaledDenominator = NormalizeScaledIfNeeded(denominator);
            return std::ldexp(scaledNumerator.mantissa_ / scaledDenominator.mantissa_, scaledNumerator.exponent_ - scaledDenominator.exponent_);
        }

        double ScaledRatio(
            const Scaled_& numerator, const Scaled_& denominator, const char* solver, const char* denominatorSubject, const char* ratioSubject) {
            if (denominator.mantissa_ == 0.0)
                ThrowFailure(solver, "numerical breakdown", denominatorSubject);
            if (numerator.mantissa_ == 0.0)
                return 0.0;
            double ratio = numerator.mantissa_ / denominator.mantissa_;
            if (numerator.normalized_ || denominator.normalized_)
                ratio = NormalizedScaledRatio(numerator, denominator);
            if (!std::isfinite(ratio) || ratio == 0.0)
                ThrowFailure(solver, "numerical breakdown", ratioSubject);
            return ratio;
        }

        void StableCombination(
            double multiplier, const Vector_<>& values, const Vector_<>& base, const char* solver, const char* subject, Vector_<>* result) {
            (void)solver;
            (void)subject;
            for (int i = 0; i < static_cast<int>(values.size()); ++i)
                (*result)[i] = std::fma(multiplier, values[i], base[i]);
        }

#if defined(__SSE2__) || defined(_M_X64)
        struct StableBatch_ {
            unsigned priorStatus_;
            bool finished_ = false;

            StableBatch_() : priorStatus_(_mm_getcsr()) { _mm_setcsr(priorStatus_ & ~(_MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW)); }

            ~StableBatch_() {
                if (!finished_) {
                    const unsigned raised = _mm_getcsr() & (_MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW);
                    _mm_setcsr(priorStatus_ | raised);
                }
            }

            void Finish(const char* solver,
                        const Vector_<>& first,
                        const char* firstSubject,
                        const Vector_<>* second = nullptr,
                        const char* secondSubject = nullptr,
                        const Vector_<>* third = nullptr,
                        const char* thirdSubject = nullptr) {
                const unsigned raised = _mm_getcsr() & (_MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW);
                _mm_setcsr(priorStatus_ | raised);
                finished_ = true;
                if (raised == 0)
                    return;
                if (!HasOnlyFiniteValues(first))
                    ThrowFailure(solver, "numerical breakdown", firstSubject);
                if (second && !HasOnlyFiniteValues(*second))
                    ThrowFailure(solver, "numerical breakdown", secondSubject);
                if (third && !HasOnlyFiniteValues(*third))
                    ThrowFailure(solver, "numerical breakdown", thirdSubject);
                ThrowFailure(solver, "numerical breakdown", firstSubject);
            }
        };
#else
        struct StableBatch_ {
            void Finish(const char* solver,
                        const Vector_<>& first,
                        const char* firstSubject,
                        const Vector_<>* second = nullptr,
                        const char* secondSubject = nullptr,
                        const Vector_<>* third = nullptr,
                        const char* thirdSubject = nullptr) {
                if (!HasOnlyFiniteValues(first))
                    ThrowFailure(solver, "numerical breakdown", firstSubject);
                if (second && !HasOnlyFiniteValues(*second))
                    ThrowFailure(solver, "numerical breakdown", secondSubject);
                if (third && !HasOnlyFiniteValues(*third))
                    ThrowFailure(solver, "numerical breakdown", thirdSubject);
            }
        };
#endif

#if defined(__AVX2__) && defined(__FMA__)
        struct DirectResidualEvidence_ {
            unsigned callbackFinite_;
            unsigned residualFinite_;
            double squareSum_;
            int firstScalar_;
        };

        DirectResidualEvidence_ AccumulateDirectResidualPrefix(Vector_<>* callbackValues, const Vector_<>& b, int expectedSize) {
            const __m256d minusOne = _mm256_set1_pd(-1.0);
            const __m256d zero = _mm256_setzero_pd();
            __m256d callbackEvidence = zero;
            __m256d residualEvidence = zero;
            __m256d sumSquares = zero;
            int i = 0;
            for (; i + 3 < expectedSize; i += 4) {
                const __m256d callbackItems = _mm256_loadu_pd(&(*callbackValues)[i]);
                callbackEvidence = _mm256_or_pd(callbackEvidence, _mm256_sub_pd(callbackItems, callbackItems));
                const __m256d bItems = _mm256_loadu_pd(&b[i]);
                const __m256d residualItems = _mm256_fmadd_pd(minusOne, callbackItems, bItems);
                residualEvidence = _mm256_or_pd(residualEvidence, _mm256_sub_pd(residualItems, residualItems));
                sumSquares = _mm256_fmadd_pd(residualItems, residualItems, sumSquares);
                _mm256_storeu_pd(&(*callbackValues)[i], residualItems);
            }
            alignas(32) double squareLanes[4];
            _mm256_store_pd(squareLanes, sumSquares);
            return {_mm256_movemask_pd(_mm256_cmp_pd(callbackEvidence, zero, _CMP_EQ_OQ)) == 15,
                    _mm256_movemask_pd(_mm256_cmp_pd(residualEvidence, zero, _CMP_EQ_OQ)) == 15,
                    squareLanes[0] + squareLanes[1] + squareLanes[2] + squareLanes[3], i};
        }

        void AccumulateDirectResidualTail(Vector_<>* callbackValues, const Vector_<>& b, int expectedSize, DirectResidualEvidence_* evidence) {
            for (int i = evidence->firstScalar_; i < expectedSize; ++i) {
                const double callbackValue = (*callbackValues)[i];
                evidence->callbackFinite_ &= static_cast<unsigned>(std::isfinite(callbackValue));
                (*callbackValues)[i] = std::fma(-1.0, callbackValue, b[i]);
                evidence->residualFinite_ &= static_cast<unsigned>(std::isfinite((*callbackValues)[i]));
                evidence->squareSum_ += (*callbackValues)[i] * (*callbackValues)[i];
            }
        }

        void ValidateDirectResidualEvidence(
            const Vector_<>& callbackValues, int expectedSize, const DirectResidualEvidence_& evidence, const char* solver, const char* callback) {
            if (evidence.callbackFinite_ == 0) {
                for (int i = 0; i < expectedSize; ++i)
                    if (!std::isfinite(callbackValues[i]))
                        ThrowFailureAtIndex(solver, "non-finite operator result", callback, i);
            }
            if (evidence.residualFinite_ == 0)
                ThrowFailure(solver, "numerical breakdown", "direct residual");
        }

        Scaled_ DirectResidualNorm(const Vector_<>& residual, double squareSum) {
            if (std::isfinite(squareSum) && squareSum >= std::numeric_limits<double>::min())
                return ScaledFromDouble(std::sqrt(squareSum));
            return SlowScaledNorm(residual);
        }
#endif

        Scaled_ ValidatedDirectResidual(Vector_<>* callbackValues, const Vector_<>& b, int expectedSize, const char* solver, const char* callback) {
            ValidateCallbackShape(*callbackValues, expectedSize, solver, callback, false);
#if defined(__AVX2__) && defined(__FMA__)
            DirectResidualEvidence_ evidence = AccumulateDirectResidualPrefix(callbackValues, b, expectedSize);
            AccumulateDirectResidualTail(callbackValues, b, expectedSize, &evidence);
            ValidateDirectResidualEvidence(*callbackValues, expectedSize, evidence, solver, callback);
            return DirectResidualNorm(*callbackValues, evidence.squareSum_);
#else
            ValidateCallbackResult(*callbackValues, expectedSize, solver, callback, false);
            StableBatch_ stableBatch;
            StableCombination(-1.0, *callbackValues, b, solver, "direct residual", callbackValues);
            stableBatch.Finish(solver, *callbackValues, "direct residual");
            return ScaledNorm(*callbackValues);
#endif
        }

        struct KrylovState_ {
            const Sparse::Square_& A_;
            const XPrecondition_& precondition_;
            const bool biConjugate_;
            const bool symmetricBiConjugate_;
            Vector_<> r_;
            Vector_<> rr_;
            Vector_<> p_;
            Vector_<> pp_;
            Vector_<> pCandidate_;
            Vector_<> ppCandidate_;
            Vector_<> xCandidate_;
            Vector_<> rCandidate_;
            Vector_<> rrCandidate_;
            Vector_<> directResidual_;
            Scaled_ betaPrev_;

            KrylovState_(const Sparse::Square_& a, const XPrecondition_& prec, bool biConjugate, int n)
                : A_(a), precondition_(prec), biConjugate_(biConjugate), symmetricBiConjugate_(biConjugate && prec.IsIdentity() && a.IsSymmetric()),
                  r_(n), rr_(biConjugate ? n : 0), p_(n, 0.0), pp_(biConjugate ? n : 0, 0.0), pCandidate_(n), ppCandidate_(biConjugate ? n : 0),
                  xCandidate_(n), rCandidate_(n), rrCandidate_(biConjugate ? n : 0), directResidual_(n), betaPrev_({0.0, 0, false}) {}
        };

        bool PrepareLeftPreconditioner(KrylovState_* state, const char* solver) {
            const bool hasPreconditioner = state->precondition_.Left(state->r_, &state->pCandidate_);
            if (hasPreconditioner)
                ValidateCallbackResult(state->pCandidate_, state->A_.Size(), solver, "PreConditionerSolveLeft", true);
            return hasPreconditioner;
        }

        bool PrepareRightPreconditioner(KrylovState_* state, const char* solver) {
            if (!state->biConjugate_)
                return false;
            const bool hasPreconditioner = state->precondition_.Right(state->rr_, &state->ppCandidate_);
            if (hasPreconditioner)
                ValidateCallbackResult(state->ppCandidate_, state->A_.Size(), solver, "PreConditionerSolveRight", true);
            return hasPreconditioner;
        }

        const Vector_<>& LeftPreconditioned(const KrylovState_& state, bool hasPreconditioner) {
            return hasPreconditioner ? state.pCandidate_ : state.r_;
        }

        const Vector_<>& RightPreconditioned(const KrylovState_& state, bool hasPreconditioner) {
            return hasPreconditioner ? state.ppCandidate_ : state.rr_;
        }

        const Vector_<>& ShadowPreconditioned(const KrylovState_& state, const Vector_<>& left, const Vector_<>& right) {
            return state.biConjugate_ ? right : left;
        }

        double DirectionBetaRatio(const KrylovState_& state, const Scaled_& beta, int iteration, const char* solver) {
            if (iteration == 0)
                return 0.0;
            return ScaledRatio(beta, state.betaPrev_, solver, "beta", "beta ratio");
        }

        bool NeedsShadowDirection(const KrylovState_& state) { return state.biConjugate_ && !state.symmetricBiConjugate_; }

        void PrepareDirectionCandidates(
            KrylovState_* state, double betaRatio, const Vector_<>& leftPreconditioned, const Vector_<>& rightPreconditioned, const char* solver) {
            StableBatch_ stableBatch;
            StableCombination(betaRatio, state->p_, leftPreconditioned, solver, "candidate direction", &state->pCandidate_);
            const Vector_<>* shadowCandidate = nullptr;
            if (NeedsShadowDirection(*state)) {
                StableCombination(betaRatio, state->pp_, rightPreconditioned, solver, "candidate shadow direction", &state->ppCandidate_);
                shadowCandidate = &state->ppCandidate_;
            }
            stableBatch.Finish(solver, state->pCandidate_, "candidate direction", shadowCandidate, "candidate shadow direction");
        }

        Scaled_ PrepareDirection(KrylovState_& s, int iteration, const char* solver) {
            const bool hasLeftPreconditioner = PrepareLeftPreconditioner(&s, solver);
            const bool hasRightPreconditioner = PrepareRightPreconditioner(&s, solver);
            const Vector_<>& leftPreconditioned = LeftPreconditioned(s, hasLeftPreconditioner);
            const Vector_<>& rightPreconditioned = RightPreconditioned(s, hasRightPreconditioner);
            const Vector_<>& shadowPreconditioned = ShadowPreconditioned(s, leftPreconditioned, rightPreconditioned);
            const Scaled_ beta = ScaledDot(shadowPreconditioned, s.r_);
            const double betaRatio = DirectionBetaRatio(s, beta, iteration, solver);
            PrepareDirectionCandidates(&s, betaRatio, leftPreconditioned, rightPreconditioned, solver);
            return beta;
        }

        BcgScaledAlphaPrivate_::StoredScaledBits_ CaptureStoredBits(const Scaled_& value) {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &value.mantissa_, sizeof(bits));
            return {bits, value.exponent_, value.normalized_};
        }

        void PrepareScaledCandidates(KrylovState_& s, const BcgScaledAlphaPrivate_::ExactAlpha_& alpha, const Vector_<>& x, const char* solver) {
            BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
#if defined(DAL35_ENABLE_TEST_SEAM)
            Dal35ObserveExactWorkspaceConstructionForTest_();
#endif
            const BcgScaledAlphaPrivate_::CandidateGroup_ group{&s.pCandidate_,
                                                                &x,
                                                                &s.r_,
                                                                s.biConjugate_ ? &s.rr_ : nullptr,
                                                                &s.xCandidate_,
                                                                &s.rCandidate_,
                                                                s.biConjugate_ ? &s.rrCandidate_ : nullptr};
            const BcgScaledAlphaPrivate_::CandidateEvidence_ evidence = BcgScaledAlphaPrivate_::EvaluateCandidateGroup_(alpha, group, &workspace);
            if (evidence.subject_ == BcgScaledAlphaPrivate_::CandidateSubject_::X)
                ThrowFailure(solver, "numerical breakdown", "candidate x");
            if (evidence.subject_ == BcgScaledAlphaPrivate_::CandidateSubject_::RESIDUAL)
                ThrowFailure(solver, "numerical breakdown", "candidate residual");
            if (evidence.subject_ == BcgScaledAlphaPrivate_::CandidateSubject_::SHADOW_RESIDUAL)
                ThrowFailure(solver, "numerical breakdown", "candidate shadow residual");
        }

        void PrepareCandidate(KrylovState_& s, const Scaled_& beta, const Vector_<>& x, const char* solver) {
            s.A_.MultiplyLeft(s.pCandidate_, &s.rCandidate_);
            ValidateCallbackResult(s.rCandidate_, s.A_.Size(), solver, "MultiplyLeft", false);
            const Vector_<>& rightDirection = s.symmetricBiConjugate_ ? s.pCandidate_ : s.ppCandidate_;
            if (s.biConjugate_)
                s.A_.MultiplyRight(rightDirection, &s.rrCandidate_);
            if (s.biConjugate_)
                ValidateCallbackResult(s.rrCandidate_, s.A_.Size(), solver, "MultiplyRight", false);

            const Vector_<>& shadowDirection = s.biConjugate_ ? rightDirection : s.pCandidate_;
            const Scaled_ alphaDenominator = ScaledDot(s.rCandidate_, shadowDirection);
            double alpha = 0.0;
            const BcgScaledAlphaPrivate_::AlphaPlan_ alphaPlan =
                BcgScaledAlphaPrivate_::ClassifyAlphaAndInvokeLegacy_(CaptureStoredBits(beta), CaptureStoredBits(alphaDenominator), [&]() {
                    alpha = ScaledRatio(beta, alphaDenominator, solver, "alpha denominator", "alpha ratio");
                });
            if (alphaPlan.path_ == BcgScaledAlphaPrivate_::AlphaPath_::DENOMINATOR_ZERO)
                ThrowFailure(solver, "numerical breakdown", "alpha denominator");
#if defined(DAL35_PROBE_ORDINARY_WORKSPACE_CONSTRUCTION)
            if (alphaPlan.path_ == BcgScaledAlphaPrivate_::AlphaPath_::ORDINARY_NORMAL)
                PrepareScaledCandidates(s, alphaPlan.exact_, x, solver);
#endif
            if (alphaPlan.path_ == BcgScaledAlphaPrivate_::AlphaPath_::SCALED_EXACT) {
                PrepareScaledCandidates(s, alphaPlan.exact_, x, solver);
                return;
            }
            StableBatch_ stableBatch;
            StableCombination(alpha, s.pCandidate_, x, solver, "candidate x", &s.xCandidate_);
            StableCombination(-alpha, s.rCandidate_, s.r_, solver, "candidate residual", &s.rCandidate_);
            if (s.biConjugate_)
                StableCombination(-alpha, s.rrCandidate_, s.rr_, solver, "candidate shadow residual", &s.rrCandidate_);
            stableBatch.Finish(solver, s.xCandidate_, "candidate x", &s.rCandidate_, "candidate residual", s.biConjugate_ ? &s.rrCandidate_ : nullptr,
                               "candidate shadow residual");
        }

        void CommitCandidate(KrylovState_* state, const Scaled_& beta, Vector_<>* x) {
            x->Swap(&state->xCandidate_);
            state->r_.Swap(&state->rCandidate_);
            state->p_.Swap(&state->pCandidate_);
            if (state->biConjugate_) {
                state->rr_.Swap(&state->rrCandidate_);
                if (!state->symmetricBiConjugate_)
                    state->pp_.Swap(&state->ppCandidate_);
            }
            state->betaPrev_ = beta;
        }

        void ConfirmAndCommit(
            KrylovState_* state, const Scaled_& beta, const Vector_<>& b, const Convergence_& convergence, const char* solver, Vector_<>* x) {
            state->A_.MultiplyLeft(state->xCandidate_, &state->directResidual_);
            const Scaled_ directResidualNorm = ValidatedDirectResidual(&state->directResidual_, b, state->A_.Size(), solver, "MultiplyLeft");
            if (!convergence.IsConverged(state->directResidual_, directResidualNorm))
                ThrowFailure(solver, "numerical breakdown", "direct residual confirmation");
            CommitCandidate(state, beta, x);
        }

        void ValidateKrylovParams(int n, const Vector_<>& b, const Vector_<>* x, double tolRel, double tolAbs, int maxIterations) {
            if (static_cast<int>(b.size()) != n || static_cast<int>(x->size()) != n)
                THROW("matrix dimensions are incompatible");
            if (!std::isfinite(tolRel))
                return;
            if (!std::isfinite(tolAbs))
                return;
            if (tolRel < 0.0 || tolAbs < 0.0 || (!IsPositive(tolRel) && !IsPositive(tolAbs)) || maxIterations <= 0)
                THROW("parameters are invalid");
        }

        Scaled_ ValidateInputsAndGetRhsNorm(const Vector_<>& b, const Vector_<>& x, const char* solver) {
            unsigned allFinite = 1;
            double sumSquares = 0.0;
            for (int i = 0; i < static_cast<int>(b.size()); ++i) {
                allFinite &= static_cast<unsigned>(std::isfinite(b[i]));
                allFinite &= static_cast<unsigned>(std::isfinite(x[i]));
                sumSquares += b[i] * b[i];
            }
            if (allFinite == 0) {
                ValidateFiniteInput(b, solver, "b");
                ValidateFiniteInput(x, solver, "x");
            }
            return std::isfinite(sumSquares) && sumSquares >= std::numeric_limits<double>::min() ? ScaledFromDouble(std::sqrt(sumSquares))
                                                                                                 : SlowScaledNorm(b);
        }

        void
        KrylovSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, bool biConjugate, Vector_<>* x) {
            const int n = A.Size();
            const char* solver = biConjugate ? "BCGSolve" : "CGSolve";
            ValidateKrylovParams(n, b, x, tolRel, tolAbs, maxIterations);
            if (!std::isfinite(tolRel))
                ThrowFailure(solver, "non-finite input", "tolRel");
            if (!std::isfinite(tolAbs))
                ThrowFailure(solver, "non-finite input", "tolAbs");

            const Scaled_ rhsNorm = ValidateInputsAndGetRhsNorm(b, *x, solver);
            const Convergence_ convergence(b, tolRel, tolAbs, rhsNorm);
            XPrecondition_ precondition(A);
            KrylovState_ s(A, precondition, biConjugate, n);

            A.MultiplyLeft(*x, &s.r_);
            const Scaled_ initialResidualNorm = ValidatedDirectResidual(&s.r_, b, n, solver, "MultiplyLeft");
            if (biConjugate)
                for (int i = 0; i < n; ++i)
                    s.rr_[i] = s.r_[i];
            if (initialResidualNorm.mantissa_ == 0.0)
                return;
            if (biConjugate && convergence.IsConverged(s.r_, initialResidualNorm))
                return;

            for (int iteration = 0; iteration < maxIterations; ++iteration) {
                const Scaled_ beta = PrepareDirection(s, iteration, solver);
                PrepareCandidate(s, beta, *x, solver);
                if (convergence.IsConverged(s.rCandidate_)) {
                    ConfirmAndCommit(&s, beta, b, convergence, solver, x);
                    return;
                }
                CommitCandidate(&s, beta, x);
            }
            THROW(biConjugate ? "Exhausted iterations in BCGSolve" : "Exhausted iterations in CGSolve");
        }
    } // namespace

    void Sparse::CGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve(A, b, tolRel, tolAbs, maxIterations, false, x);
    }

    void Sparse::BCGSolve(const Sparse::Square_& A, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        KrylovSolve(A, b, tolRel, tolAbs, maxIterations, true, x);
    }
} // namespace Dal
