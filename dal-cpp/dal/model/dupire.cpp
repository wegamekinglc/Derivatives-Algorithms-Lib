//
// Created by wegam on 2022/12/4.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/model/dupire.hpp>

namespace Dal {
#include <dal/auto/MG_DupireModelData_v1_Read.inc>
#include <dal/auto/MG_DupireModelData_v1_Write.inc>

    void DupireModelData_::Write(Archive::Store_& dst) const {
        DupireModelData_v1::XWrite(dst, name_, spot_, rate_, repo_, spots_, times_, vols_);
    }

    DupireModelData_* DupireModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for DupireModelData");
        return new DupireModelData_(*newName, spot_, rate_, repo_, spots_, times_, vols_);
    }
} // namespace Dal
