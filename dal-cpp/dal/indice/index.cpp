//
// Created by wegam on 2022/1/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/indice/index.hpp>

namespace Dal {

    double Index::PastFixing(_ENV, const String_& indexName, const DateTime_& fixingTime, bool quiet) {
        static const std::map<DateTime_, double> EMPTY;
        auto hist = Environment::Find<FixingsAccess_>(_env);
        REQUIRE(hist || quiet, "no fixing access");
        auto fixings = hist->Fetch(indexName);
        REQUIRE(fixings || quiet, "no fixings exist");

        const auto& vals = fixings ? fixings->vals_ : EMPTY;
        return LookupFixing(vals, fixingTime, quiet);
    }

    double Index_::Fixing(_ENV, const DateTime_& fixingTime) const {
        return Index::PastFixing(_env, Name(), fixingTime);
    }
} // namespace Dal
