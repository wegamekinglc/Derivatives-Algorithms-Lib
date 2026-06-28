//
// Created by wegam on 2020/12/16.
//

#include <dal/platform/strict.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable Cubic1
        Cubic-spline interpolator
&members
name is ?string
        Name of the object
x is number[]
        X-points (independent variables) of the interpolation
f is number[]
        Y-points (function values) corresponding to X
fpp is number[]
        Splined second derivatives of F at each X
-IF-------------------------------------------------------------------------*/

namespace {

    using namespace Dal;

} // namespace

namespace Dal {
    namespace {
        struct Cubic1_ : Interp1_ {
            Vector_<> x_, f_, fpp_;
            mutable size_t lastIndex_ = 0;
            double operator()(double x) const override;
            bool IsInBounds(double x) const override {
                return x >= x_.front() && x <= x_.back();
            } // simply forbid extrapolation

            Cubic1_(const String_& name,
                    const Vector_<>& x,
                    const Vector_<>& f,
                    const Interp::Boundary_& lhs,
                    const Interp::Boundary_& rhs);

            Cubic1_(const String_& name, const Vector_<>& x, const Vector_<>& f, const Vector_<>& fpp)
                : Interp1_(name), x_(x), f_(f), fpp_(fpp) {}

            void Write(Archive::Store_& dst) const override;
        };

        // based on Numerical Recipes' splint
        double Cubic1_::operator()(double x) const {
            const ptrdiff_t n = static_cast<ptrdiff_t>(x_.size());
            ptrdiff_t iGE;
            size_t idx = lastIndex_ < static_cast<size_t>(n) ? lastIndex_ : static_cast<size_t>(n - 1);
            if (x >= x_[idx]) {
                while (idx + 1 < static_cast<size_t>(n) && x_[idx + 1] <= x)
                    ++idx;
            } else {
                while (idx > 0 && x_[idx] > x)
                    --idx;
            }
            if ((idx + 1 == static_cast<size_t>(n) && x >= x_[idx]) ||
                (x_[idx] <= x && (idx + 1 == static_cast<size_t>(n) || x_[idx + 1] > x))) {
                iGE = (x_[idx] == x) ? static_cast<ptrdiff_t>(idx) : static_cast<ptrdiff_t>(idx + 1);
                lastIndex_ = idx;
            } else {
                auto pGE = LowerBound(x_, x);
                iGE = pGE - x_.begin();
                lastIndex_ = static_cast<size_t>(std::min(iGE, n - 1));
            }
            if (iGE < n && x_[iGE] == x)
                return f_[iGE];
            const ptrdiff_t iClamp = std::min(n - 1, std::max<ptrdiff_t>(1, iGE));
            const ptrdiff_t iLT = iClamp - 1;
            const double h = x_[iClamp] - x_[iLT];
            const double b = (x - x_[iLT]) / h;
            const double a = 1.0 - b;
            return a * f_[iLT] + b * f_[iClamp] -
                   a * b * ((1.0 + a) * fpp_[iLT] + (1.0 + b) * fpp_[iClamp]) * Square(h) / 6.0;
        }

        // the spline-fitting process
        Cubic1_::Cubic1_(const String_& name,
                         const Vector_<>& x,
                         const Vector_<>& f,
                         const Interp::Boundary_& lhs,
                         const Interp::Boundary_& rhs)
            : Interp1_(name), x_(x), f_(f), fpp_(f_.size()) {
            REQUIRE(x_.size() > 2 && IsMonotonic(x_), "x size should be greater than 2 and monotonic");
            REQUIRE(x_.size() == f_.size(), "x and f size should be same");
            const int n = static_cast<int>(x_.size());
            Vector_<> u(n - 1);
            switch (lhs.order_) // set left boundary
            {
            default:
                THROW("Invalid boundary order");
            case 1: {
                const double dx = x_[1] - x_[0];
                fpp_[0] = ((f_[1] - f_[0]) / dx - lhs.value_) * (3.0 / dx);
                u[0] = -0.5;
            } break;
            case 2:
                fpp_[0] = lhs.value_;
                u[0] = 0.0;
                break;
            case 3:
                fpp_[0] = -(x_[1] - x_[0]) * lhs.value_;
                u[0] = 1.0;
                break;
            }
            for (int i = 1; i < n - 1; ++i) // decomposition
            {
                const double dx = x_[i] - x_[i - 1];
                const double d2 = x_[i + 1] - x_[i - 1];
                const double sig = dx / d2;
                const double p = sig * u[i - 1] + 2.0;
                u[i] = (sig - 1.0) / p;
                const double temp = (f_[i + 1] - f_[i]) / (x_[i + 1] - x_[i]) - (f_[i] - f_[i - 1]) / dx;
                fpp_[i] = (6.0 * temp - dx * fpp_[i - 1]) / (p * d2);
            }
            switch (rhs.order_) // set right boundary
            {
            default:
                THROW("Invalid boundary order");
            case 1: {
                const double dx = x_[n - 1] - x_[n - 2];
                const double un = (3.0 / dx) * (rhs.value_ - (f_[n - 1] - f_[n - 2]) / dx);
                fpp_[n - 1] = (2.0 * un - fpp_[n - 2]) / (2.0 + u[n - 2]);
            } break;
            case 2:
                fpp_[n - 1] = rhs.value_;
                break;
            case 3:
                fpp_[n - 1] = ((x_[n - 1] - x_[n - 2]) * rhs.value_ + fpp_[n - 2]) / (1.0 - u[n - 2]);
                break;
            }
            for (int k = n - 2; k >= 0; --k) // backsubstitution
                fpp_[k] += u[k] * fpp_[k + 1];
        }
    } // namespace

    Interp1_* Interp::NewCubic(
        const String_& name, const Vector_<>& x, const Vector_<>& f, const Boundary_& lhs, const Boundary_& rhs) {
        return new Cubic1_(name, x, f, lhs, rhs);
    }

#include <dal/auto/MG_Cubic1_Read.inc>
#include <dal/auto/MG_Cubic1_Write.inc>

    void Cubic1_::Write(Archive::Store_& dst) const { Cubic1::XWrite(dst, name_, x_, f_, fpp_); }
} // namespace Dal