//
// Created by wegam on 2022/12/18.
//

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/math/matrix/bcg.hpp>

using namespace Dal;

namespace {
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

    for(int i = 0; i < n; ++i)
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

    for(int i = 0; i < n; ++i)
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

    bool CommonExponentConverged(const Vector_<>& residual, const Vector_<>& b, double tolRel, double tolAbs) {
        std::vector<OracleBinary_> residualTerms;
        std::vector<OracleBinary_> relativeTerms;
        residualTerms.reserve(residual.size());
        relativeTerms.reserve(b.size());

        int commonExponent = std::numeric_limits<int>::min();
        for (int i = 0; i < static_cast<int>(residual.size()); ++i) {
            OracleBinary_ residualTerm = OracleFromDouble(residual[i]);
            OracleBinary_ relativeTerm = OracleMultiply(OracleFromDouble(tolRel), OracleFromDouble(b[i]));
            if (!residualTerm.bits_.empty())
                commonExponent = std::max(commonExponent, residualTerm.bits_.rbegin()->first + 1);
            if (!relativeTerm.bits_.empty())
                commonExponent = std::max(commonExponent, relativeTerm.bits_.rbegin()->first + 1);
            residualTerms.push_back(std::move(residualTerm));
            relativeTerms.push_back(std::move(relativeTerm));
        }
        OracleBinary_ absoluteTerm = OracleFromDouble(tolAbs);
        if (!absoluteTerm.bits_.empty())
            commonExponent = std::max(commonExponent, absoluteTerm.bits_.rbegin()->first + 1);
        if (commonExponent == std::numeric_limits<int>::min())
            commonExponent = 0;

        for (OracleBinary_& value : residualTerms)
            value = OracleShift(value, -commonExponent);
        for (OracleBinary_& value : relativeTerms)
            value = OracleShift(value, -commonExponent);
        absoluteTerm = OracleShift(absoluteTerm, -commonExponent);

        const OracleBinary_ residualSquares = OracleSumSquares(residualTerms);
        const OracleBinary_ relativeSquares = OracleSumSquares(relativeTerms);
        if (OracleCompare(residualSquares, relativeSquares) <= 0)
            return true;

        const OracleBinary_ absoluteSquare = OracleMultiply(absoluteTerm, absoluteTerm);
        const OracleBinary_ baseToleranceSquare = OracleAdd(relativeSquares, absoluteSquare);
        if (OracleCompare(residualSquares, baseToleranceSquare) <= 0)
            return true;

        const OracleBinary_ difference = OracleSubtract(residualSquares, baseToleranceSquare);
        const OracleBinary_ crossTermSquare = OracleShift(OracleMultiply(absoluteSquare, relativeSquares), 2);
        return OracleCompare(OracleMultiply(difference, difference), crossTermSquare) <= 0;
    }

    Vector_<> DiagonalResidual(const Vector_<>& diagonal, const Vector_<>& x, const Vector_<>& b) {
        Vector_<> residual(b.size());
        for (int i = 0; i < static_cast<int>(b.size()); ++i)
            residual[i] = std::fma(-diagonal[i], x[i], b[i]);
        return residual;
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

    void AssertCallbackFault(bool biConjugate, int site, int fault, int badSize = 0) {
        SCOPED_TRACE(std::string(SolverName(biConjugate)) + " site=" + std::to_string(site) + " fault=" + std::to_string(fault));
        CallbackCounts_ counts;
        std::unique_ptr<HookedDiagonal_> matrix;
        if (site == 3 || site == 4)
            matrix = std::make_unique<HookedPreconditionedDiagonal_>(Vector_<>{1.0, 2.0}, &counts);
        else
            matrix = std::make_unique<HookedDiagonal_>(Vector_<>{1.0, 2.0}, &counts);

        const Vector_<> diagonal = {1.0, 2.0};
        auto operatorFault = [fault, badSize, &diagonal](int, const Vector_<>& input, Vector_<>* output) {
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
        };
        auto preconditionerFault = [fault, badSize](int, const Vector_<>& input, Vector_<>* output) {
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
        };

        if (site == 0)
            matrix->SetLeftHook(
                [operatorFault](int call, const Vector_<>& input, Vector_<>* output) { return call == 1 && operatorFault(call, input, output); });
        else if (site == 1)
            matrix->SetLeftHook(
                [operatorFault](int call, const Vector_<>& input, Vector_<>* output) { return call == 2 && operatorFault(call, input, output); });
        else if (site == 2)
            matrix->SetRightHook(
                [operatorFault](int call, const Vector_<>& input, Vector_<>* output) { return call == 1 && operatorFault(call, input, output); });
        else if (site == 3)
            dynamic_cast<HookedPreconditionedDiagonal_*>(matrix.get())
                ->SetPreconditionerLeftHook([preconditionerFault](int call, const Vector_<>& input, Vector_<>* output) {
                    return call == 1 && preconditionerFault(call, input, output);
                });
        else if (site == 4)
            dynamic_cast<HookedPreconditionedDiagonal_*>(matrix.get())
                ->SetPreconditionerRightHook([preconditionerFault](int call, const Vector_<>& input, Vector_<>* output) {
                    return call == 1 && preconditionerFault(call, input, output);
                });
        else
            matrix->SetLeftHook(
                [operatorFault](int call, const Vector_<>& input, Vector_<>* output) { return call == 4 && operatorFault(call, input, output); });

        const Vector_<> b = {1.0, 1.0};
        Vector_<> x = {0.0, 0.0};
        const auto solve = [&]() { RunSolver(biConjugate, *matrix, b, 1e-12, 0.0, 10, &x); };

        if (fault == 2) {
            ASSERT_THROW(solve(), CallbackSentinel_);
        } else {
            const char* category = fault == 0 ? (site == 3 || site == 4 ? "invalid preconditioner result" : "invalid operator result")
                                              : (site == 3 || site == 4 ? "non-finite preconditioner result" : "non-finite operator result");
            const char* callback = site == 2   ? "MultiplyRight"
                                   : site == 3 ? "PreConditionerSolveLeft"
                                   : site == 4 ? "PreConditionerSolveRight"
                                               : "MultiplyLeft";
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

        const std::array<CallbackCounts_, 6> cgExpected = {
            CallbackCounts_{1, 0, 0, 0},
            CallbackCounts_{2, 0, 0, 0},
            CallbackCounts_{0, 0, 0, 0},
            CallbackCounts_{1, 0, 1, 0},
            CallbackCounts_{0, 0, 0, 0},
            CallbackCounts_{4, 0, 0, 0}};
        const std::array<CallbackCounts_, 6> bcgExpected = {
            CallbackCounts_{1, 0, 0, 0},
            CallbackCounts_{2, 0, 0, 0},
            CallbackCounts_{2, 1, 0, 0},
            CallbackCounts_{1, 0, 1, 0},
            CallbackCounts_{1, 0, 1, 1},
            CallbackCounts_{4, 2, 0, 0}};
        const CallbackCounts_& expected = biConjugate ? bcgExpected[site] : cgExpected[site];
        ASSERT_EQ(expected.left_, counts.left_);
        ASSERT_EQ(expected.right_, counts.right_);
        ASSERT_EQ(expected.preconditionerLeft_, counts.preconditionerLeft_);
        ASSERT_EQ(expected.preconditionerRight_, counts.preconditionerRight_);
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
