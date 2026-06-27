//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <dal/math/aad/aad.hpp>

namespace Dal {

    // RAII guard that rewinds the AAD tape on construction and destruction.
    // Rewind (cursor reset, block reuse) keeps blocks allocated across consecutive
    // calibration Jacobian sweeps instead of freeing and re-allocating them every
    // Newton step. The next NewRecording + RegisterIndependent calls overwrite the
    // reused node storage, so no stale data leaks into the next sweep.
    // Single-threaded.
    struct TapeGuard_ {
        Dal::AAD::Tape_* t_;
        explicit TapeGuard_(Dal::AAD::Tape_* t) : t_(t) { Dal::AAD::Rewind(*t_); }
        ~TapeGuard_() {
            try {
                Dal::AAD::Rewind(*t_);
            } catch (...) {
                // swallow; we are unwinding
            }
        }
        TapeGuard_(const TapeGuard_&) = delete;
        TapeGuard_& operator=(const TapeGuard_&) = delete;
    };

} // namespace Dal
