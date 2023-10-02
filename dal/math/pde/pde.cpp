//
// Created by wegam on 2023/2/24.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/pde/pde.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal::PDE {
    namespace {
        struct IdentityMap_ : CoordinateMap_ {
            double operator()(double y, double* xp = nullptr, double* xpp = nullptr) const override {
                ASSIGN(xp, 1.0);
                ASSIGN(xpp, 0.0);
                return y;
            }
            [[nodiscard]] double Y(double x) const override { return x; }
        };

        struct SinhMap_ : CoordinateMap_ {
            // x = \lambda sinh(y / \lambda)
            double lambda_;
            explicit SinhMap_(double lambda) : lambda_(lambda) {}
            double operator()(double y, double* xp = nullptr, double* xpp = nullptr) const override {
                ASSIGN(xp, std::cosh(y / lambda_));
                ASSIGN(xpp, std::sinh(y / lambda_) / lambda_);
                return lambda_ * std::sinh(y / lambda_);
            }
            [[nodiscard]] double Y(double x) const override { return lambda_ * asinh(x / lambda_); }
        };
    } // namespace

    CoordinateMap_* NewSinhMap(double x_width, double dxdy_range) {
        REQUIRE(IsPositive(x_width) && dxdy_range >= 1.0, "x_width should be positive and dxdy_range should be greater than 1");
        double sinhMaxY = std::sqrt(Square(dxdy_range) - 1.0);
        return IsZero(Square(sinhMaxY)) ? (CoordinateMap_*)new IdentityMap_ : new SinhMap_(x_width / sinhMaxY);
    }
} // namespace Dal::PDE