//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <dal/math/aad/aad.hpp>

namespace Dal {

    // RAII guard that clears the AAD tape on construction and destruction.
    // Used around analytic-Jacobian tape sweeps that must not interfere with
    // any outer tape recording. Single-threaded.
    struct TapeGuard_ {
        Dal::AAD::Tape_* t_;
        explicit TapeGuard_(Dal::AAD::Tape_* t) : t_(t) { Dal::AAD::Clear(*t_); }
        ~TapeGuard_() {
            try {
                Dal::AAD::Clear(*t_);
            } catch (...) {
                // swallow; we are unwinding
            }
        }
        TapeGuard_(const TapeGuard_&) = delete;
        TapeGuard_& operator=(const TapeGuard_&) = delete;
    };

} // namespace Dal
