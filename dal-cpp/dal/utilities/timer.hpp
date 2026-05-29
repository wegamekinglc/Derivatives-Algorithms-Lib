//
// Created by wegam on 2022/3/10.
//

#pragma once

#include <chrono>

namespace Dal {
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    using std::chrono::nanoseconds;
    using std::chrono::duration_cast;

    class Timer_ {
        std::chrono::time_point<std::chrono::high_resolution_clock> begin_;

    public:
        Timer_(): begin_(high_resolution_clock::now()) {}
        void Reset() {
            begin_ = high_resolution_clock::now();
        }

        template <class D_ = microseconds>
        [[nodiscard]] int64_t Elapsed() const {
            return duration_cast<D_>(high_resolution_clock::now() - begin_).count();
        }
    };
}
