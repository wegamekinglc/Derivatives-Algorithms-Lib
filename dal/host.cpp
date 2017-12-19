//
// Created by wegamekinglc on 17-12-19.
//

#include <dal/host.hpp>
#include <ctime>
#include <dal/algorithms.hpp>

namespace host {
    void  localTime(int *year, int *month, int *day, int *hour, int *minute, int *second) {
        time_t t = time(nullptr);
        struct  tm * now = localtime(&t);
        ASSIGN(year, now->tm_year + 1900);
        ASSIGN(month, now->tm_mon + 1);
        ASSIGN(day, now->tm_mday);
        ASSIGN(hour, now->tm_hour);
        ASSIGN(minute, now->tm_min);
        ASSIGN(second, now->tm_sec);
    }
}