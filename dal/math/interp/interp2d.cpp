//
// Created by wegam on 2022/5/2.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/interp/interp2d.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/interp/interp.hpp>

namespace Dal::Interp {
    Interp2_* NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f) {
        return new Interp2Linear_(name, x, y, f);
    }
}

namespace {
    using namespace Dal;

#include <dal/auto/MG_Interp2Linear_v1_Read.inc>
#include <dal/auto/MG_Interp2Linear_v1_Write.inc>
} // namespace

namespace Dal {
    template <>
    void Interp2XLinear_<double>::Write(Archive::Store_& dst) const {
        Interp2Linear_v1::XWrite(dst, name_, x_, y_, f_);
    }
} // namespace Dal