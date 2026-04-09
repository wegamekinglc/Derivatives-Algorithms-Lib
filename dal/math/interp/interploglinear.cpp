//
// Created by wegam on 2026/4/10.
//

#include <dal/platform/strict.hpp>
#include <dal/math/interp/interploglinear.hpp>

/*IF--------------------------------------------------------------------------
storable LogLinear1
        Log-linear interpolator
&members
name is ?string
        Name of the object
x is number[]
        X-points (independent variables) of the interpolation
f is number[]
        Y-points (function values) corresponding to X
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        struct LogLinear1_ : Interp1_ {
            Vector_<> x_, f_, logf_;

            LogLinear1_(const String_& name, const Vector_<>& x, const Vector_<>& f);
            double operator()(double x) const override;
            void Write(Archive::Store_& dst) const override;
        };

        LogLinear1_::LogLinear1_(const String_& name, const Vector_<>& x, const Vector_<>& f)
            : Interp1_(name), x_(x), f_(f), logf_(f.size()) {
            REQUIRE(x_.size() == f_.size(), "x and f size should be same");
            REQUIRE(x_.size() >= 2, "need at least 2 points");
            REQUIRE(IsMonotonic(x_), "x should be monotonic");
            for (int i = 0; i < f_.size(); ++i) {
                REQUIRE(f_[i] > 0.0, "f values must be positive for log-linear interpolation");
                logf_[i] = log(f_[i]);
            }
        }

        double LogLinear1_::operator()(double x) const { return exp(InterpLinearImplX(x_, logf_, x)); }
    } // namespace

    Interp1_* Interp::NewLogLinear(const String_& name, const Vector_<>& x, const Vector_<>& f) {
        return new LogLinear1_(name, x, f);
    }

#include <dal/auto/MG_LogLinear1_Read.inc>
#include <dal/auto/MG_LogLinear1_Write.inc>

    void LogLinear1_::Write(Archive::Store_& dst) const { LogLinear1::XWrite(dst, name_, x_, f_); }
} // namespace Dal
