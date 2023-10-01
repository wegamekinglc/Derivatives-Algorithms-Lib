//
// Created by wegam on 2022/1/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/indice/fixings.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/maps.hpp>
#include <dal/storage/archive.hpp>


namespace Dal {
    namespace {
#include <dal/auto/MG_Fixings_Write.inc>
#include <dal/auto/MG_Fixings_Read.inc>

        Storable_* Fixings::Reader_::Build() const {
            return new Fixings_(name_, ZipToMap(fixing_times_, fixings_));
        }
    }
    const FixHistory_& FixHistory::Empty() {
        static const FixHistory_ RET_VAL((FixHistory_::vals_t()));
        return RET_VAL;
    }

    double FixHistory_::Find(const DateTime_& fix_time, bool quiet) const {
        auto less_ = [](const pair<DateTime_, double>& lhs, const DateTime_& rhs) { return lhs.first < rhs; };

        auto pGE = std::lower_bound(vals_.begin(), vals_.end(), fix_time, less_);
        if (pGE == vals_.end() || pGE->first != fix_time) {
            REQUIRE(quiet, "no fixings for that time");
            return -INF;
        }
        return pGE->second;
    }

    void Fixings_::Write(Archive::Store_& dst) const {
        Fixings::XWrite(dst, name_, MapValues(vals_), Keys(vals_));
    }
} // namespace Dal