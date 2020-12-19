//
// Created by Cheng Li on 17-12-19.
//

#pragma once

namespace Dal {
    namespace Host {
        void
        localTime(int* year, int* month, int* day, int* hour = nullptr, int* minute = nullptr, int* second = nullptr);
    }
} // namespace Dal

#define FORCE_INLINE __forceinline