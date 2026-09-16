//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/utilities/detail/scopedthreadobserver.hpp>

namespace Dal::Script::Detail {
    struct SimulationObserver_ {
        virtual ~SimulationObserver_() = default;
        virtual void BeforeCompilation() {}
        virtual void AfterSubmission() = 0;
    };

    SimulationObserver_*& SimulationObserver();

    using ScopedSimulationObserver_ = Dal::Detail::ScopedThreadObserver_<SimulationObserver_, &SimulationObserver>;
} // namespace Dal::Script::Detail
