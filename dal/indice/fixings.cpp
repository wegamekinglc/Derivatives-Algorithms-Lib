//
// Created by wegam on 2022/1/20.
//

#include <dal/platform/platform.hpp>
#include <dal/indice/fixings.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    const FixHistory_& FixHistory::Empty() {
        static const FixHistory_ RET_VAL((FixHistory_::vals_t()));
        return RET_VAL;
    }

    double FixHistory_::Find(const DateTime_& fix_time, bool quiet) const {
        auto less_ = [](const pair<DateTime_, double>& lhs, const DateTime_& rhs) {
            return lhs.first < rhs;
        };

        auto pGE = std::lower_bound(vals_.begin(), vals_.end(), fix_time, less_);
        if (pGE == vals_.end() || pGE->first != fix_time) {
            REQUIRE(quiet, "no fixings for that time");
            return -INF;
        }
        return pGE->second;
    }
}