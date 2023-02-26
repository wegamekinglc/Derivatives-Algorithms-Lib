//
// Created by wegam on 2023/2/25.
//

#include <dal/math/pde/finitedifference.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>


namespace Dal::PDE {

    namespace {
        void solve(const Matrix_<>& A, const Vector_<>& r, Vector_<>& u) {

        }
    }

    void FD1D_::Init(int numV, const Vector_<>& x, bool log) {
        x_ = x;
        res_.Resize(numV);

        r_ = Vector_<>(x_.size(), 0.0);
        mu_ = Vector_<>(x_.size(), 0.0);
        var_ = Vector_<>(x_.size(), 0.0);

        FiniteDifference_::Dx(-1, x_, dxd_);
        FiniteDifference_::Dx(0, x_, dx_);
        FiniteDifference_::Dx(1, x_, dxu_);
        FiniteDifference_::Dxx(x_, dxx_);

        int numC = dxx_.Cols();

        log_ = log;
        if (log_) {
            int n = x_.size() - 1;
            for (int i = 1; i < n; ++i) {
                for (int j = 0; j < numC; ++j)
                    dxx_(i, j) -= dx_(i, j);
            }
        }

        A_.Resize(x_.size(), numC);
        vs_.Resize(x_.size());
        ws_.Resize(x_.size());
    }

    void FD1D_::CalcAx(double one, double dtTheta, int wind, bool tr, Matrix_<>& A) const {
        int n = x_.size();
        int m = dxx_.Cols();
        int mm = m / 2;

        A.Resize(n, dxx_.Cols());

        int i, j;

        const Matrix_<>* Dx = 0;
        if (wind < 0)
            Dx = &dxd_;
        else if (wind == 0)
            Dx = &dx_;
        else if (wind == 1)
            Dx = &dxu_;

        for (i = 0; i < n; ++i) {
            if (wind > 1)
                Dx = mu_(i) < 0.0 ? &dxd_ : &dxu_;

            for (j = 0; j < m; ++j)
                A(i, j) = dtTheta * (mu_(i) * (*Dx)(i, j) + 0.5 * var_(i) * dxx_(i, j));

            A(i, mm) += one - dtTheta * r_(i);
        }

        if (tr)
            A = Dal::Matrix::MakeTranspose(A);
    }

    void FD1D_::RollBwd(double dt, double theta, int wind, Vector_<Vector_<>>& res) {
        int n = x_.size();
        int mm = dxx_.Cols() / 2;
        int numV = res.size();

        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta), wind, false, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
                Dal::Matrix::Multiply(A_, vs_, &res[k]);
            }
        }

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta, wind, false, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
//                kMatrixAlgebra::tridag(A_, vs_, res[k], ws_);
                solve(A_, vs_, res[k]);
            }
        }

    }

    void FD1D_::RollFwd(double dt, double theta, int wind, Vector_<Vector_<>>& res) {
        int numV = res.size();
        int mm = dxx_.Cols() / 2;

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta, wind, true, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
//                kMatrixAlgebra::tridag(A_, vs_, res[k], ws_)
                solve(A_, vs_, res[k]);
            }
        }

        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta), wind, true, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
                Dal::Matrix::Multiply(A_, vs_, &res[k]);
            }
        }
    }
} // namespace Dal::PDE