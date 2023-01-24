//
// Created by wegam on 2022/1/20.
//


#include <dal/indice/index.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {

    double Index::PastFixing(_ENV, const String_& index_name, const DateTime_& fixing_time, bool quiet) {
        static const std::map<DateTime_, double> EMPTY;
        auto hist = Environment::Find<FixingsAccess_>(_env);
        REQUIRE(hist || quiet, "no fixing access");
        auto fixings = hist->Fetch(index_name);
        REQUIRE(fixings || quiet, "no fixings exist");

        auto vals = fixings ? fixings->vals_ : EMPTY;
        auto pf = vals.find(fixing_time);
        if (pf == vals.end()) {
            REQUIRE(quiet, "No fixing for this time");
            return -INF;
        }
        return pf->second;
    }

    double Index_::Fixing(_ENV, const DateTime_& fixing_time) const {
        return Index::PastFixing(_env, Name(), fixing_time);
    }
} // namespace Dal
