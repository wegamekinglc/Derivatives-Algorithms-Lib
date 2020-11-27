//
// Created by wegam on 2020/11/27.
//

#pragma once

#include <map>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>


namespace Dal {
    struct HolidayCenterData_ {
        String_ center_;
        Vector_<Date_> holidays_;
        HolidayCenterData_(const String_& c, const Vector_<Date_>& h)
        :center_(c), holidays_(h) {}
    };

    struct HolidayData_ {
        Vector_<Handle_<HolidayCenterData_>> holidays_;
        std::map<String_, int> centerIndex_;

        bool IsValid() const;
        void Swap(HolidayData_* other);
    };

    namespace Holidays {
        void AddCenter(const String_& city, const Vector_<Date_>& holidays);
        int CenterIndex(const String_& center);
        Handle_<HolidayCenterData_> OfCenter(int center_index);
    }
}
