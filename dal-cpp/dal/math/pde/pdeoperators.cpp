//
// Created by dal-implementer on 2026/7/8.
//

#include <memory>

#include <dal/math/pde/pdeoperators.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::PDE {
    namespace {
        void RequireOperatorLocations(const Vector_<>& x) {
            REQUIRE(x.size() >= 3, "operator builder requires at least 3 strictly increasing locations");
            for (int i = 1; i < static_cast<int>(x.size()); ++i)
                REQUIRE(x[i - 1] < x[i], "operator builder requires at least 3 strictly increasing locations");
        }
    } // namespace

    std::unique_ptr<Sparse::TriDiagonal_> NewDx(const Vector_<>& x) {
        RequireOperatorLocations(x);
        const int n = static_cast<int>(x.size());
        std::unique_ptr<Sparse::TriDiagonal_> ret(new Sparse::TriDiagonal_(n));
        for (int i = 1; i < n - 1; ++i) {
            const double dxl = x[i] - x[i - 1];
            const double dxu = x[i + 1] - x[i];
            const double dxm = dxl + dxu;
            ret->Set(i, i - 1, -dxu / (dxl * dxm));
            ret->Set(i, i, (dxu - dxl) / (dxl * dxu));
            ret->Set(i, i + 1, dxl / (dxu * dxm));
        }
        return ret;
    }

    std::unique_ptr<Sparse::TriDiagonal_> NewDxx(const Vector_<>& x) {
        RequireOperatorLocations(x);
        const int n = static_cast<int>(x.size());
        std::unique_ptr<Sparse::TriDiagonal_> ret(new Sparse::TriDiagonal_(n));
        for (int i = 1; i < n - 1; ++i) {
            const double dxl = x[i] - x[i - 1];
            const double dxu = x[i + 1] - x[i];
            const double dxm = dxl + dxu;
            ret->Set(i, i - 1, 2.0 / (dxl * dxm));
            ret->Set(i, i, -2.0 / (dxl * dxu));
            ret->Set(i, i + 1, 2.0 / (dxu * dxm));
        }
        return ret;
    }
} // namespace Dal::PDE
