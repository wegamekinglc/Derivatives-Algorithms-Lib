//
// Created by wegam on 2020/11/6.
//

#include <dal/math/interp/interp.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/utilities/algorithms.hpp>
#include <dal/storage/archive.hpp>

namespace Dal {
    Interp1_::Interp1_(const String_& name) :Storable_("Interp1", name) {}

    Interp1Linear_::Interp1Linear_(const String_& name, const Vector_<>& x, const Vector_<>& f)
    :Interp1_(name), x_(x), f_(f) {
        REQUIRE(x_.size() == f_.size(), "x_ size must be equal to f_ size");
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
    }

    Interp1Linear_::Interp1Linear_(const String_& name, const std::map<double, double>& f)
    : Interp1_(name), x_(Keys(f)), f_(MapValues(f)) {
        REQUIRE(IsMonotonic(x_, std::less_equal<>()), "x_ array should be monotonic");
    }

    double Interp1Linear_::operator()(double x) const {
        auto pge = LowerBound(x_, x);
        if(pge == x_.end())
            return f_.back();
        else if(pge == x_.begin() || IsZero(x - *pge))
            return f_[pge - x_.begin()];
        else {
            auto plt = Previous(pge);
            const double gFrac = (x - *plt) / (*pge - *plt);
            auto flt = f_.begin() + (plt - x_.begin());
            return *flt + gFrac * (*Next(flt) - *flt);
        }
    }

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
}

namespace Dal {
    void Interp1Linear_::Write(Archive::Store_& dst) const {
        Interp1Linear_v1::XWrite(dst, name_, x_, f_);
    }
}



