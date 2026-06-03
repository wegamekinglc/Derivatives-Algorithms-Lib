//
// Created by wegam on 2023/2/25.
//

#pragma once

#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/pde/meshers/fdm1dmesher.hpp>

namespace Dal::PDE {

    class FD1D_ {
    public:
       explicit FD1D_(const FDM1DMesher_& x): x_(x) {};
        void Init();

        [[nodiscard]] const Vector_<>& R() const { return r_; }
        Vector_<>& R() { return r_; }
        [[nodiscard]] const Vector_<>& Mu() const { return mu_; }
        Vector_<>& Mu() { return mu_; }
        [[nodiscard]] const Vector_<>& Var() const { return var_; }
        Vector_<>& Var() { return var_; }
        [[nodiscard]] const Vector_<>& Res() const { return res_; }
        Vector_<>& Res() { return res_; }
        [[nodiscard]] const Vector_<>& X() const { return x_.Locations(); }

        void CalcAx(double one, double dtTheta);
        void RollBwd(double dt, double theta, Vector_<>& res);

    private:
        const FDM1DMesher_& x_;
        Vector_<> r_;
        Vector_<> mu_;
        Vector_<> var_;

        std::unique_ptr<Sparse::TriDiagonal_> dx_;
        std::unique_ptr<Sparse::TriDiagonal_> dxx_;
        std::unique_ptr<Sparse::TriDiagonal_> A_;
        Vector_<> vs_;
        Vector_<> res_;
    };

} // namespace Dal::PDE