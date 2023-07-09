//
// Created by wegam on 2020/11/6.
//

#include <dal/platform/strict.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    Interp1_::Interp1_(const String_& name) : Storable_("Interp1", name) {}

    namespace Interp {
        Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f) {
            return new Interp1Linear_(name, x, f);
        }
    }
}

namespace {
    using namespace Dal;

#include <dal/auto/MG_Interp1Linear_v1_Read.inc>
#include <dal/auto/MG_Interp1Linear_v1_Write.inc>
} // namespace

namespace Dal {
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
        return InterpLinearImplX(x_, f_, x);
    }

    void Interp1Linear_::Write(Archive::Store_& dst) const {
        Interp1Linear_v1::XWrite(dst, name_, x_, f_);
    }
} // namespace Dal
