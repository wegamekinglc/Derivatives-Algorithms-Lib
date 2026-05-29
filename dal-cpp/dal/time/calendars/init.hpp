//
// Created by wegam on 2020/11/28.
//

#pragma once

#include <mutex>

namespace Dal {
    class Calendars_ {
    public:
        static void Init();

    private:
        static bool init_;
        static std::mutex mutex_;
    };
} // namespace Dal
