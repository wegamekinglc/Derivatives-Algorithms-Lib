//
// Created by dal-implementer on 2026/7/8.
//

#include <array>
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

        template <class Coeffs_>
        std::unique_ptr<Sparse::TriDiagonal_> NewInteriorOperator(const Vector_<>& x, Coeffs_ coeffs) {
            RequireOperatorLocations(x);
            const int n = static_cast<int>(x.size());
            std::unique_ptr<Sparse::TriDiagonal_> ret(new Sparse::TriDiagonal_(n));
            for (int i = 1; i < n - 1; ++i) {
                const double dxl = x[i] - x[i - 1];
                const double dxu = x[i + 1] - x[i];
                const double dxm = dxl + dxu;
                const auto c = coeffs(dxl, dxu, dxm);
                ret->Set(i, i - 1, c[0]);
                ret->Set(i, i, c[1]);
                ret->Set(i, i + 1, c[2]);
            }
            return ret;
        }

    } // namespace

    std::unique_ptr<Sparse::TriDiagonal_> NewDx(const Vector_<>& x) {
        return NewInteriorOperator(x, [](double dxl, double dxu, double dxm) {
            return std::array<double, 3>{-dxu / (dxl * dxm), (dxu - dxl) / (dxl * dxu), dxl / (dxu * dxm)};
        });
    }

    std::unique_ptr<Sparse::TriDiagonal_> NewDxx(const Vector_<>& x) {
        return NewInteriorOperator(x, [](double dxl, double dxu, double dxm) {
            return std::array<double, 3>{2.0 / (dxl * dxm), -2.0 / (dxl * dxu), 2.0 / (dxu * dxm)};
        });
    }
} // namespace Dal::PDE
