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

        Scaled_ ScaledFromDouble(double value) {
            return {value, 0, false};
        }

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

        Scaled_ AddScaled(Scaled_ lhs, Scaled_ rhs) {
            if (lhs.mantissa_ == 0.0)
                return rhs;
            if (rhs.mantissa_ == 0.0)
                return lhs;
            if (!lhs.normalized_ && !rhs.normalized_) {
                const double sum = lhs.mantissa_ + rhs.mantissa_;
                if (std::isfinite(sum) && sum != 0.0)
                    return ScaledFromDouble(sum);
            }
            if (!lhs.normalized_)
                lhs = NormalizeScaled(lhs.mantissa_, 0);
            if (!rhs.normalized_)
                rhs = NormalizeScaled(rhs.mantissa_, 0);
            if (lhs.exponent_ < rhs.exponent_)
                std::swap(lhs, rhs);
            return NormalizeScaled(lhs.mantissa_ + std::ldexp(rhs.mantissa_, rhs.exponent_ - lhs.exponent_), lhs.exponent_);
        }

        Scaled_ SlowScaledDot(const Vector_<>& lhs, const Vector_<>& rhs) {
            Scaled_ result = {0.0, 0, false};
            for (int i = 0; i < static_cast<int>(lhs.size()); ++i)
                result = AddScaled(result, ScaledProduct(lhs[i], rhs[i]));
            return result;
        }

        Scaled_ ScaledDot(const Vector_<>& lhs, const Vector_<>& rhs) {
            const double result = InnerProduct(lhs, rhs);
            return std::isfinite(result) && result != 0.0 ? ScaledFromDouble(result) : SlowScaledDot(lhs, rhs);
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
            Scaled_ uncertainThreshold_;

            Convergence_(const Vector_<>& b, double tolRel, double tolAbs, const Scaled_& rhsNorm)
                : b_(b), tolRel_(tolRel), tolAbs_(tolAbs),
                  uncertainty_(std::min(0.25, 8.0 * static_cast<double>(b.size() + 2) * std::numeric_limits<double>::epsilon())) {
                const Scaled_ threshold = AddScaled(MultiplyScaled(rhsNorm, tolRel), ScaledFromDouble(tolAbs));
                certainThreshold_ = MultiplyScaled(threshold, (1.0 - uncertainty_) / (1.0 + uncertainty_));
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
                if (ScaledLessOrEqual(residualNorm, certainThreshold_))
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

        bool HasOnlyFiniteValues(const Vector_<>& values) {
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
#elif defined(__SSE2__) || defined(_M_X64)
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
            unsigned allFinite = 1;
            for (const double value : values)
                allFinite &= static_cast<unsigned>(std::isfinite(value));
            return allFinite != 0;
#endif
        }

        [[noreturn]] void
        ThrowInvalidCallbackSize(int actualSize, int expectedSize, const char* solver, const char* callback, bool preconditioner) {
            const char* invalidCategory = preconditioner ? "invalid preconditioner result" : "invalid operator result";
            THROW(std::string(solver) + ": " + invalidCategory + ": " + callback + " expected size " + std::to_string(expectedSize) +
                  ", actual size " + std::to_string(actualSize));
        }

        FORCE_INLINE void ValidateCallbackResult(
            const Vector_<>& values, int expectedSize, const char* solver, const char* callback, bool preconditioner) {
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

        double ScaledRatio(
            const Scaled_& numerator, const Scaled_& denominator, const char* solver, const char* denominatorSubject, const char* ratioSubject) {
            if (denominator.mantissa_ == 0.0)
                ThrowFailure(solver, "numerical breakdown", denominatorSubject);
            if (numerator.mantissa_ == 0.0)
                return 0.0;
            double ratio = numerator.mantissa_ / denominator.mantissa_;
            if (numerator.normalized_ || denominator.normalized_) {
                const Scaled_ scaledNumerator = numerator.normalized_ ? numerator : NormalizeScaled(numerator.mantissa_, 0);
                const Scaled_ scaledDenominator = denominator.normalized_ ? denominator : NormalizeScaled(denominator.mantissa_, 0);
                ratio = std::ldexp(scaledNumerator.mantissa_ / scaledDenominator.mantissa_,
                                   scaledNumerator.exponent_ - scaledDenominator.exponent_);
            }
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

            StableBatch_() : priorStatus_(_mm_getcsr()) {
                _mm_setcsr(priorStatus_ & ~(_MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW));
            }

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

        Scaled_ ValidatedDirectResidual(
            Vector_<>* callbackValues, const Vector_<>& b, int expectedSize, const char* solver, const char* callback) {
            ValidateCallbackShape(*callbackValues, expectedSize, solver, callback, false);
#if defined(__AVX2__) && defined(__FMA__)
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
            unsigned callbackFinite = _mm256_movemask_pd(_mm256_cmp_pd(callbackEvidence, zero, _CMP_EQ_OQ)) == 15;
            unsigned residualFinite = _mm256_movemask_pd(_mm256_cmp_pd(residualEvidence, zero, _CMP_EQ_OQ)) == 15;
            alignas(32) double squareLanes[4];
            _mm256_store_pd(squareLanes, sumSquares);
            double squareSum = squareLanes[0] + squareLanes[1] + squareLanes[2] + squareLanes[3];
            for (; i < expectedSize; ++i) {
                const double callbackValue = (*callbackValues)[i];
                callbackFinite &= static_cast<unsigned>(std::isfinite(callbackValue));
                (*callbackValues)[i] = std::fma(-1.0, callbackValue, b[i]);
                residualFinite &= static_cast<unsigned>(std::isfinite((*callbackValues)[i]));
                squareSum += (*callbackValues)[i] * (*callbackValues)[i];
            }
            if (callbackFinite == 0)
                for (int j = 0; j < expectedSize; ++j)
                    if (!std::isfinite((*callbackValues)[j]))
                        ThrowFailureAtIndex(solver, "non-finite operator result", callback, j);
            if (residualFinite == 0)
                ThrowFailure(solver, "numerical breakdown", "direct residual");
            return std::isfinite(squareSum) && squareSum != 0.0 ? ScaledFromDouble(std::sqrt(squareSum)) : SlowScaledNorm(*callbackValues);
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
                : A_(a), precondition_(prec), biConjugate_(biConjugate),
                  symmetricBiConjugate_(biConjugate && prec.IsIdentity() && a.IsSymmetric()), r_(n), rr_(biConjugate ? n : 0), p_(n, 0.0),
                  pp_(biConjugate ? n : 0, 0.0), pCandidate_(n), ppCandidate_(biConjugate ? n : 0), xCandidate_(n), rCandidate_(n),
                  rrCandidate_(biConjugate ? n : 0), directResidual_(n), betaPrev_({0.0, 0, false}) {}
        };

        Scaled_ PrepareDirection(KrylovState_& s, int iteration, const char* solver) {
            const bool hasLeftPreconditioner = s.precondition_.Left(s.r_, &s.pCandidate_);
            if (hasLeftPreconditioner)
                ValidateCallbackResult(s.pCandidate_, s.A_.Size(), solver, "PreConditionerSolveLeft", true);
            const bool hasRightPreconditioner = s.biConjugate_ && s.precondition_.Right(s.rr_, &s.ppCandidate_);
            if (hasRightPreconditioner)
                ValidateCallbackResult(s.ppCandidate_, s.A_.Size(), solver, "PreConditionerSolveRight", true);

            const Vector_<>& leftPreconditioned = hasLeftPreconditioner ? s.pCandidate_ : s.r_;
            const Vector_<>& rightPreconditioned = hasRightPreconditioner ? s.ppCandidate_ : s.rr_;
            const Vector_<>& shadowPreconditioned = s.biConjugate_ ? rightPreconditioned : leftPreconditioned;
            const Scaled_ beta = ScaledDot(shadowPreconditioned, s.r_);
            const double betaRatio = iteration > 0 ? ScaledRatio(beta, s.betaPrev_, solver, "beta", "beta ratio") : 0.0;
            StableBatch_ stableBatch;
            StableCombination(betaRatio, s.p_, leftPreconditioned, solver, "candidate direction", &s.pCandidate_);
            if (s.biConjugate_ && !s.symmetricBiConjugate_)
                StableCombination(betaRatio, s.pp_, rightPreconditioned, solver, "candidate shadow direction", &s.ppCandidate_);
            stableBatch.Finish(
                solver,
                s.pCandidate_,
                "candidate direction",
                s.biConjugate_ && !s.symmetricBiConjugate_ ? &s.ppCandidate_ : nullptr,
                "candidate shadow direction");
            return beta;
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
            const double alpha = ScaledRatio(beta, alphaDenominator, solver, "alpha denominator", "alpha ratio");
            StableBatch_ stableBatch;
            StableCombination(alpha, s.pCandidate_, x, solver, "candidate x", &s.xCandidate_);
            StableCombination(-alpha, s.rCandidate_, s.r_, solver, "candidate residual", &s.rCandidate_);
            if (s.biConjugate_)
                StableCombination(-alpha, s.rrCandidate_, s.rr_, solver, "candidate shadow residual", &s.rrCandidate_);
            stableBatch.Finish(solver,
                               s.xCandidate_,
                               "candidate x",
                               &s.rCandidate_,
                               "candidate residual",
                               s.biConjugate_ ? &s.rrCandidate_ : nullptr,
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
            const Scaled_ directResidualNorm =
                ValidatedDirectResidual(&state->directResidual_, b, state->A_.Size(), solver, "MultiplyLeft");
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
