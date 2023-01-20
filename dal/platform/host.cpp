//
// Created by Cheng Li on 17-12-19.
//

#include <ctime>
#include <dal/platform/host.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    namespace Host {
        void LocalTime(int* year, int* month, int* day, int* hour, int* minute, int* second) {
            time_t t = time(nullptr);
            struct tm now = {0, 0, 0, 0, 0, 0, 0, 0, 0};
#ifdef _WIN32
            localtime_s(&now, &t);
#else
            localtime_r(&t, &now);
#endif
            ASSIGN(year, now.tm_year + 1900);
            ASSIGN(month, now.tm_mon + 1);
            ASSIGN(day, now.tm_mday);
            ASSIGN(hour, now.tm_hour);
            ASSIGN(minute, now.tm_min);
            ASSIGN(second, now.tm_sec);
        }
    } // namespace Host
} // namespace Dal