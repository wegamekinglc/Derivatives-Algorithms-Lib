//
// Created by wegam on 2020/11/27.
//

#include <dal/time/holidaydata.hpp>
#include <mutex>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

static std::mutex TheHolidayDataMutex;
#define LOCK_DATA std::lock_guard<std::mutex> l(TheHolidayDataMutex)

namespace Dal {
    bool HolidayData_::IsValid() const {
        const int nc = static_cast<int>(holidays_.size());
        if (centerIndex_.size() != nc)
            return false;
        for (const auto& c_i : centerIndex_) {
            if (c_i.second >= nc || c_i.second < 0)
                return false;
            if (holidays_[c_i.second]->center_ != c_i.first)
                return false;
        }
        return true;
    }

    void HolidayData_::Swap(HolidayData_* other) {
        centerIndex_.swap(other->centerIndex_);
        holidays_.Swap(&other->holidays_);
    }

    namespace {
        HolidayData_& TheHolidayData() {
            RETURN_STATIC(HolidayData_);
        }

        HolidayData_ CopyHolidayData() {
            LOCK_DATA;
            return TheHolidayData();
        }

        bool ContainsNoWeekends(const Vector_<Date_>& dates) {
            for (const auto& d : dates)
                if (Date::DayOfWeek(d) % 6 == 0)
                    return false;
            return true;
        }
    }

    void Holidays::AddCenter(const String_& city, const Vector_<Date_>& holidays) {
        REQUIRE(TheHolidayData().IsValid(), "Holiday data is not valid");
        REQUIRE(ContainsNoWeekends(holidays), "Holidays should not contain weekends");
        REQUIRE(IsMonotonic(holidays), "Holidays should be in ascending order");
        NOTICE(city);

        HolidayData_ temp(CopyHolidayData());
        REQUIRE(!temp.centerIndex_.count(city), "Duplicate holiday center");
        temp.centerIndex_[city] = static_cast<int>(temp.holidays_.size());
        temp.holidays_.push_back(Handle_(std::make_shared<const HolidayCenterData_>(city, holidays)));

        LOCK_DATA;
        TheHolidayData().Swap(&temp);
        REQUIRE(TheHolidayData().IsValid(), "Holiday data is not valid");
    }

    int Holidays::CenterIndex(const String_& center) {
        NOTICE(center);
        LOCK_DATA;
        auto p = TheHolidayData().centerIndex_.find(center);
        REQUIRE(p != TheHolidayData().centerIndex_.end(), "Invalid holiday center");
        return p->second;
    }

    Handle_<HolidayCenterData_> Holidays::OfCenter(int center_index) {
        LOCK_DATA;
        REQUIRE(center_index >=0 && center_index < TheHolidayData().holidays_.size(),
                "Invalid holiday center index");
        return TheHolidayData().holidays_[center_index];
    }
}