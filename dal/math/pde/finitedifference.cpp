//
// Created by wegam on 2023/2/25.
//

#include <dal/math/pde/finitedifference.hpp>

namespace Dal::PDE {

    Sparse::TriDiagonal_* Dx(const Vector_<>& x) {
        int n = x.size() - 2;
        REQUIRE(n > 0, "grids size should not less then 1");

        Sparse::TriDiagonal_* rtn = new Sparse::TriDiagonal_(n);

        double dxl, dxm, dxu;
        for (int i = 0; i < n; ++i) {
            dxl = x[i + 1] - x[i];
            dxu = x[i + 2] - x[i + 1];
            dxm = dxl + dxu;

            if (i != 0)
                rtn->Set(i, i - 1, -dxu / dxl / dxm);
            rtn->Set(i, i, (dxu / dxl - dxl / dxu) / dxm);
            if (i != n - 1)
                rtn->Set(i, i + 1, dxl / dxu / dxm);
        }
        return rtn;
    }

    Sparse::TriDiagonal_* Dxx(const Vector_<>& x) {
        int n = x.size() - 2;
        REQUIRE(n > 0, "grids size should not less then 1");
        Sparse::TriDiagonal_* rtn = new Sparse::TriDiagonal_(n);

        double dxl, dxu, dxm;

        for (int i = 0; i < n; ++i) {
            dxl = x(i + 1) - x(i);
            dxu = x(i + 2) - x(i + 1);
            dxm = 0.5 * (dxl + dxu);
            if (i != 0)
                rtn->Set(i, i - 1, 1.0 / dxl / dxm);
            rtn->Set(i, i, -(1.0 / dxl + 1.0 / dxu) / dxm);
            if (i != n - 1)
                rtn->Set(i, i + 1, 1.0 / dxu / dxm);
        }
        return rtn;
    }

    Vector_<> BCx(const Vector_<>& x) {
        double dxl = x[1] - x[0];
        double dxu = x[2] - x[1];
        double dxm = dxl + dxu;
        double bclx = -dxu / dxl / dxm;

        int n = x.size() - 2;
        dxl = x[n] - x[n - 1];
        dxu = x[n + 1] - x[n];
        dxm = dxl + dxu;
        double bcrx = dxl / dxu / dxm;
        return {bclx, bcrx};
    }

    Vector_<> BCxx(const Vector_<>& x) {
        double dxl = x[1] - x[0];
        double dxu = x[2] - x[1];
        double dxm = 0.5 * (dxl + dxu);
        double bclxx = 1.0 / dxl / dxm;

        int n = x.size() - 2;
        dxl = x[n] - x[n - 1];
        dxu = x[n + 1] - x[n];
        dxm = 0.5 * (dxl + dxu);
        double bcrxx = 1.0 / dxu / dxm;
        return {bclxx, bcrxx};
    }

} // namespace Dal::PDE