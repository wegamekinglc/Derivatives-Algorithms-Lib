//
// Created by wegam on 2024/9/1.
//

#include <dal/math/aad/expr.hpp>


namespace Dal::AAD {
    thread_local std::mutex Number_::mutex_{};
    thread_local Tape_* Number_::tape_ = nullptr;
}