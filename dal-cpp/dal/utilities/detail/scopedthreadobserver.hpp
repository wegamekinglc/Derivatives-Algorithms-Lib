//
// Created by Codex on 2026/9/16.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal::Detail {
    //  Swaps a thread_local observer for a scope; the Slot_ accessor anchors the slot in the library defining it
    template <class O_, O_*& (*Slot_)()> class ScopedThreadObserver_ : noncopyable {
        O_* previous_;

    public:
        explicit ScopedThreadObserver_(O_* observer) : previous_(Slot_()) { Slot_() = observer; }
        ~ScopedThreadObserver_() { Slot_() = previous_; }
    };
} // namespace Dal::Detail
