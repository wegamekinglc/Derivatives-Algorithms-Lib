//
// Created by wegam on 2023/6/4.
//

#pragma once

#include <map>
#include <dal/platform/platform.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    class IndexPath_: noncopyable {
    public:
        virtual ~IndexPath_() = default;

        [[nodiscard]] virtual double Expectation(const DateTime_& fixing_time, const pair<double, double>& collar) const = 0;
        [[nodiscard]] virtual double FixInRangeProb(const DateTime_& fixing_time,
                                                    const pair<double, double>& range,
                                                    double ramp_width = 0.0) const = 0;
        [[nodiscard]]virtual double AllInRangeProb(const DateTime_& from,
                                                   const DateTime_& to,
                                                   const pair<double, double>& range,
                                                   double monitoring_interval,
                                                   double ramp_sigma = 0.0) const = 0;

        [[nodiscard]] virtual double Extremum(bool maximum,
                                              const DateTime_& from,
                                              const DateTime_& to,
                                              double monitoring_interval,
                                              const pair<double, double>& collar) const;

    };

    class IndexPathHistorical_ : public IndexPath_ {
    public:
        std::map<DateTime_, double> fixings_;
        [[nodiscard]] virtual double Expectation(const DateTime_& t, const pair<double, double>& lh) const;
    };
}
