//
// Created by wegam on 2023/2/25.
//


#include <dal/platform/platform.hpp>
#include <dal/math/pde/fd1d.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    void FD1D_::Init() {

        dx_.reset(Dx(x_));
        dxx_.reset(Dxx(x_));

        A_ = std::make_unique<Sparse::TriDiagonal_>(dx_->Size());
        vs_.Resize(dx_->Size());

        cachedDecomp_.reset();
        cachedDt_ = 0.0;
        cachedTheta_ = 0.0;
        cachedMu_.clear();
        cachedVar_.clear();
        cachedR_.clear();
        decompCount_ = 0;
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

    bool FD1D_::CacheHit(double dt, double theta) const {
        if (!cachedDecomp_)
            return false;
        if (dt != cachedDt_ || theta != cachedTheta_)
            return false;
        if (mu_ != cachedMu_ || var_ != cachedVar_ || r_ != cachedR_)
            return false;
        return true;
    }

    void FD1D_::RollBwd(double dt, double theta, Vector_<>& res) {
        if (theta != 1.0) {
            CalcAx(1.0, dt * (1.0 - theta));
            vs_ = res;
            A_->MultiplyLeft(vs_, &res);
        }

        if (theta != 0.0) {
            if (!CacheHit(dt, theta)) {
                CalcAx(1.0, -dt * theta);
                cachedDecomp_.reset(A_->Decompose());
                cachedDt_ = dt;
                cachedTheta_ = theta;
                cachedMu_ = mu_;
                cachedVar_ = var_;
                cachedR_ = r_;
                ++decompCount_;
            }
            vs_ = res;
            cachedDecomp_->SolveLeft(vs_, &res);
        }
    }
} // namespace Dal::PDE
