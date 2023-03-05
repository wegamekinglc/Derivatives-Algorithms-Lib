//
// Created by wegam on 2023/2/25.
//

#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/pde/fdi1d.hpp>
#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    void FD1D_::Init() {
        r_ = Vector_<>(x_.size(), 0.0);
        mu_ = Vector_<>(x_.size(), 0.0);
        var_ = Vector_<>(x_.size(), 0.0);

        dx_.reset(Dx(x_));
        dxx_.reset(Dxx(x_));
        bcx_ = BCx(x_);
        bcxx_ = BCxx(x_);

        A_ = std::make_unique<Sparse::TriDiagonal_>(dx_->Size());
        vs_.Resize(dx_->Size());
    }

    void FD1D_::CalcAx(double one, double dtTheta) {
        int n = dx_->Size();

        for (int i = 0; i < n; ++i) {
            if (i != 0)
                A_->Set(i, i - 1, dtTheta * (mu_(i + 1) * (*dx_)(i, i - 1) + 0.5 * var_(i + 1) * (*dxx_)(i, i - 1)));

            if (i != n - 1)
                A_->Set(i, i + 1, dtTheta * (mu_(i + 1) * (*dx_)(i, i + 1) + 0.5 * var_(i + 1) * (*dxx_)(i, i + 1)));

            A_->Set(i, i, dtTheta * (mu_(i + 1) * (*dx_)(i, i) + 0.5 * var_(i + 1) * (*dxx_)(i, i)) + one - dtTheta * r_(i + 1));
        }
    }

    void FD1D_::CalcBC(double dtTheta, Vector_<>* v) const {
        int n = v->size();
        int r = res_.size();
        double bcl = res_[0] * dtTheta * (mu_(0) * bcx_(0) + 0.5 * var_(0) * bcxx_(0));
        double bcr = res_[r - 1] * dtTheta * (mu_(r - 1) * bcx_(1) + 0.5 * var_(r - 1) * bcxx_(1));
        (*v)[0] += bcl;
        (*v)[n - 1] += bcr;
    }

    void FD1D_::RollBwd(double dt, double theta, Vector_<>& res) {
        int n = res.size() - 2;
        Vector_<> tmp(n);
        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta));
            vs_ = Vector_<>(res.begin() + 1, res.end() - 1);
            A_->MultiplyLeft(vs_, &tmp);
            CalcBC(dt * (1.0 - theta), &tmp);
            for (int i = 0; i < n; ++i)
                res[i + 1] = tmp[i];
        }

        if (theta != 0.0) {
            CalcAx(1.0, -dt * theta);
            vs_ = Vector_<>(res.begin() + 1, res.end() - 1);
            CalcBC(dt * theta, &vs_);
            std::unique_ptr<SquareMatrixDecomposition_> comp(A_->Decompose());
            comp->SolveLeft(vs_, &tmp);
            for (int i = 0; i < n; ++i)
                res[i + 1] = tmp[i];
        }
    }
} // namespace Dal::PDE