//
// Created by wegam on 2023/2/25.
//

#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    Sparse::TriDiagonal_* Dx(const Vector_<>& x) {
        int n = x.size();
        REQUIRE(n > 2, "grids size should not less then 3");

        Sparse::TriDiagonal_* rtn = new Sparse::TriDiagonal_(n);

        double dxl, dxm, dxu;
        rtn->Set(0, 0, 1.0);
        for (int i = 1; i < n - 1; ++i) {
            dxl = x[i] - x[i - 1];
            dxu = x[i + 1] - x[i];
            dxm = dxl + dxu;

            rtn->Set(i, i - 1, -dxu / dxl / dxm);
            rtn->Set(i, i, (dxu / dxl - dxl / dxu) / dxm);
            rtn->Set(i, i + 1, dxl / dxu / dxm);
        }
        rtn->Set(n - 1, n - 1, 1.0);
        return rtn;
    }

    Sparse::TriDiagonal_* Dxx(const Vector_<>& x) {
        int n = x.size();
        REQUIRE(n > 2, "grids size should not less then 3");
        Sparse::TriDiagonal_* rtn = new Sparse::TriDiagonal_(n);

        double dxl, dxu, dxm;
        rtn->Set(0, 0, 1.0);
        for (int i = 1; i < n - 1; ++i) {
            dxl = x[i] - x[i - 1];
            dxu = x[i + 1] - x[i];
            dxm = 0.5 * (dxl + dxu);
            rtn->Set(i, i - 1, 1.0 / dxl / dxm);
            rtn->Set(i, i, -(1.0 / dxl + 1.0 / dxu) / dxm);
            rtn->Set(i, i + 1, 1.0 / dxu / dxm);
        }
        rtn->Set(n - 1, n - 1, 1.0);
        return rtn;
    }

} // namespace Dal::PDE