//
// Created by wegam on 2023/2/25.
//

#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    void FD1D_::Init(int num_v) {
        res_.Resize(num_v);

        r_ = Vector_<>(x_.size(), 0.0);
        mu_ = Vector_<>(x_.size(), 0.0);
        var_ = Vector_<>(x_.size(), 0.0);

        dx_.reset(Dx(x_));
        dxx_.reset(Dxx(x_));

        A_ = std::make_unique<Sparse::TriDiagonal_>(x_.size());
        vs_.Resize(x_.size());
    }

    void FD1D_::CalcAx(double one, double dtTheta) {
        int n = x_.size();

        for (int i = 0; i < n; ++i) {
            if (i != 0)
                A_->Set(i, i - 1, dtTheta * (mu_(i) * (*dx_)(i, i - 1) + 0.5 * var_(i) * (*dxx_)(i, i - 1)));

            if (i != n - 1)
                A_->Set(i, i + 1, dtTheta * (mu_(i) * (*dx_)(i, i + 1) + 0.5 * var_(i) * (*dxx_)(i, i + 1)));

            A_->Set(i, i, dtTheta * (mu_(i) * (*dx_)(i, i) + 0.5 * var_(i) * (*dxx_)(i, i)) + one - dtTheta * r_(i));
        }
    }

    void FD1D_::RollBwd(double dt, double theta, Vector_<Vector_<>>& res) {
        int num_v = res.size();

        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta));
            for (int k = 0; k < num_v; ++k) {
                vs_ = res[k];
                A_->MultiplyLeft(vs_, &res[k]);
            }
        }

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta);
            for (int k = 0; k < num_v; ++k) {
                vs_ = res[k];
                A_->Decompose()->SolveLeft(vs_, &res[k]);
            }
        }
    }
} // namespace Dal::PDE