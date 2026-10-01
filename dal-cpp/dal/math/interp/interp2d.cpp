//
// Created by wegam on 2022/5/2.
//

#include <dal/platform/strict.hpp>
#include <dal/math/interp/interp2d.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    Interp2_::Interp2_(const String_& name) : Storable_("Interp2", name) {}

    namespace Interp {
        void ValidateLinear2Axes(const Vector_<>& x, const Vector_<>& y) {
            REQUIRE(!x.empty() && !y.empty(), "Bilinear interpolation requires nonempty x and y axes");
            REQUIRE(IsMonotonic(x, std::less_equal<>()), "Bilinear x axis must be non-decreasing");
            REQUIRE(IsMonotonic(y, std::less_equal<>()), "Bilinear y axis must be non-decreasing");
        }

        std::unique_ptr<Interp2_> NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f) {
            return std::make_unique<Interp2Linear_>(name, x, y, f);
        }
    } // namespace Interp
} // namespace Dal

namespace {
    using namespace Dal;

#include <dal/auto/MG_Interp2Linear_v1_Read.inc>
#include <dal/auto/MG_Interp2Linear_v1_Write.inc>
} // namespace

namespace Dal {
    Interp2Linear_::Interp2Linear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f)
        : Interp2_(name), x_(x), y_(y), f_(f) {
        Interp::ValidateLinear2Axes(x_, y_);
        REQUIRE((x_.size() == f_.Rows()) && (y_.size() == f_.Cols()), "x_, y_ size must be equal to f_ size");
    }

    double Interp2Linear_::operator()(double x, double y) const {
        return Interp2DLinearImplX(x_, y_, f_, x, y);
    }

    void Interp2Linear_::Write(Archive::Store_& dst) const {
        Interp2Linear_v1::XWrite(dst, name_, x_, y_, f_);
    }
} // namespace Dal
