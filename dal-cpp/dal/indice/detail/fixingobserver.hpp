//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/detail/scopedthreadobserver.hpp>

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

        using ScopedFixingReadObserver_ = ScopedThreadObserver_<FixingReadObserver_, &FixingReadObserver>;
    } // namespace Detail
} // namespace Dal
