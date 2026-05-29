//
// Created by wegam on 2020/11/28.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/time/calendars/init.hpp>
#include <dal/time/calendars/china.hpp>
#include <dal/time/holidaydata.hpp>

namespace Dal {

    bool Calendars_::init_ = false;
    std::mutex Calendars_::mutex_;

    void Calendars_::Init() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            Holidays::AddCenter("CN.SSE", China::SSE::HOLIDAYS);
            std::set<Date_> tmp(China::SSE::HOLIDAYS.begin(), China::SSE::HOLIDAYS.end());
            tmp.erase(Date_(2024, 2, 9));  // remove a special date for the year 2024
            Vector_<Date_> ib_holidays(tmp.size());
            Copy(tmp, &ib_holidays);
            Holidays::AddCenter("CN.IB", ib_holidays, China::IB::WORK_WEEKENDS);
            init_ = true;
        }
    }
} // namespace Dal