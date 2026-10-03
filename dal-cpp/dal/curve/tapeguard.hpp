//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <dal/math/aad/recording.hpp>

namespace Dal {

    // Compatibility entry for independent curve recordings with reusable tape capacity.
    struct TapeGuard_ {
        Dal::AAD::Tape_* t_;
        Dal::AAD::RecordingScope_ recording_;
        explicit TapeGuard_(Dal::AAD::Tape_* t) : t_(t), recording_(t) {}
        TapeGuard_(const TapeGuard_&) = delete;
        TapeGuard_& operator=(const TapeGuard_&) = delete;
        void Close() { recording_.Close(); }
    };

} // namespace Dal
