//
// Created by wegam on 2023/6/4.
//

#pragma once

#include <map>
#include <dal/utilities/noncopyable.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    class IndexPath_: noncopyable {
    public:
        virtual ~IndexPath_() = default;

        [[nodiscard]] virtual double Expectation(const DateTime_& fixing_time, const std::pair<double, double>& collar) const = 0;
        [[nodiscard]] virtual double FixInRangeProb(const DateTime_& fixing_time,
                                                    const std::pair<double, double>& range,
                                                    double ramp_width) const = 0;
        [[nodiscard]] virtual double AllInRangeProb(const DateTime_& from,
                                                    const DateTime_& to,
                                                    const std::pair<double, double>& range,
                                                    double monitoring_interval,
                                                    double ramp_sigma) const = 0;

        [[nodiscard]] virtual double Extremum(bool maximum,
                                              const DateTime_& from,
                                              const DateTime_& to,
                                              double monitoring_interval,
                                              const std::pair<double, double>& collar) const;

    };

    class IndexPathHistorical_ : public IndexPath_ {
    public:
        std::map<DateTime_, double> fixings_;
        [[nodiscard]] double Expectation(const DateTime_& t, const std::pair<double, double>& lh) const override;
        [[nodiscard]] double FixInRangeProb(const DateTime_& t, const std::pair<double, double>& range, double ramp_width) const override;
        [[nodiscard]] double AllInRangeProb(const DateTime_& from,
                                            const DateTime_& to,
                                            const std::pair<double, double>& range,
                                            double monitoring_interval,
                                            double ramp_sigma) const override;

        explicit IndexPathHistorical_(const std::map<DateTime_, double>& fixings = std::map<DateTime_, double>()): fixings_((fixings)) {}
    };
}
