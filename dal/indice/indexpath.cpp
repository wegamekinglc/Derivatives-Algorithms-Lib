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
                                const std::pair<double, double> &collar) const {
        // TODO: fix this implementation
        auto prob = AllInRangeProb(from, to, collar, monitoring_interval, 0.0);
        if (prob > 0.0 && maximum)
            return collar.second;
        if (prob > 0.0)
            return collar.first;
        return 0.0;
    }

    double IndexPathHistorical_::Expectation(const DateTime_& t, const std::pair<double, double>& lh) const {
        auto pfix = fixings_.find(t);
        REQUIRE(pfix != fixings_.end(), "No fixing exists for " + DateTime::ToString(t));
        return std::max(lh.first, std::min(lh.second, pfix->second));
    }

    double IndexPathHistorical_::FixInRangeProb(const DateTime_& fixing_time,
                                                const std::pair<double, double>& range,
                                                double ramp_width) const {
        auto pfix = fixings_.find(fixing_time);
        REQUIRE(pfix != fixings_.end(), "No fixing exists for " + DateTime::ToString(fixing_time));
        double fixed = pfix->second;
        double lower = range.first;
        double upper = range.second;

        if (fixed >= lower && fixed <= upper)
            return 1.0;
        if (fixed >= lower - ramp_width && fixed < lower)
            return 1.0 - (lower - fixed) / ramp_width;
        if (fixed <= upper + ramp_width && fixed > upper)
            return 1.0 - (fixed - upper) / ramp_width;
        return 0.0;
    }

    double IndexPathHistorical_::AllInRangeProb(const DateTime_& from,
                                                const DateTime_& to,
                                                const std::pair<double, double>& range,
                                                double monitoring_interval,
                                                double ramp_sigma) const {
        DateTime_ current = from;
        double prob = 1.0;

        while (current < to) {
            auto pfix = fixings_.find(current);
            if (pfix != fixings_.end())
                prob *= FixInRangeProb(current, range, ramp_sigma);
            current += monitoring_interval;
        }
        return prob;
    }

}
