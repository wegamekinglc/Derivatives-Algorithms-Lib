//
// Created by wegam on 2020/11/6.
//

#include <dal/platform/platform.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/platform/strict.hpp>

#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal::Interp {
    Interp1_* NewLinear(const String_& name, const Vector_<>& x, const Vector_<>& f) {
        return new Interp1Linear_ (name, x, f);
    }
}

namespace {
    using namespace Dal;

#include <dal/auto/MG_Interp1Linear_v1_Read.inc>
#include <dal/auto/MG_Interp1Linear_v1_Write.inc>
} // namespace

namespace Dal {
    template <>
    void Interp1XLinear_<double>::Write(Archive::Store_& dst) const {
        Interp1Linear_v1::XWrite(dst, name_, x_, f_);
    }
} // namespace Dal
