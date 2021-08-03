//
// Created by wegam on 2020/11/27.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/holidays.hpp>
#include <mutex>
#include <dal/time/holidaydata.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    namespace {
        std::mutex TheHolidayComboMutex;
        #define LOCK_COMBOS std::lock_guard<std::mutex> l(TheHolidayComboMutex);

        bool operator<(const Handle_<HolidayCenterData_>& lhs, const Handle_<HolidayCenterData_>& rhs) {
            return lhs->center_ < rhs->center_;
        }

        String_ NameFromCenter(const Vector_<Handle_<HolidayCenterData_>>& parts) {
            static const auto ToName = [](const Handle_<HolidayCenterData_> h) {
                return h->center_;
            };
            return String::Accumulate(Apply(ToName, parts), " ");
        }

        std::map<String_, Handle_<HolidayCenterData_>>& TheCombinations() {
            RETURN_STATIC(std::map<String_, Handle_<HolidayCenterData_>>);
        }
    }

    Holidays_::Holidays_(const String_& src) {
        Vector_<String_> centers = String::Split(src, ' ', false);
        parts_ = Apply([](const String_& c){return Holidays::OfCenter(c);}, Unique(centers));
        if (parts_.size() > 1) {
            LOCK_COMBOS;
            auto existing = TheCombinations().find(NameFromCenter(parts_));
            if (existing != TheCombinations().end())
                parts_ = Vector::V1(existing->second);
        }
    }

    bool Holidays_::IsHoliday(const Date_& date) const {
        for (const auto& ps : parts_)
            if (BinarySearch(ps->holidays_, date))
                return true;
        return false;
    }

    bool Holidays_::IsWorkWeekends(const Date_& date) const {
        for (const auto& ps : parts_)
            if (BinarySearch(ps->workWeekends_, date))
                return true;
        return false;
    }

    String_ Holidays_::String() const {
        return NameFromCenter(parts_);
    }

    bool operator==(const Holidays_& lhs, const Holidays_& rhs) {
        return lhs.String() == rhs.String();
    }

    bool operator!=(const Holidays_& lhs, const Holidays_& rhs) {
        return lhs.String() != rhs.String();
    }

    CountBusDays_::CountBusDays_(const Holidays_& src)
    :hols_(src) {
        LOCK_COMBOS;
        Handle_<HolidayCenterData_>& combo = TheCombinations()[src.String()];
        if (combo.IsEmpty()) {
            Vector_<Date_> mergedHolidays;
            Vector_<Date_> mergedWorkWeekends;
            for (const auto& p : src.parts_) {
                mergedHolidays.Append(p->holidays_);
                mergedWorkWeekends.Append(p->workWeekends_);
            }
            combo.reset(new HolidayCenterData_(src.String(), Unique(mergedHolidays), Unique(mergedWorkWeekends)));
        }
        hols_.parts_ = Vector::V1(combo);
    }

    int CountBusDays_::operator()(const Date_& begin, const Date_& end) const {
        if (end <= begin)
            return 0;
        const int weeks = (end - begin) / 7;
        const Date_& stop = begin.AddDays(7 * weeks);
        auto p1Stop = LowerBound(hols_.parts_[0]->holidays_, stop);
        auto p2Stop = LowerBound(hols_.parts_[0]->workWeekends_, stop);
        const Vector_<Date_>& hols = hols_.parts_[0]->holidays_;
        const Vector_<Date_>& wws = hols_.parts_[0]->workWeekends_;
        const auto holsToStop = static_cast<int>(p1Stop - LowerBound(hols, begin));
        const auto wwsToAdd = static_cast<int>(p2Stop - LowerBound(wws, begin));
        int ret_val = 5 * weeks - holsToStop + wwsToAdd;

        for (Date_ d = stop; d < end; ++d) {
            if (!Date::IsWeekEnd(d)) {
                if (p1Stop != hols_.parts_[0]->holidays_.end() && *p1Stop == d)
                    ++p1Stop;
                else
                    ++ret_val;
            }

            if (p2Stop != hols_.parts_[0]->workWeekends_.end() && *p2Stop == d) {
                ++ret_val;
                ++p2Stop;
            }
        }
        return ret_val;
    }

    Date_ Holidays::NextBus(const Holidays_& hols, const Date_& from) {
        for (Date_ ret_val = from;; ++ret_val) {
            if (hols.IsWorkWeekends(ret_val) || (!Date::IsWeekEnd(ret_val) && !hols.IsHoliday(ret_val)))
                return ret_val;
        }
    }

    Date_ Holidays::PrevBus(const Holidays_& hols, const Date_& from) {
        for (Date_ ret_val = from;; --ret_val) {
            if (hols.IsWorkWeekends(ret_val) || (!Date::IsWeekEnd(ret_val) && !hols.IsHoliday(ret_val)))
                return ret_val;
        }
    }

    const Holidays_& Holidays::None() {
        static const Holidays_ RET_VAL("");
        return RET_VAL;
    }
}