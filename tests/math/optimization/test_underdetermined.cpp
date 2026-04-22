//
// Created by wegam on 2026/4/23.
//

#include <memory>
#include <gtest/gtest.h>
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
