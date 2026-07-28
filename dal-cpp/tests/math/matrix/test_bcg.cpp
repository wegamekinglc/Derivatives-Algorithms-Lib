//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__SSE2__) || defined(_M_X64)
#include <immintrin.h>
#endif
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/bcg_scaled_alpha.inc>
#include "dal35_one_bit_oracle.hpp"

namespace {
    std::atomic<bool> dal35TrackAllocations_{false};
    std::atomic<std::size_t> dal35AllocationCount_{0};
#if defined(DAL35_ENABLE_TEST_SEAM)
    int dal35ExactWorkspaceConstructionCount_ = 0;
#endif

    void* Dal35Allocate_(std::size_t size) {
        if (dal35TrackAllocations_.load(std::memory_order_relaxed))
            dal35AllocationCount_.fetch_add(1, std::memory_order_relaxed);
        if (void* result = std::malloc(size == 0 ? 1 : size))
            return result;
        throw std::bad_alloc();
    }
} // namespace

#if defined(DAL35_ENABLE_TEST_SEAM)
#if defined(__GNUC__) || defined(__clang__)
#define DAL35_TEST_HIDDEN_ __attribute__((visibility("hidden")))
#else
#define DAL35_TEST_HIDDEN_
#endif
extern "C" DAL35_TEST_HIDDEN_ void Dal35ObserveExactWorkspaceConstructionForTest_() noexcept { ++dal35ExactWorkspaceConstructionCount_; }
#undef DAL35_TEST_HIDDEN_
#endif

void* operator new(std::size_t size) { return Dal35Allocate_(size); }

void* operator new[](std::size_t size) { return Dal35Allocate_(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return Dal35Allocate_(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return Dal35Allocate_(size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* address) noexcept { std::free(address); }

void operator delete[](void* address) noexcept { std::free(address); }

void operator delete(void* address, std::size_t) noexcept { std::free(address); }

void operator delete[](void* address, std::size_t) noexcept { std::free(address); }

void operator delete(void* address, const std::nothrow_t&) noexcept { std::free(address); }

void operator delete[](void* address, const std::nothrow_t&) noexcept { std::free(address); }

using namespace Dal;

namespace {
    class AllocationScope_ {
        bool active_ = true;

    public:
        AllocationScope_() noexcept {
            dal35AllocationCount_.store(0, std::memory_order_relaxed);
            dal35TrackAllocations_.store(true, std::memory_order_relaxed);
        }
        ~AllocationScope_() noexcept {
            if (active_)
                dal35TrackAllocations_.store(false, std::memory_order_relaxed);
        }
        std::size_t Finish() noexcept {
            dal35TrackAllocations_.store(false, std::memory_order_relaxed);
            active_ = false;
            return dal35AllocationCount_.load(std::memory_order_relaxed);
        }
    };

#if defined(DAL35_PROBE_TEST_TOP_CARRY_DELTA)
    constexpr int DAL35_TEST_TOP_CARRY_DELTA_ = DAL35_PROBE_TEST_TOP_CARRY_DELTA;
#else
    constexpr int DAL35_TEST_TOP_CARRY_DELTA_ = 0;
#endif

    constexpr BcgScaledAlphaPrivate_::AccumulatorBounds_ DAL35_TEST_PROBE_INPUT_ =
        BcgScaledAlphaPrivate_::WithTopRoundingCarryDelta_(BcgScaledAlphaPrivate_::REVIEWED_ACCUMULATOR_BOUNDS_, DAL35_TEST_TOP_CARRY_DELTA_);

    static_assert(BcgScaledAlphaPrivate_::REVIEWED_BOUNDS_FINGERPRINT_MATCHES_, "DAL35_TEST_SEAM_BOUNDS_FINGERPRINT_MISMATCH");
    static_assert(BcgScaledAlphaPrivate_::SameBoundsFingerprint_(
                      BcgScaledAlphaPrivate_::MakeBoundsFingerprint_(DAL35_TEST_PROBE_INPUT_,
                                                                     BcgScaledAlphaPrivate_::DeriveCandidateBounds_(DAL35_TEST_PROBE_INPUT_)),
                      BcgScaledAlphaPrivate_::REVIEWED_BOUNDS_FINGERPRINT_),
                  "DAL35_TEST_SEAM_BOUNDS_FINGERPRINT_MISMATCH");

    class RescaledIdentity_ : public Sparse::TriDiagonal_, public HasPreConditioner_ {
        static void Rescale(const Vector_<>& b, Vector_<>* x) { (*x)[0] = 1e170 * b[0]; }

    public:
        RescaledIdentity_() : Sparse::TriDiagonal_(1) { Set(0, 0, 1.0); }
        void PreConditionerSolveLeft(const Vector_<>& b, Vector_<>* x) const override { Rescale(b, x); }
        void PreConditionerSolveRight(const Vector_<>& b, Vector_<>* x) const override { Rescale(b, x); }
    };
} // namespace

TEST(MatrixTest, TestCGSolve) {
    const int n = 10;
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(n, 1, 1);
    mat->Set(9, 8, 3.0);
    mat->Set(8, 9, 3.0);

    for (int i = 0; i < n; ++i)
        mat->Set(i, i, 10.0);

    Vector_<> result(n);
    Vector_<> b(n, 1.0);
    Sparse::CGSolve(*mat, b, 1e-4, 1e-4, 100, &result);

    ASSERT_NEAR(result[8], 0.07692308, 1e-8);
    ASSERT_NEAR(result[9], 0.07692308, 1e-8);
}

TEST(MatrixTest, TestBCGSolve) {
    const int n = 10;
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(n, 1, 1);
    mat->Set(9, 8, 3.0);
    mat->Set(8, 9, 2.0);

    for (int i = 0; i < n; ++i)
        mat->Set(i, i, 10.0);

    Vector_<> result(n);
    Vector_<> b(n, 1.0);
    Sparse::BCGSolve(*mat, b, 1e-4, 1e-4, 100, &result);

    ASSERT_NEAR(result[8], 0.08510638, 1e-8);
    ASSERT_NEAR(result[9], 0.07446809, 1e-8);
}

TEST(MatrixTest, TestCGSolveAcceptsExactInitialGuess) {
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(2, 0, 0);
    mat->Set(0, 0, 2.0);
    mat->Set(1, 1, 3.0);
    const Vector_<> b = {2.0, 6.0};
    Vector_<> x = {1.0, 2.0};

    Sparse::CGSolve(*mat, b, 1e-12, 1e-14, 10, &x);

    ASSERT_DOUBLE_EQ(x[0], 1.0);
    ASSERT_DOUBLE_EQ(x[1], 2.0);
}

TEST(MatrixTest, TestBCGSolveAcceptsExactInitialGuess) {
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(2, 1, 1);
    mat->Set(0, 0, 2.0);
    mat->Set(0, 1, 1.0);
    mat->Set(1, 1, 3.0);
    const Vector_<> b = {4.0, 6.0};
    Vector_<> x = {1.0, 2.0};

    Sparse::BCGSolve(*mat, b, 1e-12, 1e-14, 10, &x);

    ASSERT_DOUBLE_EQ(x[0], 1.0);
    ASSERT_DOUBLE_EQ(x[1], 2.0);
}

TEST(MatrixTest, TestBCGSolveAcceptsToleranceConvergedInitialGuess) {
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(2, 1, 1);
    mat->Set(0, 1, 1.0);
    mat->Set(1, 0, -1.0);
    const Vector_<> b = {1.0e-12, 0.0};
    Vector_<> x = {0.0, 0.0};

    Sparse::BCGSolve(*mat, b, 0.0, 1.0e-10, 10, &x);

    ASSERT_DOUBLE_EQ(x[0], 0.0);
    ASSERT_DOUBLE_EQ(x[1], 0.0);
}

TEST(MatrixTest, TestCGSolveDoesNotTreatUnderflowedResidualAsZero) {
    const double nonzeroResidual = 1e-170;
    ASSERT_DOUBLE_EQ(nonzeroResidual * nonzeroResidual, 0.0);
    const RescaledIdentity_ mat;
    const Vector_<> b = {0.0};
    Vector_<> x = {nonzeroResidual};

    Sparse::CGSolve(mat, b, EPSILON, 0.0, 10, &x);

    ASSERT_DOUBLE_EQ(x[0], 0.0);
}

TEST(MatrixTest, TestBCGSolveDoesNotTreatUnderflowedResidualAsZero) {
    const double nonzeroResidual = 1e-170;
    ASSERT_DOUBLE_EQ(nonzeroResidual * nonzeroResidual, 0.0);
    const RescaledIdentity_ mat;
    const Vector_<> b = {0.0};
    Vector_<> x = {nonzeroResidual};

    Sparse::BCGSolve(mat, b, EPSILON, 0.0, 10, &x);

    ASSERT_DOUBLE_EQ(x[0], 0.0);
}

// Tri-diagonal systems with distinct band values exercise every branch of the
// fused Krylov sweeps. CG requires a symmetric positive-definite matrix, so it
// gets a symmetric system; BCG handles the asymmetric case (and its shadow path).
// The contract is residual ||Ax - b||_inf near machine precision after a tight solve.
namespace {
    void BuildSymmetricTridiag(Sparse::Square_* mat, int n) {
        for (int i = 0; i < n; ++i) {
            mat->Set(i, i, 4.0 + 0.1 * static_cast<double>(i));
            if (i > 0) {
                const double off = -0.5 - 0.01 * static_cast<double>(i);
                mat->Set(i, i - 1, off);
                mat->Set(i - 1, i, off);
            }
        }
    }

    void BuildAsymmetricTridiag(Sparse::Square_* mat, int n) {
        for (int i = 0; i < n; ++i) {
            mat->Set(i, i, 4.0 + 0.1 * static_cast<double>(i));
            if (i > 0)
                mat->Set(i, i - 1, -0.5 - 0.01 * static_cast<double>(i));
            if (i + 1 < n)
                mat->Set(i, i + 1, -0.3 + 0.02 * static_cast<double>(i));
        }
    }

    double ResidualInfNorm(const Sparse::Square_& A, const Vector_<>& x, const Vector_<>& b) {
        Vector_<> ax(x.size());
        A.MultiplyLeft(x, &ax);
        double worst = 0.0;
        for (int i = 0; i < static_cast<int>(ax.size()); ++i)
            worst = std::max(worst, std::fabs(ax[i] - b[i]));
        return worst;
    }
} // namespace

TEST(MatrixTest, TestCGSolveSymmetricLowResidual) {
    const int n = 40;
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(n, 1, 1);
    BuildSymmetricTridiag(mat.get(), n);

    Vector_<> b(n);
    for (int i = 0; i < n; ++i)
        b[i] = 1.0 + 0.1 * static_cast<double>(i % 7);
    Vector_<> x(n, 0.0);

    Sparse::CGSolve(*mat, b, 1e-12, 1e-14, 500, &x);
    ASSERT_LT(ResidualInfNorm(*mat, x, b), 1e-8);
}

TEST(MatrixTest, TestBCGSolveAsymmetricLowResidual) {
    const int n = 40;
    std::unique_ptr<Sparse::Square_> mat = Sparse::NewBandDiagonal(n, 1, 1);
    BuildAsymmetricTridiag(mat.get(), n);

    Vector_<> b(n);
    for (int i = 0; i < n; ++i)
        b[i] = 1.0 + 0.1 * static_cast<double>(i % 7);
    Vector_<> x(n, 0.0);

    Sparse::BCGSolve(*mat, b, 1e-12, 1e-14, 500, &x);
    ASSERT_LT(ResidualInfNorm(*mat, x, b), 1e-8);
}

namespace {
    using SolveFunction_ = void (*)(const Sparse::Square_&, const Vector_<>&, double, double, int, Vector_<>*);

    struct CallbackCounts_ {
        int left_ = 0;
        int right_ = 0;
        int preconditionerLeft_ = 0;
        int preconditionerRight_ = 0;
    };

    using CallbackHook_ = std::function<bool(int, const Vector_<>&, Vector_<>*)>;

    class HookedDiagonal_ : public Sparse::TriDiagonal_ {
        CallbackCounts_* counts_;
        CallbackHook_ leftHook_;
        CallbackHook_ rightHook_;

    public:
        HookedDiagonal_(const Vector_<>& diagonal, CallbackCounts_* counts) : Sparse::TriDiagonal_(diagonal.size()), counts_(counts) {
            for (int i = 0; i < static_cast<int>(diagonal.size()); ++i)
                Set(i, i, diagonal[i]);
        }

        void SetLeftHook(CallbackHook_ hook) { leftHook_ = std::move(hook); }
        void SetRightHook(CallbackHook_ hook) { rightHook_ = std::move(hook); }

        void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const override {
            const int call = ++counts_->left_;
            if (!leftHook_ || !leftHook_(call, x, b))
                Sparse::TriDiagonal_::MultiplyLeft(x, b);
        }

        void MultiplyRight(const Vector_<>& x, Vector_<>* b) const override {
            const int call = ++counts_->right_;
            if (!rightHook_ || !rightHook_(call, x, b))
                Sparse::TriDiagonal_::MultiplyRight(x, b);
        }
    };

    class HookedPreconditionedDiagonal_ : public HookedDiagonal_, public HasPreConditioner_ {
        CallbackCounts_* counts_;
        CallbackHook_ leftHook_;
        CallbackHook_ rightHook_;

        static void CopyVector(const Vector_<>& source, Vector_<>* destination) {
            for (int i = 0; i < static_cast<int>(source.size()); ++i)
                (*destination)[i] = source[i];
        }

    public:
        HookedPreconditionedDiagonal_(const Vector_<>& diagonal, CallbackCounts_* counts) : HookedDiagonal_(diagonal, counts), counts_(counts) {}

        void SetPreconditionerLeftHook(CallbackHook_ hook) { leftHook_ = std::move(hook); }
        void SetPreconditionerRightHook(CallbackHook_ hook) { rightHook_ = std::move(hook); }

        void PreConditionerSolveLeft(const Vector_<>& b, Vector_<>* x) const override {
            const int call = ++counts_->preconditionerLeft_;
            if (!leftHook_ || !leftHook_(call, b, x))
                CopyVector(b, x);
        }

        void PreConditionerSolveRight(const Vector_<>& b, Vector_<>* x) const override {
            const int call = ++counts_->preconditionerRight_;
            if (!rightHook_ || !rightHook_(call, b, x))
                CopyVector(b, x);
        }
    };

    class CallbackSentinel_ : public std::runtime_error {
    public:
        CallbackSentinel_() : std::runtime_error("callback sentinel") {}
    };

    struct OracleBinary_ {
        std::map<int, unsigned long long> bits_;
    };

    void OracleNormalizeBits(OracleBinary_* value) {
        for (auto it = value->bits_.begin(); it != value->bits_.end(); ++it) {
            const unsigned long long count = it->second;
            it->second = count & 1ULL;
            if (count > 1)
                value->bits_[it->first + 1] += count >> 1U;
        }
        for (auto it = value->bits_.begin(); it != value->bits_.end();) {
            if (it->second == 0)
                it = value->bits_.erase(it);
            else
                ++it;
        }
    }

    OracleBinary_ OracleFromDouble(double value) {
        OracleBinary_ result;
        if (value == 0.0)
            return result;
        int exponent = 0;
        const double mantissa = std::frexp(std::fabs(value), &exponent);
        const unsigned long long significand = static_cast<unsigned long long>(std::ldexp(mantissa, 53));
        const int leastBitExponent = exponent - 53;
        for (int bit = 0; bit < 53; ++bit)
            if ((significand & (1ULL << bit)) != 0)
                result.bits_[leastBitExponent + bit] = 1;
        return result;
    }

    OracleBinary_ OracleAdd(OracleBinary_ lhs, const OracleBinary_& rhs) {
        for (const auto& bit : rhs.bits_)
            lhs.bits_[bit.first] += bit.second;
        OracleNormalizeBits(&lhs);
        return lhs;
    }

    OracleBinary_ OracleMultiply(const OracleBinary_& lhs, const OracleBinary_& rhs) {
        OracleBinary_ result;
        for (const auto& lhsBit : lhs.bits_)
            for (const auto& rhsBit : rhs.bits_)
                result.bits_[lhsBit.first + rhsBit.first] += lhsBit.second * rhsBit.second;
        OracleNormalizeBits(&result);
        return result;
    }

    OracleBinary_ OracleShift(const OracleBinary_& value, int shift) {
        OracleBinary_ result;
        for (const auto& bit : value.bits_)
            result.bits_[bit.first + shift] = bit.second;
        return result;
    }

    int OracleCompare(const OracleBinary_& lhs, const OracleBinary_& rhs) {
        auto lhsBit = lhs.bits_.rbegin();
        auto rhsBit = rhs.bits_.rbegin();
        while (lhsBit != lhs.bits_.rend() || rhsBit != rhs.bits_.rend()) {
            if (rhsBit == rhs.bits_.rend() || (lhsBit != lhs.bits_.rend() && lhsBit->first > rhsBit->first))
                return 1;
            if (lhsBit == lhs.bits_.rend() || rhsBit->first > lhsBit->first)
                return -1;
            ++lhsBit;
            ++rhsBit;
        }
        return 0;
    }

    OracleBinary_ OracleSubtract(const OracleBinary_& lhs, const OracleBinary_& rhs) {
        if (rhs.bits_.empty())
            return lhs;
        OracleBinary_ result;
        const int leastExponent = std::min(lhs.bits_.begin()->first, rhs.bits_.begin()->first);
        const int greatestExponent = lhs.bits_.rbegin()->first;
        int borrow = 0;
        for (int exponent = leastExponent; exponent <= greatestExponent; ++exponent) {
            const int lhsBit = lhs.bits_.count(exponent) != 0 ? 1 : 0;
            const int rhsBit = rhs.bits_.count(exponent) != 0 ? 1 : 0;
            int difference = lhsBit - rhsBit - borrow;
            if (difference < 0) {
                difference += 2;
                borrow = 1;
            } else {
                borrow = 0;
            }
            if (difference != 0)
                result.bits_[exponent] = 1;
        }
        return result;
    }

    OracleBinary_ OracleSumSquares(const std::vector<OracleBinary_>& values) {
        OracleBinary_ result;
        for (const OracleBinary_& value : values)
            result = OracleAdd(std::move(result), OracleMultiply(value, value));
        return result;
    }

    struct OracleConvergenceTerms_ {
        std::vector<OracleBinary_> residual_;
        std::vector<OracleBinary_> relative_;
        OracleBinary_ absolute_;
        int commonExponent_ = std::numeric_limits<int>::min();
    };

    void IncludeOracleExponent(const OracleBinary_& value, int* commonExponent) {
        if (value.bits_.empty())
            return;
        *commonExponent = std::max(*commonExponent, value.bits_.rbegin()->first + 1);
    }

    OracleConvergenceTerms_ BuildOracleConvergenceTerms(const Vector_<>& residual, const Vector_<>& b, double tolRel, double tolAbs) {
        OracleConvergenceTerms_ result;
        result.residual_.reserve(residual.size());
        result.relative_.reserve(b.size());
        for (int i = 0; i < static_cast<int>(residual.size()); ++i) {
            OracleBinary_ residualTerm = OracleFromDouble(residual[i]);
            OracleBinary_ relativeTerm = OracleMultiply(OracleFromDouble(tolRel), OracleFromDouble(b[i]));
            IncludeOracleExponent(residualTerm, &result.commonExponent_);
            IncludeOracleExponent(relativeTerm, &result.commonExponent_);
            result.residual_.push_back(std::move(residualTerm));
            result.relative_.push_back(std::move(relativeTerm));
        }
        result.absolute_ = OracleFromDouble(tolAbs);
        IncludeOracleExponent(result.absolute_, &result.commonExponent_);
        if (result.commonExponent_ == std::numeric_limits<int>::min())
            result.commonExponent_ = 0;
        return result;
    }

    void ShiftOracleConvergenceTerms(OracleConvergenceTerms_* terms) {
        for (OracleBinary_& value : terms->residual_)
            value = OracleShift(value, -terms->commonExponent_);
        for (OracleBinary_& value : terms->relative_)
            value = OracleShift(value, -terms->commonExponent_);
        terms->absolute_ = OracleShift(terms->absolute_, -terms->commonExponent_);
    }

    bool OracleSquaresConverged(const OracleConvergenceTerms_& terms) {
        const OracleBinary_ residualSquares = OracleSumSquares(terms.residual_);
        const OracleBinary_ relativeSquares = OracleSumSquares(terms.relative_);
        if (OracleCompare(residualSquares, relativeSquares) <= 0)
            return true;

        const OracleBinary_ absoluteSquare = OracleMultiply(terms.absolute_, terms.absolute_);
        const OracleBinary_ baseToleranceSquare = OracleAdd(relativeSquares, absoluteSquare);
        if (OracleCompare(residualSquares, baseToleranceSquare) <= 0)
            return true;

        const OracleBinary_ difference = OracleSubtract(residualSquares, baseToleranceSquare);
        const OracleBinary_ crossTermSquare = OracleShift(OracleMultiply(absoluteSquare, relativeSquares), 2);
        return OracleCompare(OracleMultiply(difference, difference), crossTermSquare) <= 0;
    }

    bool CommonExponentConverged(const Vector_<>& residual, const Vector_<>& b, double tolRel, double tolAbs) {
        OracleConvergenceTerms_ terms = BuildOracleConvergenceTerms(residual, b, tolRel, tolAbs);
        ShiftOracleConvergenceTerms(&terms);
        return OracleSquaresConverged(terms);
    }

    Vector_<> DiagonalResidual(const Vector_<>& diagonal, const Vector_<>& x, const Vector_<>& b) {
        Vector_<> residual(b.size());
        for (int i = 0; i < static_cast<int>(b.size()); ++i)
            residual[i] = std::fma(-diagonal[i], x[i], b[i]);
        return residual;
    }

    std::uint64_t SplitMix64(std::uint64_t* state) {
        std::uint64_t value = (*state += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    double DoubleFromBits(std::uint64_t bits) {
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::uint64_t DoubleBits(double value) {
        std::uint64_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    struct OracleBootstrapRow_ {
        const char* id_;
        Dal35OneBitOracle_::OracleInput_ input_;
        std::uint64_t expectedBits_;
        Dal35OneBitOracle_::OracleClass_ expectedClass_;
    };

    const std::array<OracleBootstrapRow_, 10>& OracleBootstrapRows() {
        static const std::array<OracleBootstrapRow_, 10> rows = {{
            {"O1",
             {{{1}, {1}, 0, false}, 0x0000000000000000ULL, 0x0000000000000000ULL},
             0x0000000000000000ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O2",
             {{{1}, {1}, -1075, true}, 0x3ff0000000000000ULL, 0x0000000000000000ULL},
             0x8000000000000000ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O3",
             {{{1}, {1}, -1074, false}, 0x3ff0000000000000ULL, 0x0000000000000000ULL},
             0x0000000000000001ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O4",
             {{{1}, {1}, -1022, false}, 0x3ff0000000000000ULL, 0x0000000000000000ULL},
             0x0010000000000000ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O5",
             {{{1}, {1}, 0, false}, 0x7fefffffffffffffULL, 0x0000000000000000ULL},
             0x7fefffffffffffffULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O6", {{{1}, {1}, 1100, false}, 0x37d0000000000000ULL, 0x7fefffffffffffffULL}, 0, Dal35OneBitOracle_::OracleClass_::NON_FINITE},
            {"O7",
             {{{1}, {1}, 1100, false}, 0x37cfffffffffffffULL, 0x7fefffffffffffffULL},
             0x7fefffffffffffffULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O8", {{{1}, {1}, 1100, false}, 0x37d0000000000001ULL, 0x7fefffffffffffffULL}, 0, Dal35OneBitOracle_::OracleClass_::NON_FINITE},
            {"O9",
             {{{1, 0, 1}, {1, 1}, 1050, false}, 0x0000000003000000ULL, 0xc012000000000000ULL},
             0x3fe0000000000000ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
            {"O10",
             {{{1, 0, 1}, {1, 1}, 1050, true}, 0x0000000003000000ULL, 0x4012000000000000ULL},
             0xbfe0000000000000ULL,
             Dal35OneBitOracle_::OracleClass_::FINITE},
        }};
        return rows;
    }

    void AssertOracleBootstrap() {
        for (const OracleBootstrapRow_& row : OracleBootstrapRows()) {
            SCOPED_TRACE(row.id_);
            const Dal35OneBitOracle_::OracleResult_ result = Dal35OneBitOracle_::Evaluate_(row.input_);
            ASSERT_EQ(row.expectedClass_, result.classification_);
            if (result.classification_ == Dal35OneBitOracle_::OracleClass_::FINITE)
                ASSERT_EQ(row.expectedBits_, result.bits_);
        }
    }

    Dal35OneBitOracle_::OracleInput_ OracleInput(const BcgScaledAlphaPrivate_::ExactAlpha_& alpha, std::uint64_t valueBits, std::uint64_t baseBits) {
        return {{Dal35OneBitOracle_::FromU64_(alpha.numerator_), Dal35OneBitOracle_::FromU64_(alpha.denominator_), alpha.binaryExponent_,
                 alpha.negative_},
                valueBits,
                baseBits};
    }

    struct EvaluatorRow_ {
        const char* id_;
        BcgScaledAlphaPrivate_::ExactAlpha_ alpha_;
        std::uint64_t valueBits_;
        std::uint64_t baseBits_;
        std::uint64_t expectedBits_;
        BcgScaledAlphaPrivate_::RoundedClass_ expectedClass_;
    };

    const std::array<EvaluatorRow_, 21>& EvaluatorRows() {
        using BcgScaledAlphaPrivate_::RoundedClass_;
        static const std::array<EvaluatorRow_, 21> rows = {{
            {"E1", {1, 1, -1100, false}, 0x4180000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, RoundedClass_::FINITE},
            {"E2", {1, 1, -1100, false}, 0x4180000000000001ULL, 0x0000000000000000ULL, 0x0000000000000001ULL, RoundedClass_::FINITE},
            {"E3", {1, 1, -1100, false}, 0x417fffffffffffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL, RoundedClass_::FINITE},
            {"E4", {1, 1, -1100, true}, 0x4180000000000000ULL, 0x0000000000000000ULL, 0x8000000000000000ULL, RoundedClass_::FINITE},
            {"E5", {1, 1, -1523, false}, 0x5c00000000000000ULL, 0x0000000000000000ULL, 0x0000000000000001ULL, RoundedClass_::FINITE},
            {"E6", {1, 3, 1026, false}, 0x0000000000000003ULL, 0x4040000000000000ULL, 0x4040000000000000ULL, RoundedClass_::FINITE},
            {"E7", {1, 3, 1026, false}, 0x0000000000000003ULL, 0x4040000000000001ULL, 0x4040000000000002ULL, RoundedClass_::FINITE},
            {"E8", {5, 3, 1050, false}, 0x0000000003000000ULL, 0xc012000000000000ULL, 0x3fe0000000000000ULL, RoundedClass_::FINITE},
            {"E9", {5, 3, 1050, true}, 0x0000000003000000ULL, 0x4012000000000000ULL, 0xbfe0000000000000ULL, RoundedClass_::FINITE},
            {"E10", {1, 1, 1100, false}, 0x3b30000000000000ULL, 0xffe0000000000000ULL, 0x7fe0000000000000ULL, RoundedClass_::FINITE},
            {"E11", {1, 1, 1100, false}, 0xbb30000000000000ULL, 0x7fe0000000000000ULL, 0xffe0000000000000ULL, RoundedClass_::FINITE},
            {"E12", {1, 1, -1100, false}, 0xc4cffffffffffffeULL, 0x0010000000000000ULL, 0x0000000000000001ULL, RoundedClass_::FINITE},
            {"E13", {1, 1, -1100, false}, 0x44cffffffffffffeULL, 0x8010000000000000ULL, 0x8000000000000001ULL, RoundedClass_::FINITE},
            {"E14", {1, 1, 1100, false}, 0x39b0000000000000ULL, 0xfe70000000000000ULL, 0x0000000000000000ULL, RoundedClass_::FINITE},
            {"E15", {1, 1, 1100, false}, 0x3ff0000000000000ULL, 0x0000000000000000ULL, 0, RoundedClass_::NON_FINITE},
            {"E16", {1, 1, 1100, false}, 0x37cfffffffffffffULL, 0x7fefffffffffffffULL, 0x7fefffffffffffffULL, RoundedClass_::FINITE},
            {"E17", {1, 1, 1100, false}, 0x37d0000000000000ULL, 0x7fefffffffffffffULL, 0, RoundedClass_::NON_FINITE},
            {"E18", {1, 1, 1100, false}, 0x37d0000000000001ULL, 0x7fefffffffffffffULL, 0, RoundedClass_::NON_FINITE},
            {"E19", {1, 1, 1100, false}, 0xb7cfffffffffffffULL, 0xffefffffffffffffULL, 0xffefffffffffffffULL, RoundedClass_::FINITE},
            {"E20", {1, 1, 1100, false}, 0xb7d0000000000000ULL, 0xffefffffffffffffULL, 0, RoundedClass_::NON_FINITE},
            {"E21", {1, 1, 1100, false}, 0xb7d0000000000001ULL, 0xffefffffffffffffULL, 0, RoundedClass_::NON_FINITE},
        }};
        return rows;
    }

    void AssertCanonicalWorkspace(const BcgScaledAlphaPrivate_::ExactWorkspace_& workspace) {
        const auto assertMagnitude = [](const BcgScaledAlphaPrivate_::ExactMagnitude_& magnitude) {
            ASSERT_EQ(BcgScaledAlphaPrivate_::EXACT_CANDIDATE_LIMB_COUNT_, magnitude.first_);
            ASSERT_EQ(-1, magnitude.last_);
            for (const std::uint32_t limb : magnitude.limbs_)
                ASSERT_EQ(0U, limb);
        };
        assertMagnitude(workspace.positive_);
        assertMagnitude(workspace.negative_);
    }

#if defined(__SSE2__) || defined(_M_X64)
    class MxcsrRestore_ {
        unsigned prior_;

    public:
        MxcsrRestore_() noexcept : prior_(_mm_getcsr()) {}
        ~MxcsrRestore_() noexcept { _mm_setcsr(prior_); }
    };
#endif

    struct AlphaClassifierObservation_ {
        BcgScaledAlphaPrivate_::AlphaPlan_ plan_;
        int legacyConversionCalls_;
        unsigned entryMxcsr_;
        unsigned exitMxcsr_;
        std::array<std::uint64_t, 6> comparableBits_;
    };

    AlphaClassifierObservation_ ObserveAlphaClassification(const BcgScaledAlphaPrivate_::StoredScaledBits_& numerator,
                                                           const BcgScaledAlphaPrivate_::StoredScaledBits_& denominator,
                                                           bool flushToZero) {
        AlphaClassifierObservation_ result{};
#if defined(__SSE2__) || defined(_M_X64)
        MxcsrRestore_ restore;
        const unsigned configured =
            (_mm_getcsr() & ~static_cast<unsigned>(_MM_FLUSH_ZERO_MASK)) | (flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF);
        _mm_setcsr(configured);
        result.entryMxcsr_ = _mm_getcsr();
#else
        (void)flushToZero;
#endif
        result.plan_ =
            BcgScaledAlphaPrivate_::ClassifyAlphaAndInvokeLegacy_(numerator, denominator, [&result]() { ++result.legacyConversionCalls_; });
#if defined(__SSE2__) || defined(_M_X64)
        result.exitMxcsr_ = _mm_getcsr();
#endif
        result.comparableBits_ = {
            result.plan_.exact_.numerator_,
            result.plan_.exact_.denominator_,
            static_cast<std::uint64_t>(static_cast<std::int64_t>(result.plan_.exact_.binaryExponent_)),
            static_cast<std::uint64_t>(result.plan_.exact_.negative_),
            static_cast<std::uint64_t>(result.plan_.path_),
            static_cast<std::uint64_t>(result.legacyConversionCalls_),
        };
        return result;
    }

    void AssertBCGIdentityInitialClassification(const Vector_<>& b, const Vector_<>& requestedResidual, double tolRel, double tolAbs) {
        Vector_<> x = {b[0] - requestedResidual[0], b[1] - requestedResidual[1]};
        const Vector_<> entry = x;
        const Vector_<> residual = DiagonalResidual({1.0, 1.0}, x, b);
        const bool initiallyConverged = CommonExponentConverged(residual, b, tolRel, tolAbs);
        CallbackCounts_ counts;
        HookedDiagonal_ matrix({1.0, 1.0}, &counts);

        Sparse::BCGSolve(matrix, b, tolRel, tolAbs, 4, &x);

        if (initiallyConverged) {
            ASSERT_DOUBLE_EQ(entry[0], x[0]);
            ASSERT_DOUBLE_EQ(entry[1], x[1]);
            ASSERT_EQ(1, counts.left_);
            ASSERT_EQ(0, counts.right_);
        } else {
            ASSERT_DOUBLE_EQ(b[0], x[0]);
            ASSERT_DOUBLE_EQ(b[1], x[1]);
            ASSERT_EQ(3, counts.left_);
            ASSERT_EQ(1, counts.right_);
        }
    }

    void FillDiagonalProduct(const Vector_<>& diagonal, const Vector_<>& x, Vector_<>* product) {
        for (int i = 0; i < static_cast<int>(diagonal.size()); ++i)
            (*product)[i] = diagonal[i] * x[i];
    }

    void
    RunSolver(bool biConjugate, const Sparse::Square_& matrix, const Vector_<>& b, double tolRel, double tolAbs, int maxIterations, Vector_<>* x) {
        const SolveFunction_ solve = biConjugate ? &Sparse::BCGSolve : &Sparse::CGSolve;
        solve(matrix, b, tolRel, tolAbs, maxIterations, x);
    }

    const char* SolverName(bool biConjugate) { return biConjugate ? "BCGSolve" : "CGSolve"; }

    struct ScaledAlphaObservation_ {
        std::uint64_t resultBits_ = 0;
        std::uint64_t directResidualBits_ = 0;
        BcgScaledAlphaPrivate_::CandidateSubject_ evidenceSubject_ = BcgScaledAlphaPrivate_::CandidateSubject_::NONE;
        int evidenceIndex_ = -1;
        int commitCount_ = 0;
        int confirmationCount_ = 0;
        std::uint64_t confirmationInputBits_ = 0x7ff8000000000001ULL;
        std::uint64_t confirmationOutputBits_ = 0x7ff8000000000002ULL;
        CallbackCounts_ counts_;
        std::vector<std::uint64_t> callbackBits_;
    };

    void RecordCallback(std::uint64_t tag, int call, double input, double output, std::vector<std::uint64_t>* callbackBits) {
        callbackBits->push_back(tag);
        callbackBits->push_back(static_cast<std::uint64_t>(call));
        callbackBits->push_back(DoubleBits(input));
        callbackBits->push_back(DoubleBits(output));
    }

    ScaledAlphaObservation_ ObserveScaledAlphaSolve(bool biConjugate,
                                                    double diagonal,
                                                    double preconditionerScale,
                                                    double rhs,
                                                    double initial,
                                                    const BcgScaledAlphaPrivate_::ExactAlpha_* exactAlpha = nullptr) {
        ScaledAlphaObservation_ result;
        double direction = 0.0;
        double residualBase = 0.0;
        double operatorDirection = 0.0;
        double shadowOperatorDirection = 0.0;
        HookedPreconditionedDiagonal_ matrix({diagonal}, &result.counts_);
        matrix.SetLeftHook([diagonal, &result, &operatorDirection](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = diagonal * input[0];
            if (call == 2)
                operatorDirection = (*output)[0];
            if (call == 3) {
                ++result.confirmationCount_;
                result.confirmationInputBits_ = DoubleBits(input[0]);
                result.confirmationOutputBits_ = DoubleBits((*output)[0]);
            }
            RecordCallback(1, call, input[0], (*output)[0], &result.callbackBits_);
            return true;
        });
        matrix.SetRightHook([diagonal, &result, &shadowOperatorDirection](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = diagonal * input[0];
            if (call == 1)
                shadowOperatorDirection = (*output)[0];
            RecordCallback(2, call, input[0], (*output)[0], &result.callbackBits_);
            return true;
        });
        const auto preconditioner = [preconditionerScale, &result, &direction, &residualBase](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = preconditionerScale * input[0];
            direction = (*output)[0];
            residualBase = input[0];
            RecordCallback(3, call, input[0], (*output)[0], &result.callbackBits_);
            return true;
        };
        const auto shadowPreconditioner = [preconditionerScale, &result](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = preconditionerScale * input[0];
            RecordCallback(4, call, input[0], (*output)[0], &result.callbackBits_);
            return true;
        };
        matrix.SetPreconditionerLeftHook(preconditioner);
        matrix.SetPreconditionerRightHook(shadowPreconditioner);
        const Vector_<> b = {rhs};
        Vector_<> x = {initial};
        const double* const entryStorage = &x[0];

        RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 2, &x);

        result.resultBits_ = DoubleBits(x[0]);
        result.directResidualBits_ = DoubleBits(DiagonalResidual({diagonal}, x, b)[0]);
        BcgScaledAlphaPrivate_::CandidateEvidence_ evidence{BcgScaledAlphaPrivate_::CandidateSubject_::NONE, -1};
        if (exactAlpha != nullptr) {
            const Vector_<> directionVector = {direction};
            const Vector_<> xBase = {initial};
            const Vector_<> residualBaseVector = {residualBase};
            const Vector_<> shadowBase = {residualBase};
            Vector_<> xOutput(1);
            Vector_<> residual = {operatorDirection};
            Vector_<> shadow = {shadowOperatorDirection};
            BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
            const BcgScaledAlphaPrivate_::CandidateGroup_ group{&directionVector,
                                                                &xBase,
                                                                &residualBaseVector,
                                                                biConjugate ? &shadowBase : nullptr,
                                                                &xOutput,
                                                                &residual,
                                                                biConjugate ? &shadow : nullptr};
            evidence = BcgScaledAlphaPrivate_::EvaluateCandidateGroup_(*exactAlpha, group, &workspace);
            AssertCanonicalWorkspace(workspace);
        }
        result.evidenceSubject_ = evidence.subject_;
        result.evidenceIndex_ = evidence.firstNonFiniteIndex_;
        result.commitCount_ = &x[0] == entryStorage ? 0 : 1;
        return result;
    }

    struct OrdinaryCandidateObservation_ {
        std::uint64_t coefficientBits_ = 0;
        std::array<std::uint64_t, 2> directionBits_{};
        std::array<std::uint64_t, 2> operatorDirectionBits_{};
        std::array<std::uint64_t, 2> xCandidateBits_{};
        std::array<std::uint64_t, 2> residualCandidateBits_{};
        std::array<std::uint64_t, 2> shadowCandidateBits_{};
        std::array<std::uint64_t, 2> finalBits_{};
        CallbackCounts_ counts_;
        int commitCount_ = 0;
        bool callbackException_ = false;
    };

    OrdinaryCandidateObservation_ ObserveOrdinaryCandidateCommit(bool biConjugate) {
        OrdinaryCandidateObservation_ result;
        HookedPreconditionedDiagonal_ matrix({1.0, 2.25}, &result.counts_);
        matrix.SetLeftHook([&result](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = input[0];
            (*output)[1] = 2.25 * input[1];
            if (call == 2) {
                result.directionBits_ = {DoubleBits(input[0]), DoubleBits(input[1])};
                result.operatorDirectionBits_ = {DoubleBits((*output)[0]), DoubleBits((*output)[1])};
            }
            return true;
        });
        matrix.SetRightHook([](int, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = input[0];
            (*output)[1] = 2.25 * input[1];
            return true;
        });
        matrix.SetPreconditionerLeftHook([biConjugate, &result](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = input[0];
            (*output)[1] = input[1];
            if (call == 2) {
                result.residualCandidateBits_ = {DoubleBits(input[0]), DoubleBits(input[1])};
                if (!biConjugate)
                    throw CallbackSentinel_();
            }
            return true;
        });
        matrix.SetPreconditionerRightHook([&result](int call, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = input[0];
            (*output)[1] = input[1];
            if (call == 2) {
                result.shadowCandidateBits_ = {DoubleBits(input[0]), DoubleBits(input[1])};
                throw CallbackSentinel_();
            }
            return true;
        });
        const Vector_<> b = {1.0, 2.0};
        Vector_<> x = {0.0, 0.0};
        const double* const entryStorage = &x[0];
        try {
            RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 3, &x);
        } catch (const CallbackSentinel_&) {
            result.callbackException_ = true;
        }
        result.commitCount_ = &x[0] == entryStorage ? 0 : 1;
        result.xCandidateBits_ = {DoubleBits(x[0]), DoubleBits(x[1])};
        result.finalBits_ = result.xCandidateBits_;
        result.coefficientBits_ = result.xCandidateBits_[0];
        return result;
    }

#if defined(__SSE2__) || defined(_M_X64)
    struct OrdinaryFpStatusObservation_ {
        unsigned entryStatus_;
        unsigned exitStatus_;
        std::uint64_t resultBits_;
        int commitCount_;
        std::string failureMessage_;
    };

    OrdinaryFpStatusObservation_ ObserveOrdinaryFpStatus(bool biConjugate, unsigned entryStatus, bool overflowCandidate) {
        MxcsrRestore_ restore;
        const unsigned statusMask = _MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW;
        _mm_setcsr((_mm_getcsr() & ~statusMask) | entryStatus);
        OrdinaryFpStatusObservation_ result{_mm_getcsr(), 0, 0, 0, {}};
        CallbackCounts_ counts;
        const double diagonal = overflowCandidate ? std::ldexp(1.0, -1000) : 2.0;
        HookedPreconditionedDiagonal_ matrix({diagonal}, &counts);
        const auto preconditioner = [](int, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = input[0];
            return true;
        };
        matrix.SetPreconditionerLeftHook(preconditioner);
        matrix.SetPreconditionerRightHook(preconditioner);
        const Vector_<> b = {overflowCandidate ? std::ldexp(1.0, 100) : 6.0};
        Vector_<> x = {0.0};
        const double* const entryStorage = &x[0];
        try {
            RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 2, &x);
        } catch (const Exception_& exception) {
            result.failureMessage_ = exception.what();
        }
        result.exitStatus_ = _mm_getcsr();
        result.resultBits_ = DoubleBits(x[0]);
        result.commitCount_ = &x[0] == entryStorage ? 0 : 1;
        return result;
    }
#endif

    struct AllocationObservation_ {
        std::size_t count_;
        std::uint64_t resultBits_;
    };

    AllocationObservation_ ObserveSolveAllocations(bool biConjugate, double diagonal, double preconditionerScale, double rhs) {
        CallbackCounts_ counts;
        HookedPreconditionedDiagonal_ matrix({diagonal}, &counts);
        const auto preconditioner = [preconditionerScale](int, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = preconditionerScale * input[0];
            return true;
        };
        matrix.SetPreconditionerLeftHook(preconditioner);
        matrix.SetPreconditionerRightHook(preconditioner);
        const Vector_<> b = {rhs};
        Vector_<> x = {0.0};

        AllocationScope_ allocations;
        RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 2, &x);
        const std::size_t count = allocations.Finish();
        return {count, DoubleBits(x[0])};
    }

    void AssertScaledAlphaSolve(bool biConjugate, double diagonal, double preconditionerScale, double rhs, std::uint64_t expectedBits) {
        const ScaledAlphaObservation_ observation = ObserveScaledAlphaSolve(biConjugate, diagonal, preconditionerScale, rhs, 0.0);
        ASSERT_EQ(expectedBits, observation.resultBits_);
        ASSERT_EQ(0x0000000000000000ULL, observation.directResidualBits_);
        ASSERT_EQ(3, observation.counts_.left_);
        ASSERT_EQ(biConjugate ? 1 : 0, observation.counts_.right_);
        ASSERT_EQ(1, observation.counts_.preconditionerLeft_);
        ASSERT_EQ(biConjugate ? 1 : 0, observation.counts_.preconditionerRight_);
    }

    void AssertDalExceptionContains(const std::function<void()>& call, std::initializer_list<const char*> tokens) {
        bool caught = false;
        try {
            call();
        } catch (const Exception_& exception) {
            caught = true;
            const std::string message = exception.what();
            for (const char* token : tokens)
                ASSERT_NE(std::string::npos, message.find(token)) << message;
        }
        ASSERT_TRUE(caught);
    }

    bool IsPreconditionerFaultSite(int site) { return site == 3 || site == 4; }

    std::unique_ptr<HookedDiagonal_> NewFaultMatrix(int site, CallbackCounts_* counts) {
        if (IsPreconditionerFaultSite(site))
            return std::make_unique<HookedPreconditionedDiagonal_>(Vector_<>{1.0, 2.0}, counts);
        return std::make_unique<HookedDiagonal_>(Vector_<>{1.0, 2.0}, counts);
    }

    bool ApplyOperatorFault(int fault, int badSize, const Vector_<>& diagonal, const Vector_<>& input, Vector_<>* output) {
        if (fault == 0) {
            output->Resize(badSize);
            return true;
        }
        if (fault == 1) {
            FillDiagonalProduct(diagonal, input, output);
            (*output)[1] = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        throw CallbackSentinel_();
    }

    bool ApplyPreconditionerFault(int fault, int badSize, const Vector_<>& input, Vector_<>* output) {
        if (fault == 0) {
            output->Resize(badSize);
            return true;
        }
        if (fault == 1) {
            for (int i = 0; i < static_cast<int>(input.size()); ++i)
                (*output)[i] = input[i];
            (*output)[1] = std::numeric_limits<double>::infinity();
            return true;
        }
        throw CallbackSentinel_();
    }

    bool ApplyFaultOnCall(const CallbackHook_& fault, int targetCall, int call, const Vector_<>& input, Vector_<>* output) {
        if (call != targetCall)
            return false;
        return fault(call, input, output);
    }

    void ConfigureCallbackFault(HookedDiagonal_* matrix, int site, const CallbackHook_& operatorFault, const CallbackHook_& preconditionerFault) {
        if (site == 0)
            matrix->SetLeftHook([operatorFault](int call, const Vector_<>& input, Vector_<>* output) {
                return ApplyFaultOnCall(operatorFault, 1, call, input, output);
            });
        else if (site == 1)
            matrix->SetLeftHook([operatorFault](int call, const Vector_<>& input, Vector_<>* output) {
                return ApplyFaultOnCall(operatorFault, 2, call, input, output);
            });
        else if (site == 2)
            matrix->SetRightHook([operatorFault](int call, const Vector_<>& input, Vector_<>* output) {
                return ApplyFaultOnCall(operatorFault, 1, call, input, output);
            });
        else if (site == 3)
            dynamic_cast<HookedPreconditionedDiagonal_*>(matrix)->SetPreconditionerLeftHook(
                [preconditionerFault](int call, const Vector_<>& input, Vector_<>* output) {
                    return ApplyFaultOnCall(preconditionerFault, 1, call, input, output);
                });
        else if (site == 4)
            dynamic_cast<HookedPreconditionedDiagonal_*>(matrix)->SetPreconditionerRightHook(
                [preconditionerFault](int call, const Vector_<>& input, Vector_<>* output) {
                    return ApplyFaultOnCall(preconditionerFault, 1, call, input, output);
                });
        else
            matrix->SetLeftHook([operatorFault](int call, const Vector_<>& input, Vector_<>* output) {
                return ApplyFaultOnCall(operatorFault, 4, call, input, output);
            });
    }

    const char* CallbackFaultCategory(int site, int fault) {
        if (fault == 0)
            return IsPreconditionerFaultSite(site) ? "invalid preconditioner result" : "invalid operator result";
        return IsPreconditionerFaultSite(site) ? "non-finite preconditioner result" : "non-finite operator result";
    }

    const char* CallbackFaultName(int site) {
        if (site == 2)
            return "MultiplyRight";
        if (site == 3)
            return "PreConditionerSolveLeft";
        if (site == 4)
            return "PreConditionerSolveRight";
        return "MultiplyLeft";
    }

    void AssertCallbackFaultResult(bool biConjugate, int site, int fault, const std::function<void()>& solve, const Vector_<>& x) {
        if (fault == 2) {
            ASSERT_THROW(solve(), CallbackSentinel_);
        } else {
            const char* category = CallbackFaultCategory(site, fault);
            const char* callback = CallbackFaultName(site);
            if (fault == 0)
                AssertDalExceptionContains(solve, {SolverName(biConjugate), category, callback});
            else
                AssertDalExceptionContains(solve, {SolverName(biConjugate), category, callback, "1"});
        }

        if (site == 5) {
            ASSERT_DOUBLE_EQ(2.0 / 3.0, x[0]);
            ASSERT_DOUBLE_EQ(2.0 / 3.0, x[1]);
        } else {
            ASSERT_DOUBLE_EQ(0.0, x[0]);
            ASSERT_DOUBLE_EQ(0.0, x[1]);
        }
    }

    CallbackCounts_ ExpectedCallbackFaultCounts(bool biConjugate, int site) {
        const std::array<CallbackCounts_, 6> cgExpected = {CallbackCounts_{1, 0, 0, 0}, CallbackCounts_{2, 0, 0, 0}, CallbackCounts_{0, 0, 0, 0},
                                                           CallbackCounts_{1, 0, 1, 0}, CallbackCounts_{0, 0, 0, 0}, CallbackCounts_{4, 0, 0, 0}};
        const std::array<CallbackCounts_, 6> bcgExpected = {CallbackCounts_{1, 0, 0, 0}, CallbackCounts_{2, 0, 0, 0}, CallbackCounts_{2, 1, 0, 0},
                                                            CallbackCounts_{1, 0, 1, 0}, CallbackCounts_{1, 0, 1, 1}, CallbackCounts_{4, 2, 0, 0}};
        return biConjugate ? bcgExpected[site] : cgExpected[site];
    }

    void AssertCallbackCounts(const CallbackCounts_& expected, const CallbackCounts_& actual) {
        ASSERT_EQ(expected.left_, actual.left_);
        ASSERT_EQ(expected.right_, actual.right_);
        ASSERT_EQ(expected.preconditionerLeft_, actual.preconditionerLeft_);
        ASSERT_EQ(expected.preconditionerRight_, actual.preconditionerRight_);
    }

    void AssertCallbackFault(bool biConjugate, int site, int fault, int badSize = 0) {
        SCOPED_TRACE(std::string(SolverName(biConjugate)) + " site=" + std::to_string(site) + " fault=" + std::to_string(fault));
        CallbackCounts_ counts;
        std::unique_ptr<HookedDiagonal_> matrix = NewFaultMatrix(site, &counts);
        const Vector_<> diagonal = {1.0, 2.0};
        const CallbackHook_ operatorFault = [fault, badSize, &diagonal](int, const Vector_<>& input, Vector_<>* output) {
            return ApplyOperatorFault(fault, badSize, diagonal, input, output);
        };
        const CallbackHook_ preconditionerFault = [fault, badSize](int, const Vector_<>& input, Vector_<>* output) {
            return ApplyPreconditionerFault(fault, badSize, input, output);
        };
        ConfigureCallbackFault(matrix.get(), site, operatorFault, preconditionerFault);

        const Vector_<> b = {1.0, 1.0};
        Vector_<> x = {0.0, 0.0};
        const auto solve = [&]() { RunSolver(biConjugate, *matrix, b, 1e-12, 0.0, 10, &x); };
        AssertCallbackFaultResult(biConjugate, site, fault, solve, x);
        AssertCallbackCounts(ExpectedCallbackFaultCounts(biConjugate, site), counts);
    }
} // namespace

TEST(MatrixTest, TestCGSolveAndBCGSolveLargeFiniteRhs) {
    Sparse::TriDiagonal_ matrix(1);
    matrix.Set(0, 0, 1.0);
    const Vector_<> b = {1e308};
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        Vector_<> x = {0.0};
        RunSolver(biConjugate, matrix, b, EPSILON, 0.0, 10, &x);
        const Vector_<> residual = DiagonalResidual({1.0}, x, b);
        ASSERT_TRUE(std::isfinite(x[0]));
        ASSERT_DOUBLE_EQ(1e308, x[0]);
        ASSERT_TRUE(CommonExponentConverged(residual, b, EPSILON, 0.0));
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveStableFirstUpdateCancellation) {
    ASSERT_TRUE(std::isinf(2.0 * 1e308));
    ASSERT_DOUBLE_EQ(1e308, std::fma(2.0, 1e308, -1e308));

    Sparse::TriDiagonal_ matrix(1);
    matrix.Set(0, 0, 0.5);
    const Vector_<> b = {5e307};
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        Vector_<> x = {-1e308};
        RunSolver(biConjugate, matrix, b, EPSILON, 0.0, 10, &x);
        const Vector_<> residual = DiagonalResidual({0.5}, x, b);
        ASSERT_TRUE(std::isfinite(x[0]));
        ASSERT_DOUBLE_EQ(1e308, x[0]);
        ASSERT_TRUE(CommonExponentConverged(residual, b, EPSILON, 0.0));
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveStableSecondDirectionCancellation) {
    const double scale = 3e306;
    ASSERT_TRUE(std::isinf(2.4 * 27.0 * scale));

    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        CallbackCounts_ counts;
        HookedPreconditionedDiagonal_ matrix({1.0, 1.0}, &counts);
        auto scaleSafePreconditioner = [](int, const Vector_<>& input, Vector_<>* output) {
            const double commonScale = std::max(std::fabs(input[0]), std::fabs(input[1]));
            if (commonScale == 0.0) {
                (*output)[0] = 0.0;
                (*output)[1] = 0.0;
            } else {
                const double first = input[0] / commonScale;
                const double second = input[1] / commonScale;
                (*output)[0] = commonScale * (8.0 * first - 9.0 * second);
                (*output)[1] = commonScale * (-9.0 * first + 12.0 * second);
            }
            return true;
        };
        matrix.SetPreconditionerLeftHook(scaleSafePreconditioner);
        matrix.SetPreconditionerRightHook(scaleSafePreconditioner);

        const Vector_<> b = {9.0 * scale, 9.0 * scale};
        Vector_<> x = {0.0, 0.0};
        RunSolver(biConjugate, matrix, b, EPSILON, 0.0, 10, &x);

        const Vector_<> residual = DiagonalResidual({1.0, 1.0}, x, b);
        ASSERT_NEAR(9.0 * scale, x[0], 9.0 * scale * 1e-14);
        ASSERT_NEAR(9.0 * scale, x[1], 9.0 * scale * 1e-14);
        ASSERT_TRUE(CommonExponentConverged(residual, b, EPSILON, 0.0));
        ASSERT_EQ(2, counts.preconditionerLeft_);
        ASSERT_EQ(biConjugate ? 2 : 0, counts.preconditionerRight_);
    }
}

TEST(MatrixTest, TestBCGSolvePreservesSignedDotCancellation) {
    const double large = std::ldexp(1.0, 60);
    double sequentialDot = 0.0;
    for (const double term : {large, 1.0, -large})
        sequentialDot += term;
    ASSERT_DOUBLE_EQ(0.0, sequentialDot);

    CallbackCounts_ counts;
    const Vector_<> diagonal = {std::ldexp(1.0, -60), 1.0, -std::ldexp(1.0, -60)};
    HookedPreconditionedDiagonal_ matrix(diagonal, &counts);
    const auto exactPreconditioner = [large](int, const Vector_<>& input, Vector_<>* output) {
        (*output)[0] = large * input[0];
        (*output)[1] = input[1];
        (*output)[2] = -large * input[2];
        return true;
    };
    matrix.SetPreconditionerLeftHook(exactPreconditioner);
    matrix.SetPreconditionerRightHook(exactPreconditioner);
    const Vector_<> b = {1.0, 1.0, 1.0};
    Vector_<> x = {0.0, 0.0, 0.0};

    Sparse::BCGSolve(matrix, b, EPSILON, 0.0, 10, &x);

    ASSERT_DOUBLE_EQ(large, x[0]);
    ASSERT_DOUBLE_EQ(1.0, x[1]);
    ASSERT_DOUBLE_EQ(-large, x[2]);
    ASSERT_TRUE(CommonExponentConverged(DiagonalResidual(diagonal, x, b), b, EPSILON, 0.0));
    ASSERT_EQ(3, counts.left_);
    ASSERT_EQ(1, counts.right_);
    ASSERT_EQ(1, counts.preconditionerLeft_);
    ASSERT_EQ(1, counts.preconditionerRight_);
}

namespace {
    bool BuildFiniteNonzeroPermutationDiagonal(const std::array<double, 4>& terms, Vector_<>* diagonal) {
        double sequentialBeta = 0.0;
        double sequentialDenominator = 0.0;
        for (int i = 0; i < static_cast<int>(terms.size()); ++i) {
            (*diagonal)[i] = 3.0 * terms[i];
            sequentialBeta += terms[i];
            sequentialDenominator += (*diagonal)[i];
        }
        return std::isfinite(sequentialBeta) && sequentialBeta != 0.0 && std::isfinite(sequentialDenominator) && sequentialDenominator != 0.0;
    }

    void AssertAllEqual(const Vector_<>& values, double expected) {
        for (const double value : values)
            ASSERT_DOUBLE_EQ(expected, value);
    }

    void AssertFiniteNonzeroSignedDotPermutation(const std::array<double, 4>& terms, int permutation, int* finiteNonzeroCases) {
        SCOPED_TRACE("permutation=" + std::to_string(permutation));
        const Vector_<> b(terms.begin(), terms.end());
        Vector_<> diagonal(b.size());
        if (BuildFiniteNonzeroPermutationDiagonal(terms, &diagonal))
            ++*finiteNonzeroCases;

        CallbackCounts_ counts;
        HookedPreconditionedDiagonal_ matrix(diagonal, &counts);
        const auto preconditioner = [b](int, const Vector_<>& input, Vector_<>* output) {
            for (int i = 0; i < static_cast<int>(input.size()); ++i)
                (*output)[i] = input[i] / b[i];
            return true;
        };
        matrix.SetPreconditionerLeftHook(preconditioner);
        matrix.SetPreconditionerRightHook(preconditioner);
        Vector_<> x(b.size(), 0.0);

        Sparse::BCGSolve(matrix, b, EPSILON, 0.0, 1, &x);

        AssertAllEqual(x, 1.0 / 3.0);
        ASSERT_EQ(3, counts.left_);
        ASSERT_EQ(1, counts.right_);
        ASSERT_EQ(1, counts.preconditionerLeft_);
        ASSERT_EQ(1, counts.preconditionerRight_);
    }
} // namespace

TEST(MatrixTest, TestBCGSolvePreservesFiniteNonzeroSignedDotCancellationPermutations) {
    const double large = std::ldexp(1.0, 60);
    std::array<double, 4> terms = {large, 100.0, -large, 2.0};
    std::sort(terms.begin(), terms.end());
    int permutation = 0;
    int finiteNonzeroCases = 0;
    do {
        AssertFiniteNonzeroSignedDotPermutation(terms, permutation++, &finiteNonzeroCases);
    } while (std::next_permutation(terms.begin(), terms.end()));
    ASSERT_EQ(24, permutation);
    ASSERT_EQ(18, finiteNonzeroCases);
}

TEST(MatrixTest, TestCGSolveAndBCGSolveDirectConfirmationCounts) {
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        CallbackCounts_ counts;
        HookedDiagonal_ matrix({1.0, 2.0}, &counts);
        Vector_<> terminalInput(2);
        matrix.SetLeftHook([&terminalInput](int call, const Vector_<>& input, Vector_<>*) {
            if (call != 4)
                return false;
            terminalInput[0] = input[0];
            terminalInput[1] = input[1];
            return false;
        });
        const Vector_<> b = {1.0, 1.0};
        Vector_<> x = {0.0, 0.0};

        RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x);

        ASSERT_EQ(4, counts.left_);
        ASSERT_EQ(biConjugate ? 2 : 0, counts.right_);
        ASSERT_DOUBLE_EQ(1.0, terminalInput[0]);
        ASSERT_DOUBLE_EQ(0.5, terminalInput[1]);
        ASSERT_DOUBLE_EQ(1.0, x[0]);
        ASSERT_DOUBLE_EQ(0.5, x[1]);
        ASSERT_TRUE(CommonExponentConverged(DiagonalResidual({1.0, 2.0}, x, b), b, 1e-12, 0.0));
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveInitialAndExhaustionCounts) {
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        {
            CallbackCounts_ counts;
            HookedPreconditionedDiagonal_ matrix({1.0, 2.0}, &counts);
            const Vector_<> b = {1.0, 1.0};
            Vector_<> x = {1.0, 0.5};
            RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x);
            ASSERT_EQ(1, counts.left_);
            ASSERT_EQ(0, counts.right_);
            ASSERT_EQ(0, counts.preconditionerLeft_);
            ASSERT_EQ(0, counts.preconditionerRight_);
        }
        {
            CallbackCounts_ counts;
            HookedPreconditionedDiagonal_ matrix({1.0, 2.0}, &counts);
            const Vector_<> b = {1.0, 1.0};
            Vector_<> x = {0.0, 0.0};
            AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 1, &x); },
                                       {biConjugate ? "Exhausted iterations in BCGSolve" : "Exhausted iterations in CGSolve"});
            ASSERT_EQ(2, counts.left_);
            ASSERT_EQ(biConjugate ? 1 : 0, counts.right_);
            ASSERT_EQ(1, counts.preconditionerLeft_);
            ASSERT_EQ(biConjugate ? 1 : 0, counts.preconditionerRight_);
            ASSERT_DOUBLE_EQ(2.0 / 3.0, x[0]);
            ASSERT_DOUBLE_EQ(2.0 / 3.0, x[1]);
        }
    }

    CallbackCounts_ counts;
    HookedPreconditionedDiagonal_ matrix({1.0, 2.0}, &counts);
    const Vector_<> b = {1e-12, 0.0};
    Vector_<> x = {0.0, 0.0};
    RunSolver(true, matrix, b, 0.0, 1e-10, 10, &x);
    ASSERT_EQ(1, counts.left_);
    ASSERT_EQ(0, counts.right_);
    ASSERT_EQ(0, counts.preconditionerLeft_);
    ASSERT_EQ(0, counts.preconditionerRight_);
    ASSERT_DOUBLE_EQ(0.0, x[0]);
    ASSERT_DOUBLE_EQ(0.0, x[1]);
}

TEST(MatrixTest, TestCGSolveAndBCGSolveRejectDirectResidualMismatchWithoutCommit) {
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        CallbackCounts_ counts;
        HookedDiagonal_ matrix({1.0, 2.0}, &counts);
        matrix.SetLeftHook([](int call, const Vector_<>&, Vector_<>* output) {
            if (call != 4)
                return false;
            (*output)[0] = 0.0;
            (*output)[1] = 0.0;
            return true;
        });
        const Vector_<> b = {1.0, 1.0};
        Vector_<> x = {0.0, 0.0};

        AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x); },
                                   {SolverName(biConjugate), "numerical breakdown", "direct residual confirmation"});

        ASSERT_EQ(4, counts.left_);
        ASSERT_EQ(biConjugate ? 2 : 0, counts.right_);
        ASSERT_DOUBLE_EQ(2.0 / 3.0, x[0]);
        ASSERT_DOUBLE_EQ(2.0 / 3.0, x[1]);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveProtectCommitOnConfirmationFailure) {
    for (const bool biConjugate : {false, true}) {
        AssertCallbackFault(biConjugate, 5, 0, 1);
        AssertCallbackFault(biConjugate, 5, 1);
        AssertCallbackFault(biConjugate, 5, 2);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveCommonExponentBoundary) {
    const Vector_<> large = {1.3e308, 1.3e308};
    const Vector_<> wideResidual = {1.0, std::numeric_limits<double>::denorm_min()};
    const Vector_<> wideRhs = {1.0, 0.0};
    ASSERT_FALSE(CommonExponentConverged(wideResidual, wideRhs, 1.0, 0.0));
    ASSERT_TRUE(std::isinf(std::hypot(large[0], large[1])));
    ASSERT_FALSE(CommonExponentConverged(large, large, std::nextafter(1.0, 0.0), 0.0));
    ASSERT_TRUE(CommonExponentConverged(large, large, 1.0, 0.0));
    ASSERT_TRUE(CommonExponentConverged(large, large, std::nextafter(1.0, 2.0), 0.0));

    Sparse::TriDiagonal_ zero(2);
    for (const double tolRel : {std::nextafter(1.0, 0.0), 1.0, std::nextafter(1.0, 2.0)}) {
        Vector_<> x = {0.0, 0.0};
        if (tolRel < 1.0)
            AssertDalExceptionContains([&]() { Sparse::BCGSolve(zero, large, tolRel, 0.0, 10, &x); }, {"BCGSolve", "numerical breakdown"});
        else
            Sparse::BCGSolve(zero, large, tolRel, 0.0, 10, &x);
        ASSERT_DOUBLE_EQ(0.0, x[0]);
        ASSERT_DOUBLE_EQ(0.0, x[1]);
    }
}

TEST(MatrixTest, TestBCGSolvePreservesWideExponentResidualContribution) {
    CallbackCounts_ counts;
    HookedDiagonal_ matrix({1.0, 1.0}, &counts);
    const Vector_<> b = {1.0, 0.0};
    Vector_<> x = {0.0, -std::numeric_limits<double>::denorm_min()};
    const Vector_<> initialResidual = {1.0, std::numeric_limits<double>::denorm_min()};
    ASSERT_FALSE(CommonExponentConverged(initialResidual, b, 1.0, 0.0));

    Sparse::BCGSolve(matrix, b, 1.0, 0.0, 10, &x);

    ASSERT_DOUBLE_EQ(1.0, x[0]);
    ASSERT_DOUBLE_EQ(0.0, x[1]);
    ASSERT_EQ(3, counts.left_);
    ASSERT_EQ(1, counts.right_);
}

TEST(MatrixTest, TestBCGSolveDoesNotRoundSubnormalThresholdIntoConvergence) {
    const double denorm = std::numeric_limits<double>::denorm_min();
    const auto assertContinues = [denorm](const Vector_<>& b, Vector_<> x, double tolRel, double tolAbs) {
        const Vector_<> initialResidual = {b[0] - x[0], b[1] - x[1]};
        ASSERT_FALSE(CommonExponentConverged(initialResidual, b, tolRel, tolAbs));

        CallbackCounts_ counts;
        HookedDiagonal_ matrix({1.0, 1.0}, &counts);
        Sparse::BCGSolve(matrix, b, tolRel, tolAbs, 10, &x);

        ASSERT_DOUBLE_EQ(b[0], x[0]);
        ASSERT_DOUBLE_EQ(b[1], x[1]);
        ASSERT_EQ(3, counts.left_);
        ASSERT_EQ(1, counts.right_);
    };

    for (const int exponent : {-1074, -1030, -1029, -1028}) {
        SCOPED_TRACE(exponent);
        const double primary = std::scalbn(1.0, exponent);
        assertContinues({primary, 0.0}, {0.0, -denorm}, 1.0, 0.0);
    }

    const double boundary = std::scalbn(1.0, -1029);
    assertContinues({0.0, 0.0}, {-boundary, -denorm}, 1.0, boundary);
    assertContinues({boundary, 0.0}, {0.0, -denorm}, 0.5, 0.5 * boundary);
}

#if defined(__AVX2__) && defined(__FMA__)
TEST(MatrixTest, TestBCGSolveNativeDirectResidualPreservesNonPowerSubnormalNorm) {
    constexpr double primary = 0x1.02cc22b489eadp-537;
    constexpr double secondary = 0x1.02cc22b489eadp-557;
    AssertBCGIdentityInitialClassification({primary, 0.0}, {primary, secondary}, 1.0, 0.0);
}
#endif

TEST(MatrixTest, TestBCGSolveFixedSeedNonPowerSubnormalClassification) {
    constexpr std::uint64_t fractionMask = (1ULL << 52) - 1;
    constexpr std::uint64_t exponentBits = 486ULL << 52;
    std::uint64_t state = 0x6d5a56da3c9ef187ULL;

    for (int candidate = 0; candidate < 128; ++candidate) {
        SCOPED_TRACE(candidate);
        const double primary = DoubleFromBits(exponentBits | ((SplitMix64(&state) & fractionMask) | 1ULL));
        const double secondary = std::scalbn(primary, -20);
        const double below = std::nextafter(primary, 0.0);
        const double half = 0.5 * primary;
        const double mixedAbsolute = 0.625 * primary;

        AssertBCGIdentityInitialClassification({primary, 0.0}, {primary, 0.0}, 1.0, 0.0);
        AssertBCGIdentityInitialClassification({primary, 0.0}, {primary, secondary}, 1.0, 0.0);
        AssertBCGIdentityInitialClassification({primary, 0.0}, {below, 0.0}, 1.0, 0.0);
        AssertBCGIdentityInitialClassification({primary, 0.0}, {half, 0.0}, 1.0, 0.0);
        AssertBCGIdentityInitialClassification({0.0, 0.0}, {primary, 0.0}, 1.0, primary);
        AssertBCGIdentityInitialClassification({0.0, 0.0}, {primary, secondary}, 1.0, primary);
        AssertBCGIdentityInitialClassification({primary, 0.0}, {below, 0.0}, 0.375, mixedAbsolute);
        AssertBCGIdentityInitialClassification({primary, 0.0}, {primary, secondary}, 0.375, mixedAbsolute);
        AssertBCGIdentityInitialClassification({0.0, 0.0}, {primary, 0.0}, 1.0, 0.0);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveCommonExponentOracleHandlesZeroThreshold) { ASSERT_FALSE(CommonExponentConverged({1.0}, {0.0}, 1.0, 0.0)); }

TEST(MatrixTest, TestCGSolveAndBCGSolveRetainLargeInitialToleranceBehavior) {
    Sparse::TriDiagonal_ matrix(1);
    matrix.Set(0, 0, 1.0);
    const Vector_<> b = {1e308};
    Vector_<> x = {std::nextafter(1e308, 0.0)};
    const double initial = x[0];

    Sparse::BCGSolve(matrix, b, EPSILON, 0.0, 10, &x);

    ASSERT_DOUBLE_EQ(initial, x[0]);
    ASSERT_TRUE(CommonExponentConverged(DiagonalResidual({1.0}, x, b), b, EPSILON, 0.0));
}

TEST(MatrixTest, TestCGSolveAndBCGSolveValidateCallbackShapes) {
    for (const int badSize : {1, 3}) {
        for (const bool biConjugate : {false, true}) {
            AssertCallbackFault(biConjugate, 0, 0, badSize);
            AssertCallbackFault(biConjugate, 1, 0, badSize);
            AssertCallbackFault(biConjugate, 3, 0, badSize);
            AssertCallbackFault(biConjugate, 5, 0, badSize);
        }
        AssertCallbackFault(true, 2, 0, badSize);
        AssertCallbackFault(true, 4, 0, badSize);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveValidateCallbackFiniteness) {
    for (const bool biConjugate : {false, true}) {
        AssertCallbackFault(biConjugate, 0, 1);
        AssertCallbackFault(biConjugate, 1, 1);
        AssertCallbackFault(biConjugate, 3, 1);
        AssertCallbackFault(biConjugate, 5, 1);
    }
    AssertCallbackFault(true, 2, 1);
    AssertCallbackFault(true, 4, 1);
}

TEST(MatrixTest, TestCGSolveAndBCGSolvePropagateCallbackExceptions) {
    for (const bool biConjugate : {false, true}) {
        AssertCallbackFault(biConjugate, 0, 2);
        AssertCallbackFault(biConjugate, 1, 2);
        AssertCallbackFault(biConjugate, 3, 2);
        AssertCallbackFault(biConjugate, 5, 2);
    }
    AssertCallbackFault(true, 2, 2);
    AssertCallbackFault(true, 4, 2);
}

TEST(MatrixTest, TestCGSolveAndBCGSolveRejectNonFiniteInputsBeforeCallbacks) {
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        const std::array<Vector_<>, 2> invalidB = {Vector_<>{1.0, std::numeric_limits<double>::quiet_NaN()},
                                                   Vector_<>{1.0, std::numeric_limits<double>::infinity()}};
        for (const Vector_<>& b : invalidB) {
            CallbackCounts_ counts;
            HookedDiagonal_ matrix({1.0, 2.0}, &counts);
            Vector_<> x = {0.0, 0.0};
            AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x); },
                                       {SolverName(biConjugate), "non-finite input", "b", "1"});
            ASSERT_EQ(0, counts.left_);
            ASSERT_DOUBLE_EQ(0.0, x[0]);
            ASSERT_DOUBLE_EQ(0.0, x[1]);
        }
        {
            CallbackCounts_ counts;
            HookedDiagonal_ matrix({1.0, 2.0}, &counts);
            const Vector_<> b = {1.0, 1.0};
            Vector_<> x = {std::numeric_limits<double>::infinity(), 0.0};
            AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x); },
                                       {SolverName(biConjugate), "non-finite input", "x", "0"});
            ASSERT_EQ(0, counts.left_);
            ASSERT_TRUE(std::isinf(x[0]));
            ASSERT_DOUBLE_EQ(0.0, x[1]);
        }
        for (const std::pair<double, const char*> tolerance : {std::pair<double, const char*>{std::numeric_limits<double>::quiet_NaN(), "tolRel"},
                                                               std::pair<double, const char*>{std::numeric_limits<double>::infinity(), "tolAbs"}}) {
            CallbackCounts_ counts;
            HookedDiagonal_ matrix({1.0, 2.0}, &counts);
            const Vector_<> b = {1.0, 1.0};
            Vector_<> x = {0.0, 0.0};
            const double tolRel = std::string(tolerance.second) == "tolRel" ? tolerance.first : 1e-12;
            const double tolAbs = std::string(tolerance.second) == "tolAbs" ? tolerance.first : 0.0;
            AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, tolRel, tolAbs, 10, &x); },
                                       {SolverName(biConjugate), "non-finite input", tolerance.second});
            ASSERT_EQ(0, counts.left_);
        }
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolvePreserveGenericValidationMessages) {
    Sparse::TriDiagonal_ matrix(2);
    const Vector_<> b = {1.0, 1.0};
    for (const bool biConjugate : {false, true}) {
        {
            Vector_<> x = {0.0};
            AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 10, &x); }, {"matrix dimensions are incompatible"});
        }
        for (const std::array<double, 3> parameters :
             {std::array<double, 3>{-1.0, 0.0, 10.0}, std::array<double, 3>{0.0, -1.0, 10.0}, std::array<double, 3>{1e-12, 0.0, 0.0}}) {
            Vector_<> x = {0.0, 0.0};
            AssertDalExceptionContains(
                [&]() { RunSolver(biConjugate, matrix, b, parameters[0], parameters[1], static_cast<int>(parameters[2]), &x); },
                {"parameters are invalid"});
        }
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveClassifyZeroDenominatorBreakdown) {
    Sparse::TriDiagonal_ zero(1);
    const Vector_<> b = {1.0};
    for (const bool biConjugate : {false, true}) {
        Vector_<> x = {0.0};
        AssertDalExceptionContains([&]() { RunSolver(biConjugate, zero, b, EPSILON, 0.0, 10, &x); },
                                   {SolverName(biConjugate), "numerical breakdown", "alpha denominator"});
        ASSERT_DOUBLE_EQ(0.0, x[0]);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveRemainScaleMetamorphic) {
    const double scale = std::ldexp(1.0, 500);
    const Vector_<> diagonal = {2.0, 3.0};
    Sparse::TriDiagonal_ matrix(2);
    matrix.Set(0, 0, diagonal[0]);
    matrix.Set(1, 1, diagonal[1]);

    for (const bool biConjugate : {false, true}) {
        Vector_<> unitX = {0.0, 0.0};
        const Vector_<> unitB = {2.0, 6.0};
        RunSolver(biConjugate, matrix, unitB, 1e-12, 0.0, 10, &unitX);

        Vector_<> scaledX = {0.0, 0.0};
        const Vector_<> scaledB = {2.0 * scale, 6.0 * scale};
        RunSolver(biConjugate, matrix, scaledB, 1e-12, 0.0, 10, &scaledX);

        ASSERT_NEAR(scale * unitX[0], scaledX[0], scale * 1e-12);
        ASSERT_NEAR(scale * unitX[1], scaledX[1], scale * 1e-12);
        ASSERT_TRUE(CommonExponentConverged(DiagonalResidual(diagonal, scaledX, scaledB), scaledB, 1e-12, 0.0));
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolvePublicSignaturesRemainExact) {
    SolveFunction_ cg = &Sparse::CGSolve;
    SolveFunction_ bcg = &Sparse::BCGSolve;
    ASSERT_NE(nullptr, cg);
    ASSERT_NE(nullptr, bcg);
}

TEST(MatrixTest, TestCGSolveScaledAlphaOverflow) {
    AssertScaledAlphaSolve(false, std::ldexp(1.0, -500), std::ldexp(1.0, -600), std::ldexp(1.0, 100), 0x6570000000000000ULL);
}

TEST(MatrixTest, TestBCGSolveScaledAlphaOverflow) {
    AssertScaledAlphaSolve(true, std::ldexp(1.0, -500), std::ldexp(1.0, -600), std::ldexp(1.0, 100), 0x6570000000000000ULL);
}

TEST(MatrixTest, TestCGSolveScaledAlphaUnderflow) {
    AssertScaledAlphaSolve(false, std::ldexp(1.0, 500), std::ldexp(1.0, 600), std::ldexp(1.0, -100), 0x1a70000000000000ULL);
}

TEST(MatrixTest, TestBCGSolveScaledAlphaUnderflow) {
    AssertScaledAlphaSolve(true, std::ldexp(1.0, 500), std::ldexp(1.0, 600), std::ldexp(1.0, -100), 0x1a70000000000000ULL);
}

TEST(MatrixTest, TestCGSolveScaledAlphaMinimumSubnormal) {
    AssertScaledAlphaSolve(false, std::ldexp(1.0, 500), std::ldexp(1.0, 1023), std::ldexp(1.0, -574), 0x0000000000000001ULL);
}

TEST(MatrixTest, TestBCGSolveScaledAlphaMinimumSubnormal) {
    AssertScaledAlphaSolve(true, std::ldexp(1.0, 500), std::ldexp(1.0, 1023), std::ldexp(1.0, -574), 0x0000000000000001ULL);
}

TEST(MatrixTest, TestScaledAlphaBoundsAndClassifierMatrix) {
    using BcgScaledAlphaPrivate_::AlphaPath_;
    using BcgScaledAlphaPrivate_::StoredScaledBits_;
    const StoredScaledBits_ unit{0x3fe0000000000000ULL, 1, true};
    struct ClassifierRow_ {
        const char* id_;
        StoredScaledBits_ numerator_;
        StoredScaledBits_ denominator_;
        std::uint64_t expectedNumerator_;
        std::uint64_t expectedDenominator_;
        int expectedExponent_;
        AlphaPath_ expectedPath_;
    };
    const std::array<ClassifierRow_, 12> rows = {{
        {"C1", {0x3fe0000000000000ULL, -1074, true}, unit, 1, 1, -1075, AlphaPath_::SCALED_EXACT},
        {"C2", {0x3fe0000000000000ULL, -1073, true}, unit, 1, 1, -1074, AlphaPath_::SCALED_EXACT},
        {"C3", {0x3feffffffffffffeULL, -1022, true}, unit, 4503599627370495ULL, 2251799813685248ULL, -1023, AlphaPath_::SCALED_EXACT},
        {"C4", {0x3fe0000000000000ULL, -1021, true}, unit, 1, 1, -1022, AlphaPath_::ORDINARY_NORMAL},
        {"C5", {0x3fe0000000000000ULL, 1024, true}, unit, 1, 1, 1023, AlphaPath_::ORDINARY_NORMAL},
        {"C6", {0x3fefffffffffffffULL, 1024, true}, unit, 9007199254740991ULL, 4503599627370496ULL, 1023, AlphaPath_::ORDINARY_NORMAL},
        {"C7",
         {0x3fefffffffffffffULL, 1025, true},
         {0x3feffffffffffffeULL, 1, true},
         9007199254740991ULL,
         9007199254740990ULL,
         1024,
         AlphaPath_::SCALED_EXACT},
        {"C8", {0x0000000000000001ULL, 0, false}, {0x0000000000000001ULL, 0, false}, 1, 1, 0, AlphaPath_::SCALED_EXACT},
        {"C9a", {0x3fe0000000000000ULL, 1025, true}, unit, 1, 1, 1024, AlphaPath_::SCALED_EXACT},
        {"C9b", {0x3fe0000000000000ULL, 1026, true}, unit, 1, 1, 1025, AlphaPath_::SCALED_EXACT},
        {"C10", {0x3fe0000000000000ULL, 1101, true}, unit, 1, 1, 1100, AlphaPath_::SCALED_EXACT},
        {"C11", {0x3fe0000000000000ULL, -1099, true}, unit, 1, 1, -1100, AlphaPath_::SCALED_EXACT},
    }};

    ASSERT_EQ(-8660, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.storedMinExponent_);
    ASSERT_EQ(8300, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.storedMaxExponent_);
    ASSERT_EQ(-16960, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.alphaMinExponent_);
    ASSERT_EQ(16960, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.alphaMaxExponent_);
    ASSERT_EQ(19112, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.logicalMagnitudeBits_);
    ASSERT_EQ(598, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.candidateLimbCount_);
    ASSERT_EQ(19111, BcgScaledAlphaPrivate_::REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_);

    for (const ClassifierRow_& row : rows) {
        for (const int signCase : {0, 1, 2}) {
            SCOPED_TRACE(std::string(row.id_) + " sign=" + std::to_string(signCase));
            StoredScaledBits_ numerator = row.numerator_;
            StoredScaledBits_ denominator = row.denominator_;
            if (signCase == 1)
                numerator.mantissaBits_ ^= 0x8000000000000000ULL;
            if (signCase == 2)
                denominator.mantissaBits_ ^= 0x8000000000000000ULL;
            AlphaClassifierObservation_ observations[2];
#if defined(__SSE2__) || defined(_M_X64)
            const int modeCount = 2;
#else
            const int modeCount = 1;
#endif
            for (int mode = 0; mode < modeCount; ++mode) {
                const bool flushToZero = mode != 0;
                observations[mode] = ObserveAlphaClassification(numerator, denominator, flushToZero);
#if defined(__SSE2__) || defined(_M_X64)
                const unsigned expectedMode = flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF;
                ASSERT_EQ(expectedMode, observations[mode].entryMxcsr_ & static_cast<unsigned>(_MM_FLUSH_ZERO_MASK));
                ASSERT_EQ(observations[mode].entryMxcsr_, observations[mode].exitMxcsr_);
#endif
                ASSERT_EQ(row.expectedNumerator_, observations[mode].plan_.exact_.numerator_);
                ASSERT_EQ(row.expectedDenominator_, observations[mode].plan_.exact_.denominator_);
                ASSERT_EQ(row.expectedExponent_, observations[mode].plan_.exact_.binaryExponent_);
                ASSERT_EQ(signCase != 0, observations[mode].plan_.exact_.negative_);
                ASSERT_EQ(row.expectedPath_, observations[mode].plan_.path_);
                ASSERT_EQ(row.expectedPath_ == AlphaPath_::ORDINARY_NORMAL ? 1 : 0, observations[mode].legacyConversionCalls_);
            }
#if defined(__SSE2__) || defined(_M_X64)
            ASSERT_EQ(observations[0].comparableBits_, observations[1].comparableBits_);
#endif
        }
    }

    ASSERT_EQ(AlphaPath_::DENOMINATOR_ZERO,
              BcgScaledAlphaPrivate_::ClassifyAlpha_({0x0000000000000000ULL, 0, false}, {0x8000000000000000ULL, 0, false}).path_);
    ASSERT_EQ(AlphaPath_::LEGACY_ZERO,
              BcgScaledAlphaPrivate_::ClassifyAlpha_({0x8000000000000000ULL, 0, false}, {0x3ff0000000000000ULL, 0, false}).path_);
}

TEST(MatrixTest, TestScaledAlphaOracleBootstrap) { AssertOracleBootstrap(); }

TEST(MatrixTest, TestScaledAlphaEvaluatorMatrixAndFtz) {
    AssertOracleBootstrap();
    for (const EvaluatorRow_& row : EvaluatorRows()) {
        SCOPED_TRACE(row.id_);
        const Dal35OneBitOracle_::OracleResult_ oracle = Dal35OneBitOracle_::Evaluate_(OracleInput(row.alpha_, row.valueBits_, row.baseBits_));
        const Dal35OneBitOracle_::OracleClass_ expectedOracleClass = row.expectedClass_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE
                                                                         ? Dal35OneBitOracle_::OracleClass_::FINITE
                                                                         : Dal35OneBitOracle_::OracleClass_::NON_FINITE;
        ASSERT_EQ(expectedOracleClass, oracle.classification_);
        if (oracle.classification_ == Dal35OneBitOracle_::OracleClass_::FINITE)
            ASSERT_EQ(row.expectedBits_, oracle.bits_);

        BcgScaledAlphaPrivate_::RoundedBinary64_ priorResult{};
        bool havePrior = false;
        for (const bool flushToZero : {false, true}) {
            BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
#if defined(__SSE2__) || defined(_M_X64)
            MxcsrRestore_ restore;
            const unsigned configured =
                (_mm_getcsr() & ~static_cast<unsigned>(_MM_FLUSH_ZERO_MASK)) | (flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF);
            _mm_setcsr(configured);
            const unsigned before = _mm_getcsr();
#else
            if (flushToZero)
                continue;
#endif
            const BcgScaledAlphaPrivate_::RoundedBinary64_ result =
                BcgScaledAlphaPrivate_::EvaluateElement_(row.alpha_, row.valueBits_, row.baseBits_, &workspace);
#if defined(__SSE2__) || defined(_M_X64)
            const unsigned after = _mm_getcsr();
            ASSERT_EQ(before, after);
#endif
            ASSERT_EQ(row.expectedClass_, result.classification_);
            std::uint64_t destination = 0x3fd5555555555555ULL;
            if (result.classification_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE)
                destination = result.bits_;
            ASSERT_EQ(row.expectedClass_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE ? row.expectedBits_ : 0x3fd5555555555555ULL, destination);
            AssertCanonicalWorkspace(workspace);
            if (havePrior) {
                ASSERT_EQ(priorResult.classification_, result.classification_);
                if (result.classification_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE)
                    ASSERT_EQ(priorResult.bits_, result.bits_);
            }
            priorResult = result;
            havePrior = true;
        }
    }
}

TEST(MatrixTest, TestScaledAlphaWorkspaceCleanupInjection) {
    const BcgScaledAlphaPrivate_::ExactAlpha_ alpha{1, 1, 0, false};
    {
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        workspace.positive_.first_ = 7;
        workspace.positive_.last_ = 9;
        workspace.positive_.limbs_[7] = 1;
        workspace.positive_.limbs_[9] = 3;
        ASSERT_THROW(BcgScaledAlphaPrivate_::EvaluateElement_(alpha, 0x3ff0000000000000ULL, 0x0000000000000000ULL, &workspace), Exception_);
        AssertCanonicalWorkspace(workspace);
    }
    {
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        workspace.negative_.first_ = -1;
        workspace.negative_.last_ = BcgScaledAlphaPrivate_::EXACT_CANDIDATE_LIMB_COUNT_;
        workspace.negative_.limbs_[0] = 5;
        workspace.negative_.limbs_.back() = 7;
        ASSERT_THROW(BcgScaledAlphaPrivate_::EvaluateElement_(alpha, 0x3ff0000000000000ULL, 0x0000000000000000ULL, &workspace), Exception_);
        AssertCanonicalWorkspace(workspace);
    }
}

TEST(MatrixTest, TestScaledAlphaWorkspaceForwardReverseReuse) {
    AssertOracleBootstrap();
    const std::array<int, 4> order = {9, 4, 13, 14};
    BcgScaledAlphaPrivate_::ExactWorkspace_ reused;
    for (const bool reverse : {false, true}) {
        for (int position = 0; position < static_cast<int>(order.size()); ++position) {
            const int index = order[reverse ? static_cast<int>(order.size()) - 1 - position : position];
            const EvaluatorRow_& row = EvaluatorRows()[index];
            SCOPED_TRACE(std::string(row.id_) + (reverse ? " reverse" : " forward"));
            const BcgScaledAlphaPrivate_::RoundedBinary64_ actual =
                BcgScaledAlphaPrivate_::EvaluateElement_(row.alpha_, row.valueBits_, row.baseBits_, &reused);
            BcgScaledAlphaPrivate_::ExactWorkspace_ fresh;
            const BcgScaledAlphaPrivate_::RoundedBinary64_ expected =
                BcgScaledAlphaPrivate_::EvaluateElement_(row.alpha_, row.valueBits_, row.baseBits_, &fresh);
            ASSERT_EQ(expected.classification_, actual.classification_);
            if (actual.classification_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE)
                ASSERT_EQ(expected.bits_, actual.bits_);
            AssertCanonicalWorkspace(reused);
            AssertCanonicalWorkspace(fresh);
        }
    }
}

TEST(MatrixTest, TestScaledAlphaReviewedExponentAndTopCarryBounds) {
    const std::uint64_t maximumSignificand = (1ULL << 53U) - 1ULL;
    BcgScaledAlphaPrivate_::ExactMagnitude_ topCarry;
    BcgScaledAlphaPrivate_::AddProduct_(maximumSignificand, maximumSignificand, 19005, &topCarry);
    BcgScaledAlphaPrivate_::AddProduct_(maximumSignificand, maximumSignificand, 19005, &topCarry);
    ASSERT_EQ(19111, BcgScaledAlphaPrivate_::HighestBit_(topCarry));
    BcgScaledAlphaPrivate_::ResetMagnitude_(&topCarry);
    ASSERT_EQ(BcgScaledAlphaPrivate_::EXACT_CANDIDATE_LIMB_COUNT_, topCarry.first_);
    ASSERT_EQ(-1, topCarry.last_);

    const std::array<BcgScaledAlphaPrivate_::ExactAlpha_, 2> alphas = {
        BcgScaledAlphaPrivate_::ExactAlpha_{maximumSignificand, maximumSignificand, -16960, false},
        BcgScaledAlphaPrivate_::ExactAlpha_{maximumSignificand, maximumSignificand, 16960, false}};
    const std::array<std::uint64_t, 2> values = {0x0000000000000001ULL, 0x7fefffffffffffffULL};
    const std::array<std::uint64_t, 2> bases = {0x7fefffffffffffffULL, 0x0000000000000001ULL};
    for (int i = 0; i < 2; ++i) {
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        const BcgScaledAlphaPrivate_::RoundedBinary64_ actual = BcgScaledAlphaPrivate_::EvaluateElement_(alphas[i], values[i], bases[i], &workspace);
        const Dal35OneBitOracle_::OracleResult_ expected = Dal35OneBitOracle_::Evaluate_(OracleInput(alphas[i], values[i], bases[i]));
        ASSERT_EQ(expected.classification_ == Dal35OneBitOracle_::OracleClass_::FINITE ? BcgScaledAlphaPrivate_::RoundedClass_::FINITE
                                                                                       : BcgScaledAlphaPrivate_::RoundedClass_::NON_FINITE,
                  actual.classification_);
        if (actual.classification_ == BcgScaledAlphaPrivate_::RoundedClass_::FINITE)
            ASSERT_EQ(expected.bits_, actual.bits_);
        AssertCanonicalWorkspace(workspace);
    }
}

TEST(MatrixTest, TestScaledAlphaCandidateEvidencePriorityAndAscendingIndex) {
    using BcgScaledAlphaPrivate_::CandidateEvidence_;
    using BcgScaledAlphaPrivate_::CandidateGroup_;
    using BcgScaledAlphaPrivate_::CandidateSubject_;
    const BcgScaledAlphaPrivate_::ExactAlpha_ alpha{1, 1, 1100, false};
    const Vector_<> zeroBase = {0.0, 0.0, 0.0};
    {
        const Vector_<> direction = {0.0, 1.0, 1.0};
        Vector_<> residual = {3.0, 5.0, 7.0};
        const Vector_<> residualBase = {11.0, 13.0, 17.0};
        Vector_<> shadow = {19.0, 23.0, 29.0};
        const Vector_<> shadowBase = {31.0, 37.0, 41.0};
        Vector_<> xOutput = {43.0, 47.0, 53.0};
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        const CandidateGroup_ group{
            &direction, &zeroBase, &residualBase, &shadowBase, &xOutput, &residual, &shadow,
        };

        const CandidateEvidence_ evidence = BcgScaledAlphaPrivate_::EvaluateCandidateGroup_(alpha, group, &workspace);

        ASSERT_EQ(CandidateSubject_::X, evidence.subject_);
        ASSERT_EQ(1, evidence.firstNonFiniteIndex_);
        ASSERT_EQ(0x0000000000000000ULL, DoubleBits(xOutput[0]));
        ASSERT_EQ(0x4047800000000000ULL, DoubleBits(xOutput[1]));
        ASSERT_EQ(0x404a800000000000ULL, DoubleBits(xOutput[2]));
        ASSERT_EQ(0x4008000000000000ULL, DoubleBits(residual[0]));
        ASSERT_EQ(0x4014000000000000ULL, DoubleBits(residual[1]));
        ASSERT_EQ(0x401c000000000000ULL, DoubleBits(residual[2]));
        ASSERT_EQ(0x4033000000000000ULL, DoubleBits(shadow[0]));
        ASSERT_EQ(0x4037000000000000ULL, DoubleBits(shadow[1]));
        ASSERT_EQ(0x403d000000000000ULL, DoubleBits(shadow[2]));
        AssertCanonicalWorkspace(workspace);
    }
    {
        const Vector_<> direction = {0.0, 0.0, 0.0};
        Vector_<> residual = {0.0, 1.0, 1.0};
        const Vector_<> residualBase = {0.0, 0.0, 0.0};
        Vector_<> shadow = {19.0, 23.0, 29.0};
        const Vector_<> shadowBase = {31.0, 37.0, 41.0};
        Vector_<> xOutput = {43.0, 47.0, 53.0};
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        const CandidateGroup_ group{
            &direction, &zeroBase, &residualBase, &shadowBase, &xOutput, &residual, &shadow,
        };

        const CandidateEvidence_ evidence = BcgScaledAlphaPrivate_::EvaluateCandidateGroup_(alpha, group, &workspace);

        ASSERT_EQ(CandidateSubject_::RESIDUAL, evidence.subject_);
        ASSERT_EQ(1, evidence.firstNonFiniteIndex_);
        ASSERT_EQ(0x0000000000000000ULL, DoubleBits(residual[0]));
        ASSERT_EQ(0x3ff0000000000000ULL, DoubleBits(residual[1]));
        ASSERT_EQ(0x3ff0000000000000ULL, DoubleBits(residual[2]));
        ASSERT_EQ(0x4033000000000000ULL, DoubleBits(shadow[0]));
        ASSERT_EQ(0x4037000000000000ULL, DoubleBits(shadow[1]));
        ASSERT_EQ(0x403d000000000000ULL, DoubleBits(shadow[2]));
        AssertCanonicalWorkspace(workspace);
    }
    {
        const Vector_<> direction = {0.0, 0.0, 0.0};
        Vector_<> residual = {0.0, 0.0, 0.0};
        const Vector_<> residualBase = {0.0, 0.0, 0.0};
        Vector_<> shadow = {0.0, 1.0, 1.0};
        const Vector_<> shadowBase = {0.0, 0.0, 0.0};
        Vector_<> xOutput = {43.0, 47.0, 53.0};
        BcgScaledAlphaPrivate_::ExactWorkspace_ workspace;
        const CandidateGroup_ group{
            &direction, &zeroBase, &residualBase, &shadowBase, &xOutput, &residual, &shadow,
        };

        const CandidateEvidence_ evidence = BcgScaledAlphaPrivate_::EvaluateCandidateGroup_(alpha, group, &workspace);

        ASSERT_EQ(CandidateSubject_::SHADOW_RESIDUAL, evidence.subject_);
        ASSERT_EQ(1, evidence.firstNonFiniteIndex_);
        ASSERT_EQ(0x0000000000000000ULL, DoubleBits(shadow[0]));
        ASSERT_EQ(0x3ff0000000000000ULL, DoubleBits(shadow[1]));
        ASSERT_EQ(0x3ff0000000000000ULL, DoubleBits(shadow[2]));
        AssertCanonicalWorkspace(workspace);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaOverflowCancellation) {
    const double diagonal = std::ldexp(1.0, -500);
    const double preconditioner = std::ldexp(1.0, -600);
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        const ScaledAlphaObservation_ positive =
            ObserveScaledAlphaSolve(biConjugate, diagonal, preconditioner, std::ldexp(1.0, 523), -std::ldexp(1.0, 1023));
        const ScaledAlphaObservation_ negative =
            ObserveScaledAlphaSolve(biConjugate, diagonal, preconditioner, -std::ldexp(1.0, 523), std::ldexp(1.0, 1023));
        ASSERT_EQ(0x7fe0000000000000ULL, positive.resultBits_);
        ASSERT_EQ(0xffe0000000000000ULL, negative.resultBits_);
        ASSERT_EQ(0x0000000000000000ULL, positive.directResidualBits_);
        ASSERT_EQ(0x0000000000000000ULL, negative.directResidualBits_);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaMinimumSubnormalCancellation) {
    const double diagonal = std::ldexp(1.0, 500);
    const double preconditioner = std::ldexp(1.0, 600);
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        const ScaledAlphaObservation_ positive =
            ObserveScaledAlphaSolve(biConjugate, diagonal, preconditioner, std::ldexp(1.0, -574), std::ldexp(1.0, -1022));
        const ScaledAlphaObservation_ negative =
            ObserveScaledAlphaSolve(biConjugate, diagonal, preconditioner, -std::ldexp(1.0, -574), -std::ldexp(1.0, -1022));
        ASSERT_EQ(0x0000000000000001ULL, positive.resultBits_);
        ASSERT_EQ(0x8000000000000001ULL, negative.resultBits_);
        ASSERT_EQ(0x0000000000000000ULL, positive.directResidualBits_);
        ASSERT_EQ(0x0000000000000000ULL, negative.directResidualBits_);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaRejectCandidateOverflow) {
    const double diagonal = std::ldexp(1.0, -500);
    const double preconditionerScale = std::ldexp(1.0, -600);
    const Vector_<> b = {std::ldexp(1.0, 600)};
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        CallbackCounts_ counts;
        HookedPreconditionedDiagonal_ matrix({diagonal}, &counts);
        const auto preconditioner = [preconditionerScale](int, const Vector_<>& input, Vector_<>* output) {
            (*output)[0] = preconditionerScale * input[0];
            return true;
        };
        matrix.SetPreconditionerLeftHook(preconditioner);
        matrix.SetPreconditionerRightHook(preconditioner);
        Vector_<> x = {0.0};
        const double* const entryStorage = &x[0];

        AssertDalExceptionContains([&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 2, &x); },
                                   {SolverName(biConjugate), "numerical breakdown", "candidate x"});

        const int commitCount = &x[0] == entryStorage ? 0 : 1;
        ASSERT_EQ(0, commitCount);
        ASSERT_EQ(0x0000000000000000ULL, DoubleBits(x[0]));
        ASSERT_EQ(2, counts.left_);
        ASSERT_EQ(biConjugate ? 1 : 0, counts.right_);
        ASSERT_EQ(1, counts.preconditionerLeft_);
        ASSERT_EQ(biConjugate ? 1 : 0, counts.preconditionerRight_);
    }
}

TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaRejectResidualAndShadowOverflow) {
    const double minimumSubnormal = DoubleFromBits(0x0000000000000001ULL);
    const double maximumFinite = DoubleFromBits(0x7fefffffffffffffULL);
    const Vector_<> b = {1.0, maximumFinite};
    for (const bool shadowFailure : {false, true}) {
        for (const bool biConjugate : {false, true}) {
            if (shadowFailure && !biConjugate)
                continue;
            SCOPED_TRACE(std::string(SolverName(biConjugate)) + (shadowFailure ? " shadow" : " residual"));
            CallbackCounts_ counts;
            HookedPreconditionedDiagonal_ matrix({1.0, 1.0}, &counts);
            matrix.SetLeftHook([minimumSubnormal, shadowFailure](int call, const Vector_<>&, Vector_<>* output) {
                if (call == 1) {
                    (*output)[0] = 0.0;
                    (*output)[1] = 0.0;
                } else {
                    (*output)[0] = shadowFailure ? 0.0 : 1.0;
                    (*output)[1] = minimumSubnormal;
                }
                return true;
            });
            matrix.SetRightHook([minimumSubnormal](int, const Vector_<>&, Vector_<>* output) {
                (*output)[0] = 1.0;
                (*output)[1] = minimumSubnormal;
                return true;
            });
            const auto preconditioner = [minimumSubnormal](int, const Vector_<>&, Vector_<>* output) {
                (*output)[0] = 0.0;
                (*output)[1] = minimumSubnormal;
                return true;
            };
            matrix.SetPreconditionerLeftHook(preconditioner);
            matrix.SetPreconditionerRightHook(preconditioner);
            Vector_<> x = {0.0, 0.0};
            const double* const entryStorage = &x[0];

            AssertDalExceptionContains(
                [&]() { RunSolver(biConjugate, matrix, b, 1e-12, 0.0, 2, &x); },
                {SolverName(biConjugate), "numerical breakdown", shadowFailure ? "candidate shadow residual" : "candidate residual"});

            const int commitCount = &x[0] == entryStorage ? 0 : 1;
            ASSERT_EQ(0, commitCount);
            ASSERT_EQ(0x0000000000000000ULL, DoubleBits(x[0]));
            ASSERT_EQ(0x0000000000000000ULL, DoubleBits(x[1]));
            ASSERT_EQ(2, counts.left_);
            ASSERT_EQ(biConjugate ? 1 : 0, counts.right_);
            ASSERT_EQ(1, counts.preconditionerLeft_);
            ASSERT_EQ(biConjugate ? 1 : 0, counts.preconditionerRight_);
        }
    }
}

#if defined(__SSE2__) || defined(_M_X64)
TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaNamedFtzFixtures) {
    struct SolverFtzRow_ {
        const char* id_;
        double diagonal_;
        double preconditioner_;
        double rhs_;
        double initial_;
        std::uint64_t expectedBits_;
        BcgScaledAlphaPrivate_::ExactAlpha_ exactAlpha_;
        std::uint64_t expectedRhsBits_;
        std::array<std::uint64_t, 16> cgCallbacks_;
        std::array<std::uint64_t, 24> bcgCallbacks_;
    };
    const std::array<SolverFtzRow_, 3> rows = {{
        {"S3",
         std::ldexp(1.0, 500),
         std::ldexp(1.0, 1023),
         std::ldexp(1.0, -574),
         0.0,
         0x0000000000000001ULL,
         {1, 1, -1523, false},
         0x1c10000000000000ULL,
         {1, 1, 0x0000000000000000ULL, 0x0000000000000000ULL, 3, 1, 0x1c10000000000000ULL, 0x5c00000000000000ULL, 1, 2, 0x5c00000000000000ULL,
          0x7b40000000000000ULL, 1, 3, 0x0000000000000001ULL, 0x1c10000000000000ULL},
         {1, 1, 0x0000000000000000ULL, 0x0000000000000000ULL, 3, 1, 0x1c10000000000000ULL, 0x5c00000000000000ULL,
          4, 1, 0x1c10000000000000ULL, 0x5c00000000000000ULL, 1, 2, 0x5c00000000000000ULL, 0x7b40000000000000ULL,
          2, 1, 0x5c00000000000000ULL, 0x7b40000000000000ULL, 1, 3, 0x0000000000000001ULL, 0x1c10000000000000ULL}},
        {"S5+",
         std::ldexp(1.0, 500),
         std::ldexp(1.0, 600),
         std::ldexp(1.0, -574),
         std::ldexp(1.0, -1022),
         0x0000000000000001ULL,
         {1, 1, -1100, false},
         0x1c10000000000000ULL,
         {1, 1, 0x0010000000000000ULL, 0x1f50000000000000ULL, 3, 1, 0x9f4ffffffffffffeULL, 0xc4cffffffffffffeULL, 1, 2, 0xc4cffffffffffffeULL,
          0xe40ffffffffffffeULL, 1, 3, 0x0000000000000001ULL, 0x1c10000000000000ULL},
         {1, 1, 0x0010000000000000ULL, 0x1f50000000000000ULL, 3, 1, 0x9f4ffffffffffffeULL, 0xc4cffffffffffffeULL,
          4, 1, 0x9f4ffffffffffffeULL, 0xc4cffffffffffffeULL, 1, 2, 0xc4cffffffffffffeULL, 0xe40ffffffffffffeULL,
          2, 1, 0xc4cffffffffffffeULL, 0xe40ffffffffffffeULL, 1, 3, 0x0000000000000001ULL, 0x1c10000000000000ULL}},
        {"S5-",
         std::ldexp(1.0, 500),
         std::ldexp(1.0, 600),
         -std::ldexp(1.0, -574),
         -std::ldexp(1.0, -1022),
         0x8000000000000001ULL,
         {1, 1, -1100, false},
         0x9c10000000000000ULL,
         {1, 1, 0x8010000000000000ULL, 0x9f50000000000000ULL, 3, 1, 0x1f4ffffffffffffeULL, 0x44cffffffffffffeULL, 1, 2, 0x44cffffffffffffeULL,
          0x640ffffffffffffeULL, 1, 3, 0x8000000000000001ULL, 0x9c10000000000000ULL},
         {1, 1, 0x8010000000000000ULL, 0x9f50000000000000ULL, 3, 1, 0x1f4ffffffffffffeULL, 0x44cffffffffffffeULL,
          4, 1, 0x1f4ffffffffffffeULL, 0x44cffffffffffffeULL, 1, 2, 0x44cffffffffffffeULL, 0x640ffffffffffffeULL,
          2, 1, 0x44cffffffffffffeULL, 0x640ffffffffffffeULL, 1, 3, 0x8000000000000001ULL, 0x9c10000000000000ULL}},
    }};
    for (const bool biConjugate : {false, true}) {
        for (const SolverFtzRow_& row : rows) {
            SCOPED_TRACE(std::string(SolverName(biConjugate)) + " " + row.id_);
            ScaledAlphaObservation_ observations[2];
            for (const bool flushToZero : {false, true}) {
                MxcsrRestore_ restore;
                const unsigned configured =
                    (_mm_getcsr() & ~static_cast<unsigned>(_MM_FLUSH_ZERO_MASK)) | (flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF);
                _mm_setcsr(configured);
                const unsigned before = _mm_getcsr();
                observations[flushToZero ? 1 : 0] =
                    ObserveScaledAlphaSolve(biConjugate, row.diagonal_, row.preconditioner_, row.rhs_, row.initial_, &row.exactAlpha_);
                const unsigned after = _mm_getcsr();
                ASSERT_EQ(flushToZero ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF, before & static_cast<unsigned>(_MM_FLUSH_ZERO_MASK));
                ASSERT_EQ(before & static_cast<unsigned>(_MM_FLUSH_ZERO_MASK), after & static_cast<unsigned>(_MM_FLUSH_ZERO_MASK));
                const ScaledAlphaObservation_& observation = observations[flushToZero ? 1 : 0];
                const CallbackCounts_ expectedCounts = biConjugate ? CallbackCounts_{3, 1, 1, 1} : CallbackCounts_{3, 0, 1, 0};
                const std::vector<std::uint64_t> expectedCallbacks =
                    biConjugate ? std::vector<std::uint64_t>(row.bcgCallbacks_.begin(), row.bcgCallbacks_.end())
                                : std::vector<std::uint64_t>(row.cgCallbacks_.begin(), row.cgCallbacks_.end());
                ASSERT_EQ(row.expectedBits_, observation.resultBits_);
                ASSERT_EQ(0x0000000000000000ULL, observation.directResidualBits_);
                ASSERT_EQ(BcgScaledAlphaPrivate_::CandidateSubject_::NONE, observation.evidenceSubject_);
                ASSERT_EQ(-1, observation.evidenceIndex_);
                ASSERT_EQ(1, observation.commitCount_);
                ASSERT_EQ(1, observation.confirmationCount_);
                ASSERT_EQ(row.expectedBits_, observation.confirmationInputBits_);
                ASSERT_EQ(row.expectedRhsBits_, observation.confirmationOutputBits_);
                ASSERT_EQ(expectedCallbacks, observation.callbackBits_);
                AssertCallbackCounts(expectedCounts, observation.counts_);
            }
            ASSERT_EQ(row.expectedBits_, observations[0].resultBits_);
            ASSERT_EQ(0x0000000000000000ULL, observations[0].directResidualBits_);
            ASSERT_EQ(BcgScaledAlphaPrivate_::CandidateSubject_::NONE, observations[0].evidenceSubject_);
            ASSERT_EQ(-1, observations[0].evidenceIndex_);
            ASSERT_EQ(1, observations[0].commitCount_);
            ASSERT_EQ(observations[0].resultBits_, observations[1].resultBits_);
            ASSERT_EQ(observations[0].directResidualBits_, observations[1].directResidualBits_);
            ASSERT_EQ(observations[0].evidenceSubject_, observations[1].evidenceSubject_);
            ASSERT_EQ(observations[0].evidenceIndex_, observations[1].evidenceIndex_);
            ASSERT_EQ(observations[0].commitCount_, observations[1].commitCount_);
            ASSERT_EQ(observations[0].callbackBits_, observations[1].callbackBits_);
            AssertCallbackCounts(observations[0].counts_, observations[1].counts_);
        }
    }
}
#endif

#if defined(DAL35_ENABLE_TEST_SEAM)
TEST(MatrixTest, TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary) {
    const double diagonal = std::ldexp(1.0, -500);
    const double preconditioner = std::ldexp(1.0, -600);
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        dal35ExactWorkspaceConstructionCount_ = 0;
        const OrdinaryCandidateObservation_ ordinary = ObserveOrdinaryCandidateCommit(biConjugate);
        ASSERT_TRUE(ordinary.callbackException_);
        ASSERT_EQ(1, ordinary.commitCount_);
        ASSERT_EQ(0, dal35ExactWorkspaceConstructionCount_);

        dal35ExactWorkspaceConstructionCount_ = 0;
        const ScaledAlphaObservation_ scaled =
            ObserveScaledAlphaSolve(biConjugate, diagonal, preconditioner, std::ldexp(1.0, 523), -std::ldexp(1.0, 1023));
        ASSERT_EQ(0x7fe0000000000000ULL, scaled.resultBits_);
        ASSERT_EQ(1, scaled.commitCount_);
        ASSERT_EQ(1, dal35ExactWorkspaceConstructionCount_);
    }
}
#endif

TEST(MatrixTest, TestCGSolveAndBCGSolveOrdinaryAlphaBitwiseCorpus) {
    const std::vector<std::uint64_t> cgCallbacks = {
        1, 1, 0x0000000000000000ULL, 0x0000000000000000ULL, 3, 1, 0x4018000000000000ULL, 0x4018000000000000ULL,
        1, 2, 0x4018000000000000ULL, 0x4028000000000000ULL, 1, 3, 0x4008000000000000ULL, 0x4018000000000000ULL};
    const std::vector<std::uint64_t> bcgCallbacks = {
        1, 1, 0x0000000000000000ULL, 0x0000000000000000ULL, 3, 1, 0x4018000000000000ULL, 0x4018000000000000ULL,
        4, 1, 0x4018000000000000ULL, 0x4018000000000000ULL, 1, 2, 0x4018000000000000ULL, 0x4028000000000000ULL,
        2, 1, 0x4018000000000000ULL, 0x4028000000000000ULL, 1, 3, 0x4008000000000000ULL, 0x4018000000000000ULL};
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        const OrdinaryCandidateObservation_ candidateObservation = ObserveOrdinaryCandidateCommit(biConjugate);
        ASSERT_TRUE(candidateObservation.callbackException_);
        ASSERT_EQ(1, candidateObservation.commitCount_);
        ASSERT_EQ(0x3fe0000000000000ULL, candidateObservation.coefficientBits_);
        ASSERT_EQ((std::array<std::uint64_t, 2>{0x3ff0000000000000ULL, 0x4000000000000000ULL}), candidateObservation.directionBits_);
        ASSERT_EQ((std::array<std::uint64_t, 2>{0x3ff0000000000000ULL, 0x4012000000000000ULL}), candidateObservation.operatorDirectionBits_);
        ASSERT_EQ((std::array<std::uint64_t, 2>{0x3fe0000000000000ULL, 0x3ff0000000000000ULL}), candidateObservation.xCandidateBits_);
        ASSERT_EQ((std::array<std::uint64_t, 2>{0x3fe0000000000000ULL, 0xbfd0000000000000ULL}), candidateObservation.residualCandidateBits_);
        if (biConjugate)
            ASSERT_EQ((std::array<std::uint64_t, 2>{0x3fe0000000000000ULL, 0xbfd0000000000000ULL}), candidateObservation.shadowCandidateBits_);
        ASSERT_EQ(candidateObservation.xCandidateBits_, candidateObservation.finalBits_);
        AssertCallbackCounts(biConjugate ? CallbackCounts_{2, 1, 2, 2} : CallbackCounts_{2, 0, 2, 0}, candidateObservation.counts_);
#if defined(__SSE2__) || defined(_M_X64)
        MxcsrRestore_ restore;
        const unsigned statusMask = _MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW;
        const unsigned configured = _mm_getcsr() | statusMask;
        _mm_setcsr(configured);
#endif
        const ScaledAlphaObservation_ observation = ObserveScaledAlphaSolve(biConjugate, 2.0, 1.0, 6.0, 0.0);
#if defined(__SSE2__) || defined(_M_X64)
        ASSERT_EQ(configured & statusMask, _mm_getcsr() & statusMask);
#endif
        ASSERT_EQ(0x4008000000000000ULL, observation.resultBits_);
        ASSERT_EQ(0x0000000000000000ULL, observation.directResidualBits_);
        ASSERT_EQ(biConjugate ? bcgCallbacks : cgCallbacks, observation.callbackBits_);
        ASSERT_EQ(3, observation.counts_.left_);
        ASSERT_EQ(biConjugate ? 1 : 0, observation.counts_.right_);
        ASSERT_EQ(1, observation.counts_.preconditionerLeft_);
        ASSERT_EQ(biConjugate ? 1 : 0, observation.counts_.preconditionerRight_);
    }
}

#if defined(__SSE2__) || defined(_M_X64)
TEST(MatrixTest, TestCGSolveAndBCGSolveOrdinaryAlphaFpStatus) {
    const unsigned statusMask = _MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW;
    for (const bool biConjugate : {false, true}) {
        for (const unsigned entryStatus : {0U, static_cast<unsigned>(_MM_EXCEPT_INVALID), static_cast<unsigned>(_MM_EXCEPT_OVERFLOW),
                                           static_cast<unsigned>(_MM_EXCEPT_INVALID | _MM_EXCEPT_OVERFLOW)}) {
            SCOPED_TRACE(std::string(SolverName(biConjugate)) + " success status=" + std::to_string(entryStatus));
            const OrdinaryFpStatusObservation_ observation = ObserveOrdinaryFpStatus(biConjugate, entryStatus, false);
            ASSERT_EQ(entryStatus, observation.entryStatus_ & statusMask);
            ASSERT_EQ(entryStatus, observation.exitStatus_ & statusMask);
            ASSERT_EQ(0x4008000000000000ULL, observation.resultBits_);
            ASSERT_EQ(1, observation.commitCount_);
            ASSERT_TRUE(observation.failureMessage_.empty());
        }
        for (const unsigned entryStatus : {0U, static_cast<unsigned>(_MM_EXCEPT_INVALID)}) {
            SCOPED_TRACE(std::string(SolverName(biConjugate)) + " overflow status=" + std::to_string(entryStatus));
            const OrdinaryFpStatusObservation_ observation = ObserveOrdinaryFpStatus(biConjugate, entryStatus, true);
            ASSERT_EQ(entryStatus, observation.entryStatus_ & statusMask);
            ASSERT_EQ(entryStatus | static_cast<unsigned>(_MM_EXCEPT_OVERFLOW), observation.exitStatus_ & statusMask);
            ASSERT_EQ(0x0000000000000000ULL, observation.resultBits_);
            ASSERT_EQ(0, observation.commitCount_);
            ASSERT_NE(std::string::npos, observation.failureMessage_.find(SolverName(biConjugate)));
            ASSERT_NE(std::string::npos, observation.failureMessage_.find("numerical breakdown"));
            ASSERT_NE(std::string::npos, observation.failureMessage_.find("candidate x"));
        }
    }
}
#endif

TEST(MatrixTest, TestCGSolveAndBCGSolveScaledAlphaAddsNoHeapAllocations) {
    for (const bool biConjugate : {false, true}) {
        SCOPED_TRACE(SolverName(biConjugate));
        const AllocationObservation_ ordinary = ObserveSolveAllocations(biConjugate, 2.0, 1.0, 6.0);
        const AllocationObservation_ scaled =
            ObserveSolveAllocations(biConjugate, std::ldexp(1.0, -500), std::ldexp(1.0, -600), std::ldexp(1.0, 100));
        ASSERT_GT(ordinary.count_, 0U);
        ASSERT_LE(scaled.count_, ordinary.count_);
        ASSERT_EQ(0x4008000000000000ULL, ordinary.resultBits_);
        ASSERT_EQ(0x6570000000000000ULL, scaled.resultBits_);
    }
}
