//
// Created by wegam on 2026/4/23.
//

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include <dal/platform/platform.hpp>

#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/utilities/dictionary.hpp>

using Dal::Cell_;
using Dal::Dictionary_;
using Dal::Exception_;
using Dal::Matrix_;
using Dal::SquareMatrix_;
using Dal::UnderdeterminedControls_;
using Dal::Vector_;
using Dal::Sparse::SymmetricDecomposition_;
using Dal::Sparse::TriDiagonal_;

namespace Underdetermined = Dal::Underdetermined;

namespace {
    UnderdeterminedControls_ MakeControls(int maxEvaluations = 20, int maxRestarts = 10) {
        Dictionary_ dict;
        dict.Insert("MAXEVALUATIONS", Cell_(static_cast<double>(maxEvaluations)));
        dict.Insert("MAXRESTARTS", Cell_(static_cast<double>(maxRestarts)));
        return UnderdeterminedControls_(dict);
    }

    void SetDiagonalWeights(TriDiagonal_* weights, double w0, double w1) {
        weights->Set(0, 0, w0);
        weights->Set(1, 1, w1);
    }

    class LinearSumFunc_ : public Underdetermined::Function_ {
        double target_;

    public:
        explicit LinearSumFunc_(double target) : target_(target) {}

        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] + x[1] - target_}; }
    };

    class DenseJacobian_ : public Underdetermined::Jacobian_ {
        Matrix_<> j_;

    public:
        explicit DenseJacobian_(const Matrix_<>& j) : j_(j) {}

        [[nodiscard]] int Rows() const override { return j_.Rows(); }
        [[nodiscard]] int Columns() const override { return j_.Cols(); }

        void DivideRows(const Vector_<>& tol) override {
            for (int iRow = 0; iRow < j_.Rows(); ++iRow)
                for (int iCol = 0; iCol < j_.Cols(); ++iCol)
                    j_(iRow, iCol) /= tol[iRow];
        }

        [[nodiscard]] Vector_<> MultiplyLeft(const Vector_<>& dx) const override {
            Vector_<> retVal(j_.Rows(), 0.0);
            for (int iRow = 0; iRow < j_.Rows(); ++iRow)
                for (int iCol = 0; iCol < j_.Cols(); ++iCol)
                    retVal[iRow] += j_(iRow, iCol) * dx[iCol];
            return retVal;
        }

        [[nodiscard]] Vector_<> MultiplyRight(const Vector_<>& t) const override {
            Vector_<> retVal(j_.Cols(), 0.0);
            for (int iCol = 0; iCol < j_.Cols(); ++iCol)
                for (int iRow = 0; iRow < j_.Rows(); ++iRow)
                    retVal[iCol] += t[iRow] * j_(iRow, iCol);
            return retVal;
        }

        void QForm(const SymmetricDecomposition_& w, SquareMatrix_<>* form) const override { w.QForm(j_, form); }

        void SecantUpdate(const Vector_<>&, const Vector_<>&) override {}
    };

    struct NonlinearSquareGradientState_ {
        Vector_<Vector_<>> xs_;
        Vector_<Vector_<>> fs_;
    };

    class NonlinearSquareAnalyticFunc_ : public Underdetermined::Function_ {
        NonlinearSquareGradientState_* gradientState_;

    public:
        explicit NonlinearSquareAnalyticFunc_(NonlinearSquareGradientState_* gradientState) : gradientState_(gradientState) {}

        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] * x[0] - 4.0}; }

        [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
            gradientState_->xs_.push_back(x);
            gradientState_->fs_.push_back(f);
            Matrix_<> j(1, 1);
            j(0, 0) = 2.0 * x[0];
            return new DenseJacobian_(j);
        }
    };

    class NonlinearSquareDenseFunc_ : public Underdetermined::Function_ {
    public:
        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] * x[0] - 4.0}; }
    };

    class BacktrackProbeFunc_ : public Underdetermined::Function_ {
        Vector_<Vector_<>>& evaluations_;

    public:
        explicit BacktrackProbeFunc_(Vector_<Vector_<>>& evaluations) : evaluations_(evaluations) {}

        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override {
            evaluations_.push_back(x);
            if (evaluations_.size() == 3)
                THROW("backtrack probe complete");
            return Vector_<>{1.0 - x[0] - 3.0 * x[0] * x[0]};
        }

        [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>&, const Vector_<>&) const override {
            Matrix_<> j(1, 2, 0.0);
            j(0, 0) = -1.0;
            return new DenseJacobian_(j);
        }
    };

    class MultiResidualFunc_ : public Underdetermined::Function_ {
        mutable int fastCalls_;

    public:
        MultiResidualFunc_() : fastCalls_(0) {}

        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] + x[2] - 3.0, x[1] + x[2] - 4.0}; }

        [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>&, const Vector_<>&) const override {
            Matrix_<> j(2, 3, 0.0);
            j(0, 0) = 1.0;
            j(0, 2) = 1.0;
            j(1, 1) = 1.0;
            j(1, 2) = 1.0;
            return new DenseJacobian_(j);
        }

        [[nodiscard]] int FastCalls() const { return fastCalls_; }

    private:
        void FFast(const Vector_<>&, Vector_<>*) const override {
            ++fastCalls_;
            THROW("custom Jacobian path should not call finite-difference fallback");
        }
    };

    // Probe function: same residual as MultiResidualFunc_, but records every x passed to Gradient so a
    // test can tell whether the solver's convergence-branch hook fired. Used to verify the solver does
    // NOT call the at-solution Gradient when the caller passes nullptr for fwdJacobianAtSolution.
    class CountingResidualFunc_ : public Underdetermined::Function_ {
        mutable Vector_<Vector_<>> gradientXs_;

    public:
        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] + x[2] - 3.0, x[1] + x[2] - 4.0}; }

        [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>&) const override {
            gradientXs_.push_back(x);
            Matrix_<> j(2, 3, 0.0);
            j(0, 0) = 1.0;
            j(0, 2) = 1.0;
            j(1, 1) = 1.0;
            j(1, 2) = 1.0;
            return new DenseJacobian_(j);
        }

        [[nodiscard]] const Vector_<Vector_<>>& GradientXs() const { return gradientXs_; }
    };

    bool WasGradientCalledAt(const Vector_<Vector_<>>& xs, const Vector_<>& target, double tol) {
        for (const auto& x : xs) {
            bool match = true;
            for (int i = 0; i < static_cast<int>(x.size()); ++i)
                match = match && std::abs(x[i] - target[i]) <= tol;
            if (match)
                return true;
        }
        return false;
    }

    // Probe that records every (x, f) pair passed to Gradient. The solver's convergence-branch hook
    // must pass the UNSCALED residual func.F(xNew); the per-restart path (XScaledFunc_::J) intentionally
    // passes the scaled residual, so only the convergence call -- the one at the solution x -- is
    // checked. With tol != 1 the two residuals differ, making a scaled convergence f detectable.
    class ResidualCheckingFunc_ : public Underdetermined::Function_ {
        mutable Vector_<Vector_<>> gradientXs_;
        mutable Vector_<Vector_<>> gradientFs_;

    public:
        [[nodiscard]] Vector_<> F(const Vector_<>& x) const override { return Vector_<>{x[0] + x[2] - 3.0, x[1] + x[2] - 4.0}; }

        [[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override {
            gradientXs_.push_back(x);
            gradientFs_.push_back(f);
            Matrix_<> j(2, 3, 0.0);
            j(0, 0) = 1.0;
            j(0, 2) = 1.0;
            j(1, 1) = 1.0;
            j(1, 2) = 1.0;
            return new DenseJacobian_(j);
        }

        // f passed to the Gradient call at the solution (the convergence call), or empty if Gradient
        // was never called there.
        [[nodiscard]] Vector_<> FAtSolution(const Vector_<>& solution, double tol) const {
            for (int i = 0; i < static_cast<int>(gradientXs_.size()); ++i) {
                bool match = true;
                for (int k = 0; k < static_cast<int>(solution.size()); ++k)
                    match = match && std::abs(gradientXs_[i][k] - solution[k]) <= tol;
                if (match)
                    return gradientFs_[i];
            }
            return Vector_<>();
        }
    };
} // namespace

TEST(UnderdeterminedTest, TestFindRespectsWeights) {
    LinearSumFunc_ func(3.0);
    Vector_<> guess = {0.0, 0.0};
    Vector_<> tol = {1.0e-10};

    TriDiagonal_ weights(2);
    SetDiagonalWeights(&weights, 1.0, 4.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Vector_<> calculated = Underdetermined::Find(func, guess, tol, *decomp, MakeControls());

    ASSERT_NEAR(calculated[0], 2.4, 1e-10);
    ASSERT_NEAR(calculated[1], 0.6, 1e-10);
    ASSERT_NEAR(func.F(calculated)[0], 0.0, 1e-10);
}

TEST(UnderdeterminedTest, TestApproximateBalancesFitAndDistance) {
    LinearSumFunc_ func(3.0);
    Vector_<> guess = {0.0, 0.0};
    Vector_<> funcTol = {1.0};

    TriDiagonal_ weights(2);
    SetDiagonalWeights(&weights, 1.0, 1.0);
    Vector_<> calculated = Underdetermined::Approximate(func, guess, funcTol, 1.0, weights, MakeControls());

    ASSERT_NEAR(calculated[0], 1.0, 1e-10);
    ASSERT_NEAR(calculated[1], 1.0, 1e-10);
    ASSERT_NEAR(func.F(calculated)[0], -1.0, 1e-10);
}

TEST(UnderdeterminedTest, TestFindWithCustomJacobianAndMultipleResiduals) {
    MultiResidualFunc_ func;
    Vector_<> guess = {0.0, 0.0, 0.0};
    Vector_<> tol = {1.0e-10, 1.0e-10};

    TriDiagonal_ weights(3);
    weights.Set(0, 0, 1.0);
    weights.Set(1, 1, 1.0);
    weights.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Vector_<> calculated = Underdetermined::Find(func, guess, tol, *decomp, MakeControls());

    ASSERT_NEAR(calculated[0], 2.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[1], 5.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[2], 7.0 / 3.0, 1e-10);
    ASSERT_NEAR(func.F(calculated)[0], 0.0, 1e-10);
    ASSERT_NEAR(func.F(calculated)[1], 0.0, 1e-10);
    ASSERT_EQ(func.FastCalls(), 0);
}

TEST(UnderdeterminedTest, TestFindThrowsWhenControlsAreExhausted) {
    LinearSumFunc_ func(3.0);
    Vector_<> guess = {0.0, 0.0};
    Vector_<> tol = {1.0e-10};

    TriDiagonal_ weights(2);
    SetDiagonalWeights(&weights, 1.0, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    ASSERT_THROW(Underdetermined::Find(func, guess, tol, *decomp, MakeControls(1, 1)), Exception_);
}

TEST(UnderdeterminedTest, TestFindUsesQuadraticBacktrackMinimum) {
    Vector_<Vector_<>> evaluations;
    BacktrackProbeFunc_ func(evaluations);
    const Vector_<> guess = {0.0, 0.0};
    const Vector_<> tol = {0.1};

    TriDiagonal_ weights(2);
    SetDiagonalWeights(&weights, 1.0, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    ASSERT_THROW(Underdetermined::Find(func, guess, tol, *decomp, MakeControls()), Exception_);
    ASSERT_EQ(evaluations.size(), 3u);

    // f_old=1 and f_new=-3, so Q(k)=|k*f_old+(1-k)*f_new|^2 is minimized at k=3/4.
    // The full Gauss-Newton step is x=1, hence retaining 1-k=1/4 of it evaluates x=0.25.
    ASSERT_NEAR(evaluations[2][0], 0.25, 1e-12);
    ASSERT_NEAR(evaluations[2][1], 0.0, 1e-12);
}

TEST(UnderdeterminedTest, TestFindPopulatesEffectiveJacobianInverse) {
    LinearSumFunc_ func(3.0);
    Vector_<> guess = {0.0, 0.0};
    Vector_<> tol = {1.0e-10};

    TriDiagonal_ weights(2);
    SetDiagonalWeights(&weights, 1.0, 4.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Matrix_<> effJacobianInverse;
    Vector_<> calculated = Underdetermined::Find(func, guess, tol, *decomp, MakeControls(), &effJacobianInverse);

    ASSERT_NEAR(calculated[0], 2.4, 1e-10);
    ASSERT_NEAR(calculated[1], 0.6, 1e-10);
    ASSERT_EQ(effJacobianInverse.Rows(), 2);
    ASSERT_EQ(effJacobianInverse.Cols(), 1);
    ASSERT_NEAR(effJacobianInverse(0, 0), 8.0e-11, 1e-18);
    ASSERT_NEAR(effJacobianInverse(1, 0), 2.0e-11, 1e-18);
}

TEST(UnderdeterminedTest, TestEffectiveInverseUsesFreshAnalyticJacobianAndUnscaledResidualAtSolution) {
    NonlinearSquareGradientState_ gradientState;
    NonlinearSquareAnalyticFunc_ func(&gradientState);
    const Vector_<> guess = {1.0};
    const Vector_<> tolerance = {1.0e-8};
    TriDiagonal_ weights(1);
    weights.Set(0, 0, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomposition(weights.DecomposeSymmetric());

    Matrix_<> effectiveInverse;
    const Vector_<> solved = Underdetermined::Find(func, guess, tolerance, *decomposition, MakeControls(), &effectiveInverse);
    ASSERT_NEAR(solved[0], 2.0, 1.0e-8);
    ASSERT_EQ(effectiveInverse.Rows(), 1);
    ASSERT_EQ(effectiveInverse.Cols(), 1);
    ASSERT_NEAR(effectiveInverse(0, 0) / tolerance[0], 1.0 / (2.0 * solved[0]), 1.0e-12);

    int solutionGradient = -1;
    for (int i = 0; i < static_cast<int>(gradientState.xs_.size()); ++i)
        if (std::abs(gradientState.xs_[i][0] - solved[0]) < 1.0e-12)
            solutionGradient = i;
    ASSERT_GE(solutionGradient, 0);
    ASSERT_NEAR(gradientState.fs_[solutionGradient][0], func.F(solved)[0], 1.0e-16);
}

TEST(UnderdeterminedTest, TestEffectiveInverseUsesFreshDenseFiniteDifferenceJacobianAtSolution) {
    NonlinearSquareDenseFunc_ func;
    const Vector_<> guess = {1.0};
    const Vector_<> tolerance = {1.0e-8};
    TriDiagonal_ weights(1);
    weights.Set(0, 0, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomposition(weights.DecomposeSymmetric());

    Matrix_<> effectiveInverse;
    const Vector_<> solved = Underdetermined::Find(func, guess, tolerance, *decomposition, MakeControls(), &effectiveInverse);
    constexpr double finiteDifferenceBump = 1.0e-4;
    ASSERT_NEAR(solved[0], 2.0, 1.0e-8);
    ASSERT_EQ(effectiveInverse.Rows(), 1);
    ASSERT_EQ(effectiveInverse.Cols(), 1);
    ASSERT_NEAR(effectiveInverse(0, 0) / tolerance[0], 1.0 / (2.0 * solved[0] + finiteDifferenceBump), 1.0e-10);
}

// Forward diagnostics preserve the raw analytic Jacobian rather than the tolerance-scaled solver Jacobian.

TEST(UnderdeterminedTest, TestFindPopulatesForwardJacobianAtSolution) {
    MultiResidualFunc_ func;
    Vector_<> guess = {0.0, 0.0, 0.0};
    Vector_<> tol = {1.0e-10, 1.0e-10};

    TriDiagonal_ weights(3);
    weights.Set(0, 0, 1.0);
    weights.Set(1, 1, 1.0);
    weights.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Matrix_<> fwdJacobian;
    Vector_<> calculated = Underdetermined::Find(func, guess, tol, *decomp, MakeControls(), nullptr, &fwdJacobian);

    ASSERT_NEAR(calculated[0], 2.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[1], 5.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[2], 7.0 / 3.0, 1e-10);

    ASSERT_EQ(fwdJacobian.Rows(), 2);
    ASSERT_EQ(fwdJacobian.Cols(), 3);
    ASSERT_NEAR(fwdJacobian(0, 0), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(0, 1), 0.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(0, 2), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 0), 0.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 1), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 2), 1.0, 1e-12);
}

// Analytic gradients may consume the residual, so diagnostics must pass it unscaled.

TEST(UnderdeterminedTest, TestFindPassesUnscaledResidualToAtSolutionGradient) {
    ResidualCheckingFunc_ func;
    Vector_<> guess = {0.0, 0.0, 0.0};
    Vector_<> tol = {1.0e-3, 1.0e-3};

    TriDiagonal_ weights(3);
    weights.Set(0, 0, 1.0);
    weights.Set(1, 1, 1.0);
    weights.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Matrix_<> fwdJacobian;
    const Vector_<> solved = Underdetermined::Find(func, guess, tol, *decomp, MakeControls(), nullptr, &fwdJacobian);
    ASSERT_FALSE(fwdJacobian.Empty());

    const Vector_<> fAtSolution = func.FAtSolution(solved, 1e-9);
    ASSERT_EQ(fAtSolution.size(), 2);
    const Vector_<> expected = func.F(solved);
    ASSERT_NEAR(fAtSolution[0], expected[0], 1e-12);
    ASSERT_NEAR(fAtSolution[1], expected[1], 1e-12);
}

// At-solution gradient work is diagnostic-only and must be skipped when no output is requested.

TEST(UnderdeterminedTest, TestFindSkipsAtSolutionGradientWhenOutParamNull) {
    const Vector_<> guess = {0.0, 0.0, 0.0};
    const Vector_<> tol = {1.0e-10, 1.0e-10};

    CountingResidualFunc_ funcNull;
    TriDiagonal_ weightsNull(3);
    weightsNull.Set(0, 0, 1.0);
    weightsNull.Set(1, 1, 1.0);
    weightsNull.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decompNull(weightsNull.DecomposeSymmetric());
    const Vector_<> solved = Underdetermined::Find(funcNull, guess, tol, *decompNull, MakeControls(), nullptr, nullptr);
    const auto& xsNull = funcNull.GradientXs();
    const int nGradientNoOut = static_cast<int>(xsNull.size());

    CountingResidualFunc_ funcOut;
    TriDiagonal_ weightsOut(3);
    weightsOut.Set(0, 0, 1.0);
    weightsOut.Set(1, 1, 1.0);
    weightsOut.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decompOut(weightsOut.DecomposeSymmetric());
    Matrix_<> fwdJacobian;
    const Vector_<> solvedOut = Underdetermined::Find(funcOut, guess, tol, *decompOut, MakeControls(), nullptr, &fwdJacobian);
    const auto& xsOut = funcOut.GradientXs();

    ASSERT_EQ(static_cast<int>(xsOut.size()), nGradientNoOut + 1);
    for (int i = 0; i < static_cast<int>(solved.size()); ++i)
        ASSERT_NEAR(solvedOut[i], solved[i], 1e-10);

    ASSERT_FALSE(WasGradientCalledAt(xsNull, solved, 1e-9));
    ASSERT_TRUE(WasGradientCalledAt(xsOut, solved, 1e-9));
}
