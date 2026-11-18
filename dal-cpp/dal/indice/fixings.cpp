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
            REQUIRE(fixing_times_.size() == fixings_.size(),
                    "ARCHIVE_FIXINGS_LENGTH_MISMATCH: fixing_times=" + String_(std::to_string(fixing_times_.size())) +
                        "; fixings=" + String_(std::to_string(fixings_.size())));
            Fixings_::vals_t values;
            for (size_t i = 0; i < fixing_times_.size(); ++i)
                REQUIRE(values.emplace(fixing_times_[i], fixings_[i]).second,
                        "ARCHIVE_FIXINGS_DUPLICATE_TIME: field=/fixing_times/" + String_(std::to_string(i)));
            return new Fixings_(name_, values);
        }
    } // namespace
    const IndexFixHistory_& FixHistory::Empty() {
        static const IndexFixHistory_ RET_VAL((IndexFixHistory_::vals_t()));
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

    double IndexFixHistory_::Find(const DateTime_& fix_time, bool quiet) const { return LookupFixing(vals_, fix_time, quiet); }

    void Fixings_::Write(Archive::Store_& dst) const {
        Fixings::XWrite(dst, name_, MapValues(vals_), Keys(vals_));
    }
} // namespace Dal
