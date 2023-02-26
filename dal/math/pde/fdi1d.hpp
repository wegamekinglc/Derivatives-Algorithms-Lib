//
// Created by wegam on 2023/2/25.
//

#pragma once

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

namespace Dal::PDE {

    class FD1D_ {
    public:
        FD1D_(int num_t = 1): res_(num_t) {}
        void Init(int num_v, bool log);

        const Vector_<>& R() const { return r_; }
        Vector_<>& R() { return r_; }
        const Vector_<>& Mu() const { return mu_; }
        Vector_<>& Mu() { return mu_; }
        const Vector_<>& Var() const { return var_; }
        Vector_<>& Var() { return var_; }
        const Vector_<>& X() const { return x_; }
        Vector_<>& X() { return x_; }
        const Vector_<Vector_<>>& Res() const { return res_; }
        Vector_<Vector_<>>& Res() { return res_; }

        void CalcAx(double one, double dtTheta, int wind, bool tr, Matrix_<>& A) const;
        void RollBwd(double dt, double theta, int wind, Vector_<Vector_<>>& res);
        void RollFwd(double dt, double theta, int wind, Vector_<Vector_<>>& res);

    private:
        bool log_{false};
        Vector_<> x_;
        Vector_<> r_;
        Vector_<> mu_;
        Vector_<> var_;

        Matrix_<> dxd_;
        Matrix_<> dxu_;
        Matrix_<> dx_;
        Matrix_<> dxx_;
        Matrix_<> A_;
        Vector_<> vs_;
        Vector_<> ws_;
        Vector_<Vector_<>> res_;
    };

} // namespace Dal::PDE