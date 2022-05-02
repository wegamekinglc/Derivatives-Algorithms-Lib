//
// Created by wegam on 2022/5/2.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/interp/interp2d.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/interp/interp.hpp>

/*IF--------------------------------------------------------------------------
storable Interp2Linear
        Linear interpolator on known values in two dimensions
version 1
&members
name is ?string
x is number[]
y is number[]
f is number[][]
-IF-------------------------------------------------------------------------*/

namespace Dal {
    Interp2_::Interp2_(const String_& name) : Storable_("Interp2", name) {}

    Interp2Linear_::Interp2Linear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f)
    : Interp2_(name), x_(x), y_(y), f_(f) {
        REQUIRE((x_.size() == f_.Rows()) && (y_.size() == f_.Cols()), "x_, y_ size must be equal to f_ size");
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
        REQUIRE(IsMonotonic(y_, std::less_equal<>()), "y_ array should be monotonic");

        n_ = x_.size();
        m_ = y_.size();

        // using internal 1D interpolations
        for(size_t i = 0; i != n_; ++i) {
            auto row = f_.Row(i);
            auto rowVector = Vector_(row.begin(), row.end());
            interps_.push_back(
                Handle_<Interp1_>(Interp::NewLinear("interp1_" + String::FromInt(i), y_, rowVector))
            );
        }
    }

    double Interp2Linear_::operator()(double x, double y) const {
        auto it = UpperBound(x_, x);
        const size_t n2 = std::distance(x_.begin(), it);

        // extrapolation
        if (n2 == n_)
            return (*interps_[n2-1])(y);
        if (n2 == 0)
            return (*interps_[0])(y);

        // interpolation
        const size_t n1 = n2 - 1;
        auto x1 = x_[n1];
        auto x2 = x_[n2];
        auto z1 = (*interps_[n1])(y);
        auto z2 = (*interps_[n2])(y);

        auto t = (x - x1) / (x2 - x1);
        return z1 + (z2 - z1) * t;
    }

    namespace Interp {
        Interp2_* NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f) {
            return new Interp2Linear_(name, x, y, f);
        }
    }
}

namespace {
    using namespace Dal;

#include <dal/auto/MG_Interp2Linear_v1_Read.inc>
#include <dal/auto/MG_Interp2Linear_v1_Write.inc>
} // namespace

namespace Dal {
    void Interp2Linear_::Write(Archive::Store_& dst) const {
        Interp2Linear_v1::XWrite(dst, name_, x_, y_, f_);
    }
} // namespace Dal