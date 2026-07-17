//
// Created by wegam on 2020/11/27.
//


#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/holidaydata.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>
#include <shared_mutex>

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
        HolidayData_& TheHolidayData() { RETURN_STATIC(HolidayData_); }
        std::shared_mutex& TheHolidayMutex() { RETURN_STATIC(std::shared_mutex); }

        HolidayData_ CopyHolidayData() {
            std::shared_lock<std::shared_mutex> lock(TheHolidayMutex());
            return TheHolidayData();
        }

        bool ContainsNoWeekends(const Vector_<Date_>& dates) {
            for (const auto& d : dates)
                if (Date::DayOfWeek(d) % 6 == 0)
                    return false;
            return true;
        }

        bool ContainsOnlyWeekends(const Vector_<Date_>& dates) {
            for (const auto& d : dates)
                if (Date::DayOfWeek(d) % 6 != 0)
                    return false;
            return true;
        }
    } // namespace

    void Holidays::AddCenter(const String_& city, const Vector_<Date_>& holidays, const Vector_<Date_>& workWeekends) {
        REQUIRE(TheHolidayData().IsValid(), "Holiday data is not valid");
        REQUIRE(ContainsNoWeekends(holidays), "Holidays should not contain weekends");
        REQUIRE(ContainsOnlyWeekends(workWeekends), "Can only weekends in special working days");
        REQUIRE(IsMonotonic(holidays), "Holidays should be in ascending order");
        REQUIRE(IsMonotonic(workWeekends), "Working weekends should be in ascending order");
        NOTICE(city);

        HolidayData_ temp(CopyHolidayData());
        REQUIRE(!temp.centerIndex_.count(city), "Duplicate holiday center");
        temp.centerIndex_[city] = static_cast<int>(temp.holidays_.size());
        temp.holidays_.push_back(Handle_(std::make_shared<const HolidayCenterData_>(city, holidays, workWeekends)));

        std::unique_lock<std::shared_mutex> lock(TheHolidayMutex());
        TheHolidayData().Swap(&temp);
        REQUIRE(TheHolidayData().IsValid(), "Holiday data is not valid");
    }

    int Holidays::CenterIndex(const String_& center) {
        NOTICE(center);
        std::shared_lock<std::shared_mutex> lock(TheHolidayMutex());
        auto p = TheHolidayData().centerIndex_.find(center);
        REQUIRE(p != TheHolidayData().centerIndex_.end(), "Invalid holiday center");
        return p->second;
    }

    Handle_<HolidayCenterData_> Holidays::OfCenter(int centerIndex) {
        std::shared_lock<std::shared_mutex> lock(TheHolidayMutex());
        REQUIRE(centerIndex >= 0 && centerIndex < TheHolidayData().holidays_.size(), "Invalid holiday center index");
        return TheHolidayData().holidays_[centerIndex];
    }

    Handle_<HolidayCenterData_> Holidays::OfCenter(const String_& center) { return OfCenter(CenterIndex(center)); }
} // namespace Dal
