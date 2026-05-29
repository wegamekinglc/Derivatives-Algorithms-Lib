//
// Created by wegam on 2022/4/3.
//


#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/box.hpp>
#include <dal/storage/archive.hpp>

namespace Dal {
    namespace {
#include <dal/auto/MG_Box_Write.inc>
#include <dal/auto/MG_Box_Read.inc>
    }

    void Box_::Write(Archive::Store_& dst) const { Box::XWrite(dst, name_, contents_); }
}

