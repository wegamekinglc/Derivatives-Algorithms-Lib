//
// Created by wegam on 2022/11/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/aad/models/blackscholes.hpp>

namespace Dal::AAD {
#include <dal/auto/MG_BSModelData_v1_Read.inc>
#include <dal/auto/MG_BSModelData_v1_Write.inc>

    void BSModelData_::Write(Archive::Store_& dst) const {
        BSModelData_v1::XWrite(dst, name_, spot_, vol_, rate_, div_);
    }

    BSModelData_* BSModelData_::MutantModel(const String_* new_name, const Slide_* slide) const {
        std::unique_ptr<BSModelData_> temp(new BSModelData_(*new_name, spot_, vol_, rate_, div_));
        if (slide) {
            // TODO: finish the implementation
        }
        return temp.release();
    }
}