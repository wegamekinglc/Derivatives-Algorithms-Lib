//
// Created by wegam on 2022/11/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/model/blackscholes.hpp>

namespace Dal {
#include <dal/auto/MG_BSModelData_v1_Read.inc>
#include <dal/auto/MG_BSModelData_v1_Write.inc>

    void BSModelData_::Write(Archive::Store_& dst) const {
        BSModelData_v1::XWrite(dst, name_, spot_, vol_, rate_, div_);
    }

    std::unique_ptr<ModelData_> BSModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for BSModelData");
        return std::make_unique<BSModelData_>(*newName, spot_, vol_, rate_, div_);
    }
} // namespace Dal
