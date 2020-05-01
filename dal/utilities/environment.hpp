//
// Created by wegam on 2019/11/4.
//

#pragma once

#include <dal/utilities/noncopyable.hpp>

namespace Dal {
    namespace Environment {
        struct Entry_: noncopyable {
            virtual ~Entry_() = default;
        };
    }
}