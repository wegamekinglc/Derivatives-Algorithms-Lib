//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <utility>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/utilities/numerics.hpp>

namespace Dal {
    // Dense Jacobian subclass for curve calibration. Storage is dense regardless of how the matrix
    // is filled; assembly is sparse-by-row because AAD produces exact structural zeros. Used by both
    // the single-curve AAD path (calibration.cpp) and the joint multi-curve AAD path
    // (jointcalibration.cpp). The method bodies mirror XJDense_ (underdetermined.cpp) -- the
    // storage is dense regardless of how the matrix is filled. Declared in Dal:: (not anonymous) so
    // the calibration flow can construct it and the solver's virtual Jacobian_ interface
    // (MultiplyLeft) can read its contents without an inline accessor.
    struct XCurveJacobian_ : Underdetermined::Jacobian_ {
        Matrix_<> j_;
        explicit XCurveJacobian_(Matrix_<>&& j) : j_(std::move(j)) {}

        [[nodiscard]] int Rows() const override { return j_.Rows(); }
        [[nodiscard]] int Columns() const override { return j_.Cols(); }

        void DivideRows(const Vector_<>& tol) override {
            for (int ii = 0; ii < j_.Rows(); ++ii) {
                auto row = j_.Row(ii);
                Transform(&row, [&tol, &ii](double x) { return 1.0 / tol[ii] * x; });
            }
        }

        [[nodiscard]] Vector_<> MultiplyRight(const Vector_<>& t) const override {
            Vector_<> retval;
            Matrix::Multiply(t, j_, &retval);
            return retval;
        }
        [[nodiscard]] Vector_<> MultiplyLeft(const Vector_<>& dx) const override {
            Vector_<> retval;
            Matrix::Multiply(j_, dx, &retval);
            return retval;
        }

        void QForm(const Sparse::SymmetricDecomposition_& w, SquareMatrix_<>* form) const override { w.QForm(j_, form); }

        void SecantUpdate(const Vector_<>& dx, const Vector_<>& df) override {
            const auto nf = df.size();
            const double x2 = InnerProduct(dx, dx);
            for (int ii = 0; ii < nf; ++ii) {
                auto row = j_.Row(ii);
                const double excess = df[ii] - InnerProduct(dx, row);
                Transform(&row, dx, LinearIncrement(excess / x2));
            }
        }
    };
} // namespace Dal
