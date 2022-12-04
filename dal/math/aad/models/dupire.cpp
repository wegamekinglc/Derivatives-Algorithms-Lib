//
// Created by wegam on 2022/12/4.
//


#include <dal/math/aad/models/dupire.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::AAD {
#include <dal/auto/MG_DupireModelData_v1_Read.inc>
#include <dal/auto/MG_DupireModelData_v1_Write.inc>

    void DupireModelData_::Write(Archive::Store_& dst) const {
        DupireModelData_v1::XWrite(dst, name_, spot_, spots_, times_, vols_);
    }
}
