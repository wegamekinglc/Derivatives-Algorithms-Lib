//
// Created by wegam on 2020/11/28.
//

#include <dal/platform/strict.hpp>
#include <dal/time/calendars/china.hpp>
#include <dal/time/calendars/init.hpp>
#include <dal/time/holidaydata.hpp>

namespace Dal {

    bool Calendars_::init_ = false;
    std::mutex Calendars_::mutex_;

    void Calendars_::Init() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            Holidays::AddCenter("CN.SSE", China::SSE::holidays);
            Holidays::AddCenter("CN.IB", China::SSE::holidays, China::IB::workWeekends);
            init_ = true;
        }
    }
} // namespace Dal