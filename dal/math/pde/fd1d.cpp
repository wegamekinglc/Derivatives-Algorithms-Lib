//
// Created by wegam on 2023/2/25.
//

#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    namespace {
        void BanMul(const Matrix_<>& A, int m1, int m2, const Vector_<>& b, Vector_<>* x) {
            int n = A.Rows() - 1;
            double xi;
            for (int i = 0; i <= n; ++i) {
                int jl = std::max(0, i - m1);
                int ju = std::min(i + m2, n);
                xi = 0.0;
                for (int j = jl; j <= ju; ++j) {
                    int k = j - i + m1;
                    xi += A(i, k) * b(j);
                }
                (*x)(i) = xi;
            }
        }

        void Solve(const Matrix_<>& A, const Vector_<>& r, Vector_<>* u) {
            int n = A.Rows();
            if (u->size() < n)
                u->Resize(n);
            Vector_<> gam(n, 0.0);

            double bet = A(0, 1);

            (*u)(0) = r(0) / A(0, 1);
            for (int j = 1; j < n; ++j) {
                gam(j) = A(j - 1, 2) / bet;
                bet = A(j, 1) - A(j, 0) * gam(j);
                (*u)(j) = (r(j) - A(j, 0) * (*u)(j - 1)) / bet;
            }
            for (int j = n - 2; j >= 0; --j)
                (*u)(j) -= gam(j + 1) * (*u)(j + 1);
        }
    } // namespace

    void FD1D_::Init(int num_v, bool log) {
        res_.Resize(num_v);

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
                BanMul(A_, mm, mm, vs_, &res[k]);
            }
        }

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta, wind, false, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
                Solve(A_, vs_, &res[k]);
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
                Solve(A_, vs_, &res[k]);
            }
        }

        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta), wind, true, A_);
            for (int k = 0; k < numV; ++k) {
                vs_ = res[k];
                BanMul(A_, mm, mm, vs_, &res[k]);
            }
        }
    }
} // namespace Dal::PDE