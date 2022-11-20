//
// Created by wegam on 2022/11/20.
//

#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/platform/strict.hpp>


namespace Dal::AAD {
#include <dal/auto/MG_BSModelData_v1_Read.inc>
#include <dal/auto/MG_BSModelData_v1_Write.inc>

    void BSModelData_::Write(Archive::Store_& dst) const {
        BSModelData_v1::XWrite(dst, name_, spot_, vol_, spotMeasure_, rate_, div_);
    }
}