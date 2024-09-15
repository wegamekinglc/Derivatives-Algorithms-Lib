//
// Created by wegam on 2023/7/21.
//

#pragma once
#include <mutex>

namespace Dal {
    class RegisterAll_ {
    public:
        static RegisterAll_& Init(const int n_threads = 0) {
            static RegisterAll_ reg{n_threads};
            return reg;
        };

    protected:
        explicit RegisterAll_(int n_threads);
        static bool init_;
        static std::mutex mutex_;
    };
}
