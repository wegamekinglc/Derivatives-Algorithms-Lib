//
// Created by wegam on 2023/2/25.
//

#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    void FD1D_::Init() {

        dx_.reset(Dx(x_));
        dxx_.reset(Dxx(x_));

        A_ = std::make_unique<Sparse::TriDiagonal_>(dx_->Size());
        vs_.Resize(dx_->Size());
    }

    void FD1D_::CalcAx(double one, double dtTheta) {
        int n = dx_->Size();

        for (int i = 0; i < n; ++i) {
            if (i != 0 && i != n -1) {
                A_->Set(i, i - 1, dtTheta * (mu_(i) * (*dx_)(i, i - 1) + 0.5 * var_(i) * (*dxx_)(i, i - 1)));
                A_->Set(i, i + 1, dtTheta * (mu_(i) * (*dx_)(i, i + 1) + 0.5 * var_(i) * (*dxx_)(i, i + 1)));
                A_->Set(i, i, dtTheta * (mu_(i) * (*dx_)(i, i) + 0.5 * var_(i) * (*dxx_)(i, i)) + one - dtTheta * r_(i));
            }
            else
                A_->Set(i, i, 1.0);
        }
    }

    void FD1D_::RollBwd(double dt, double theta, Vector_<>& res) {
        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta));
            vs_ = res;
            A_->MultiplyLeft(vs_, &res);
        }

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta);
            vs_ = res;
            std::unique_ptr<SquareMatrixDecomposition_> comp(A_->Decompose());
            comp->SolveLeft(vs_, &res);
        }
    }
} // namespace Dal::PDE