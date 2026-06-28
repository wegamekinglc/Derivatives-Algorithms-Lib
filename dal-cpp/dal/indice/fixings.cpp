//
// Created by wegam on 2022/1/20.
//

#include <dal/platform/consts.hpp>
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
    } // namespace
    const FixHistory_& FixHistory::Empty() {
        static const FixHistory_ RET_VAL((FixHistory_::vals_t()));
        return RET_VAL;
    }

    double LookupFixing(const std::map<DateTime_, double>& vals, const DateTime_& fixTime, bool quiet) {
        auto pf = vals.find(fixTime);
        if (pf == vals.end()) {
            REQUIRE(quiet, "no fixings for that time");
            return -INF;
        }
        return pf->second;
    }

    double FixHistory_::Find(const DateTime_& fix_time, bool quiet) const { return LookupFixing(vals_, fix_time, quiet); }

    void Fixings_::Write(Archive::Store_& dst) const {
        Fixings::XWrite(dst, name_, MapValues(vals_), Keys(vals_));
    }
} // namespace Dal
