//
// Created by wegamekinglc on 22-12-17.
//

#include <cmath>
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
            return std::isfinite(sumSquares) && sumSquares != 0.0 ? ScaledFromDouble(std::sqrt(sumSquares)) : SlowScaledNorm(values);
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

        bool IsConverged(const Vector_<>& residual, const Scaled_& threshold) {
            return ScaledLessOrEqual(ScaledNorm(residual), threshold);
        }

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

        Scaled_ ValidatedCallbackDot(
            const Vector_<>& callbackValues, const Vector_<>& other, const char* solver, const char* callback, bool preconditioner) {
            unsigned allFinite = 1;
            double result = 0.0;
            for (int i = 0; i < static_cast<int>(callbackValues.size()); ++i) {
                allFinite &= static_cast<unsigned>(std::isfinite(callbackValues[i]));
                result += callbackValues[i] * other[i];
            }
            if (allFinite == 0) {
                const char* category = preconditioner ? "non-finite preconditioner result" : "non-finite operator result";
                for (int i = 0; i < static_cast<int>(callbackValues.size()); ++i)
                    if (!std::isfinite(callbackValues[i]))
                        ThrowFailureAtIndex(solver, category, callback, i);
            }
            return std::isfinite(result) && result != 0.0 ? ScaledFromDouble(result) : SlowScaledDot(callbackValues, other);
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
            mutable bool finished_ = false;

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
                        const char* thirdSubject = nullptr) const {
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
                        const char* thirdSubject = nullptr) const {
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
            ValidateCallbackShape(s.rCandidate_, s.A_.Size(), solver, "MultiplyLeft", false);
            const Vector_<>& rightDirection = s.symmetricBiConjugate_ ? s.pCandidate_ : s.ppCandidate_;
            if (s.biConjugate_)
                s.A_.MultiplyRight(rightDirection, &s.rrCandidate_);
            if (s.biConjugate_)
                ValidateCallbackResult(s.rrCandidate_, s.A_.Size(), solver, "MultiplyRight", false);

            const Vector_<>& shadowDirection = s.biConjugate_ ? rightDirection : s.pCandidate_;
            const Scaled_ alphaDenominator = ValidatedCallbackDot(s.rCandidate_, shadowDirection, solver, "MultiplyLeft", false);
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

        void ConfirmAndCommit(KrylovState_* state,
                              const Scaled_& beta,
                              const Vector_<>& b,
                              const Scaled_& convergenceThreshold,
                              const char* solver,
                              Vector_<>* x) {
            state->A_.MultiplyLeft(state->xCandidate_, &state->directResidual_);
            const Scaled_ directResidualNorm =
                ValidatedDirectResidual(&state->directResidual_, b, state->A_.Size(), solver, "MultiplyLeft");
            if (!ScaledLessOrEqual(directResidualNorm, convergenceThreshold))
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
            return std::isfinite(sumSquares) && sumSquares != 0.0 ? ScaledFromDouble(std::sqrt(sumSquares)) : SlowScaledNorm(b);
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
            const Scaled_ convergenceThreshold = AddScaled(MultiplyScaled(rhsNorm, tolRel), ScaledFromDouble(tolAbs));
            XPrecondition_ precondition(A);
            KrylovState_ s(A, precondition, biConjugate, n);

            A.MultiplyLeft(*x, &s.r_);
            const Scaled_ initialResidualNorm = ValidatedDirectResidual(&s.r_, b, n, solver, "MultiplyLeft");
            if (biConjugate)
                for (int i = 0; i < n; ++i)
                    s.rr_[i] = s.r_[i];
            if (initialResidualNorm.mantissa_ == 0.0)
                return;
            if (biConjugate && ScaledLessOrEqual(initialResidualNorm, convergenceThreshold))
                return;

            for (int iteration = 0; iteration < maxIterations; ++iteration) {
                const Scaled_ beta = PrepareDirection(s, iteration, solver);
                PrepareCandidate(s, beta, *x, solver);
                if (IsConverged(s.rCandidate_, convergenceThreshold)) {
                    ConfirmAndCommit(&s, beta, b, convergenceThreshold, solver, x);
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
