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
    // NOT call the at-solution Gradient when the caller passes nullptr for fwd_jacobian_at_solution.
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

// The trailing fwd_jacobian_at_solution out-param captures the UNSCALED analytic forward Jacobian at
// the solution via a single raw func.Gradient(xNew, fUnscaled) call on the convergence branch -- NOT
// the XScaledFunc_::J path, so DivideRows(tol) is never applied. When the caller passes nullptr the
// hook is skipped entirely (Gradient is not called at the solution); a Gradient that returns nullptr
// clears the output matrix rather than falling back to the dense finite-difference matrix.

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

    // Solution unchanged.
    ASSERT_NEAR(calculated[0], 2.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[1], 5.0 / 3.0, 1e-10);
    ASSERT_NEAR(calculated[2], 7.0 / 3.0, 1e-10);

    // Shape: nResiduals x nX, and the constant analytic J is captured unscaled (rows 1.0 in cols 0,2
    // and 1,1 in cols 1,2 -- MultiResidualFunc_'s DenseJacobian_).
    ASSERT_EQ(fwdJacobian.Rows(), 2);
    ASSERT_EQ(fwdJacobian.Cols(), 3);
    ASSERT_NEAR(fwdJacobian(0, 0), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(0, 1), 0.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(0, 2), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 0), 0.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 1), 1.0, 1e-12);
    ASSERT_NEAR(fwdJacobian(1, 2), 1.0, 1e-12);
}

// The convergence-branch hook must pass the UNSCALED residual func.F(xNew) to func.Gradient, not the
// scaled XScaledFunc_::F output. With tol far from 1 the two differ, so a ResidualCheckingFunc_
// (which asserts f == F(x) inside Gradient) trips if a scaled f is passed. This pins the contract
// for any future Function_ whose Gradient actually consumes f.

TEST(UnderdeterminedTest, TestFindPassesUnscaledResidualToAtSolutionGradient) {
    ResidualCheckingFunc_ func;
    Vector_<> guess = {0.0, 0.0, 0.0};
    // tol != 1 so the scaled residual differs from the unscaled one by the 1e-3 factor.
    Vector_<> tol = {1.0e-3, 1.0e-3};

    TriDiagonal_ weights(3);
    weights.Set(0, 0, 1.0);
    weights.Set(1, 1, 1.0);
    weights.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decomp(weights.DecomposeSymmetric());

    Matrix_<> fwdJacobian;
    const Vector_<> solved = Underdetermined::Find(func, guess, tol, *decomp, MakeControls(), nullptr, &fwdJacobian);
    ASSERT_FALSE(fwdJacobian.Empty());

    // The convergence-call f must be the UNSCALED residual, which is ~0 at the solution. If the hook
    // passed the scaled fNew, f would be ~0/1e-3 and still round to 0 -- so instead compare directly
    // against F(solution): the unscaled residual is exactly F(solved), the scaled one is F(solved)/tol.
    const Vector_<> fAtSolution = func.FAtSolution(solved, 1e-9);
    ASSERT_EQ(fAtSolution.size(), 2);
    const Vector_<> expected = func.F(solved);
    ASSERT_NEAR(fAtSolution[0], expected[0], 1e-12);
    ASSERT_NEAR(fAtSolution[1], expected[1], 1e-12);
}

// When the caller passes nullptr for fwd_jacobian_at_solution, the solver must NOT call the
// at-solution Gradient at all -- the convergence-branch hook is skipped entirely. This probes that
// contract directly: a CountingResidualFunc_ records every x passed to Gradient, and the solution x
// may appear in that record only when the out-param is supplied (then exactly once, as the
// convergence call). Replaces an earlier vacuous check that allocated a matrix it never passed in.

TEST(UnderdeterminedTest, TestFindSkipsAtSolutionGradientWhenOutParamNull) {
    const Vector_<> guess = {0.0, 0.0, 0.0};
    const Vector_<> tol = {1.0e-10, 1.0e-10};

    // Reference solution and the per-iteration Gradient-call count with no out-param.
    CountingResidualFunc_ funcNull;
    TriDiagonal_ weightsNull(3);
    weightsNull.Set(0, 0, 1.0);
    weightsNull.Set(1, 1, 1.0);
    weightsNull.Set(2, 2, 1.0);
    std::unique_ptr<SymmetricDecomposition_> decompNull(weightsNull.DecomposeSymmetric());
    const Vector_<> solved = Underdetermined::Find(funcNull, guess, tol, *decompNull, MakeControls(), nullptr, nullptr);
    const auto& xsNull = funcNull.GradientXs();
    const int nGradientNoOut = static_cast<int>(xsNull.size());

    // With the out-param supplied, the solver makes exactly one ADDITIONAL Gradient call -- the
    // convergence-branch call -- and that call is at the solution.
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

    // The solution was NOT a Gradient evaluation point when no out-param was requested...
    ASSERT_FALSE(WasGradientCalledAt(xsNull, solved, 1e-9));
    // ...and IS (exactly the one convergence call) when the out-param is supplied.
    ASSERT_TRUE(WasGradientCalledAt(xsOut, solved, 1e-9));
}
