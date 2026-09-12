//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    class Index_;
    class Environment_;

    namespace Detail {
        struct FixingReadObserver_ {
            virtual ~FixingReadObserver_() = default;
            virtual void BeforeHistory(const String_&) {}
            virtual void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) {}
        };

        FixingReadObserver_*& FixingReadObserver();

        class ScopedFixingReadObserver_ : noncopyable {
            FixingReadObserver_* previous_;

        public:
            explicit ScopedFixingReadObserver_(FixingReadObserver_* observer) : previous_(FixingReadObserver()) { FixingReadObserver() = observer; }
            ~ScopedFixingReadObserver_() { FixingReadObserver() = previous_; }
        };
    } // namespace Detail
} // namespace Dal
