//
// Created by wegam on 2020/11/28.
//

#include <dal/platform/strict.hpp>
#include <dal/time/calendars/china.hpp>
#include <dal/time/calendars/init.hpp>
#include <dal/time/holidaydata.hpp>
#include <mutex>

static std::mutex TheCalendarsMutex;
#define LOCK_CAL std::lock_guard<std::mutex> l(TheCalendarsMutex)

namespace Dal {

    bool Calendars_::init_ = false;

    void Calendars_::Init() {
        LOCK_CAL;
        if (!init_) {
            Holidays::AddCenter("china.sse", China::SSE::holidays);
            Holidays::AddCenter("china.ib", China::SSE::holidays, China::IB::workWeekends);
            init_ = true;
        }
    }
}