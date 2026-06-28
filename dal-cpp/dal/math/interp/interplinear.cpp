//
// Created by wegam on 2020/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>


/*IF--------------------------------------------------------------------------
storable Interp1Linear
        Linear interpolator on known values in one dimension
version 1
&members
name is ?string
x is number[]
f is number[]
-IF-------------------------------------------------------------------------*/


namespace Dal {

    namespace {
        class Interp1Linear_ : public Interp1_ {
            Vector_<> x_;
            Vector_<> f_;
            mutable size_t lastIndex_ = 0;

        public:
            Interp1Linear_(const String_& name, const Vector_<>& x, const Vector_<>& f);
            Interp1Linear_(const String_& name, const std::map<double, double>& f);
            void Write(Archive::Store_& dst) const override;
            double operator()(double x) const override;
            [[nodiscard]] const Vector_<>& x() const { return x_; }
            [[nodiscard]] const Vector_<>& f() const { return f_; }
        };

        Interp1Linear_::Interp1Linear_(const String_& name, const Vector_<>& x, const Vector_<>& f)
            : Interp1_(name), x_(x), f_(f) {
            REQUIRE(x_.size() == f_.size(), "x_ size must be equal to f_ size");
            REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
        }

        Interp1Linear_::Interp1Linear_(const String_& name, const std::map<double, double>& f)
            : Interp1_(name), x_(Keys(f)), f_(MapValues(f)) {
            REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
        }

        double Interp1Linear_::operator()(double x) const {
            return InterpLinearImplX(x_, f_, x, &lastIndex_);
        }

    } // namespace

#include <dal/auto/MG_Interp1Linear_v1_Read.inc>
#include <dal/auto/MG_Interp1Linear_v1_Write.inc>

    namespace Interp {

        Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f) {
            return new Interp1Linear_(name, x, f);
        }
    }
    
    void Interp1Linear_::Write(Archive::Store_& dst) const {
        Interp1Linear_v1::XWrite(dst, name_, x_, f_);
    }

} // namespace Dal
