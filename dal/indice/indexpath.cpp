//
// Created by wegam on 2023/6/4.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/indice/indexpath.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {

    double IndexPath_::Extremum(bool maximum,
                                const Dal::DateTime_ &from,
                                const Dal::DateTime_ &to,
                                double monitoring_interval,
                                const pair<double, double> &collar) const {
        // TODO: fix this implementation
        auto prob = AllInRangeProb(from, to, collar, monitoring_interval, 0.0);
        if (prob > 0.0 && maximum)
            return collar.second;
        else if (prob > 0.0)
            return collar.first;
        else
            return 0.0;
    }

    double IndexPathHistorical_::Expectation(const DateTime_& t, const pair<double, double>& lh) const {
        auto pFix = fixings_.find(t);
        REQUIRE(pFix != fixings_.end(), "No fixing exists for " + DateTime::ToString(t));
        return std::max(lh.first, std::min(lh.second, pFix->second));
    }
}
