//
// Created by wegam on 2020/11/27.
//

#pragma once

#include <dal/time/holidaydata.hpp>

namespace Dal {
    class String_;
    class Date_;

    class Holidays_ {
        Vector_<Handle_<HolidayCenterData_>> parts_;
        friend class CountBusDays_;

    public:
        explicit Holidays_(const String_& src);
        String_ String() const;
        bool IsHoliday(const Date_& date) const;
    };

    namespace Holidays {
        const Holidays_& None();
        Date_ NextBus(const Holidays_& hols, const Date_& from);
        Date_ PrevBus(const Holidays_& hols, const Date_& from);
    }

    class CountBusDays_ {
        Holidays_ hols_;

    public:
        explicit CountBusDays_(const Holidays_& holidays);
        int operator()(const Date_& begin, const Date_& end) const;
    };
}