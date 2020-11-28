//
// Created by wegam on 2020/11/28.
//

#include <dal/time/calendars/init.hpp>
#include <mutex>
#include <dal/time/holidaydata.hpp>
#include <dal/time/calendars/china.cpp>

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