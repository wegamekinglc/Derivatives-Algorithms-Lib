//
// Created by wegam on 2022/12/10.
//

#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/sparse.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_UnderdeterminedControls_object.hpp>

    namespace Underdetermined {
        void Function_::Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* j) const {
            Vector_<> fBase;
            Vector_<> fBumped;
            FFast(x, &fBase);
            Vector_<> xBumped(x);
            const double dx = BumpSize();
            auto scale = [&dx](double x) { return 1.0 / dx * x; };
            const int nx = x.size();
            j->Resize(f.size(), nx);
            for (int ix = 0; ix < nx; ++ix) {
                xBumped[ix] += dx;
                FFast(xBumped, &fBumped);
                fBumped -= fBase;
                auto col = j->Col(ix);
                Transform(fBumped, scale, &col);
                xBumped[ix] = x[ix];
            }
        }
    }

    namespace {
        struct XJDense_ : Underdetermined::Jacobian_ {
            Matrix_<>& j_;
            XJDense_(Matrix_<>& j) : j_(j) {}

            int Rows() const override { return j_.Rows(); }
            int Columns() const override { return j_.Cols(); }

            void DivideRows(const Vector_<>& tol) override {
                for (int ii = 0; ii < j_.Rows(); ++ii) {
                    auto row = j_.Row(ii);
                    Transform(&row, [&tol, &ii](double x) { return 1.0 / tol[ii] * x; });
                }
            }

            Vector_<> MultiplyRight(const Vector_<>& t) const override {
                Vector_<> retval;
                Matrix::Multiply(t, j_, &retval);
                return retval;
            }
            Vector_<> MultiplyLeft(const Vector_<>& dx) const override
            {
                Vector_<> retval;
                Matrix::Multiply(j_, dx, &retval);
                return retval;
            }

            void QForm(const Sparse::SymmetricDecomposition_& w, SquareMatrix_<>* form) const override
            {
                w.QForm(j_, form);
            }

            void SecantUpdate(const Vector_<>& dx, const Vector_<>& df) override
            {
                const int nf = df.size();
                const double x2 = InnerProduct(dx, dx);
                for (int ii = 0; ii < nf; ++ii) {
                    auto row = j_.Row(ii);
                    const double excess = df[ii] - InnerProduct(dx, row);
                    Transform(&row, dx, LinearIncrement(excess / x2));
                }
            }
        };
    }
}