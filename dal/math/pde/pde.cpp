//
// Created by wegam on 2023/2/24.
//

#include <dal/math/pde/pde.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::PDE {
    namespace {
        struct IdentityMap_ : PDE::CoordinateMap_ {
            double operator()(double y, double* xp = nullptr, double* xpp = nullptr) const override {
                ASSIGN(xp, 1.0);
                ASSIGN(xpp, 0.0);
                return y;
            }
            double Y(double x) const override { return x; }
        };

        struct SinhMap_ : PDE::CoordinateMap_ {
            // x = \lambda sinh(y / \lambda)
            double lambda_;
            SinhMap_(double lambda) : lambda_(lambda) {}
            double operator()(double y, double* xp = nullptr, double* xpp = nullptr) const override {
                ASSIGN(xp, cosh(y / lambda_));
                ASSIGN(xpp, sinh(y / lambda_) / lambda_);
                return lambda_ * sinh(y / lambda_);
            }
            double Y(double x) const override { return lambda_ * asinh(x / lambda_); }
        };
    } // namespace

    PDE::CoordinateMap_* PDE::NewSinhMap(double x_width, double dxdy_range) {
        REQUIRE(IsPositive(x_width) && dxdy_range >= 1.0, "x_width should be positive and dxdy_range should be greater than 1");
        double sinhMaxY = sqrt(Square(dxdy_range) - 1.0);
        return IsZero(Square(sinhMaxY)) ? (CoordinateMap_*)new IdentityMap_ : new SinhMap_(x_width / sinhMaxY);
    }
} // namespace Dal::PDE