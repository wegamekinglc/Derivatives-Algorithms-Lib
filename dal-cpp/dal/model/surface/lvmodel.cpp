//
// Created by Codex on 2026/9/30.
//

#include <dal/model/surface/lvmodel.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
#include <dal/auto/MG_LocalVolSurfaceData_v1_Read.inc>
#include <dal/auto/MG_LocalVolSurfaceData_v1_Write.inc>

    void LocalVolSurfaceData_::Write(Archive::Store_& dst) const { LocalVolSurfaceData_v1::XWrite(dst, name_, spots_, times_, vols_); }
} // namespace Dal
