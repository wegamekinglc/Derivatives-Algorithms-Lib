//
// Created by wegam on 2023/7/21.
//

#pragma once
#include <mutex>

namespace Dal {
    class RegisterAll_ {
    public:
        static RegisterAll_& Init() {
            static RegisterAll_ reg;
            return reg;
        };

    protected:
        RegisterAll_();
        static bool init_;
        static std::mutex mutex_;
    };
}
