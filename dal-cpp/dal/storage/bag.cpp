//
// Created by wegam on 2023/1/21.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/bag.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/maps.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    namespace {
        #include <dal/auto/MG_Bag_Write.inc>
        #include <dal/auto/MG_Bag_Read.inc>

        Storable_* Bag::Reader_::Build() const {
            return new Bag_(name_, ZipToMultimap(keys_, contents_));
        }
    }

    void Bag_::Write(Archive::Store_& dst) const {
        Bag::XWrite(dst, name_, MapValues(contents_), Keys(contents_));
    }
}