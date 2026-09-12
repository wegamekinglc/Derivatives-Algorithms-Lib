//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal::Script::Detail {
    struct SimulationObserver_ {
        virtual ~SimulationObserver_() = default;
        virtual void AfterSubmission() = 0;
    };

    SimulationObserver_*& SimulationObserver();

    class ScopedSimulationObserver_ : noncopyable {
        SimulationObserver_* previous_;

    public:
        explicit ScopedSimulationObserver_(SimulationObserver_* observer) : previous_(SimulationObserver()) { SimulationObserver() = observer; }
        ~ScopedSimulationObserver_() { SimulationObserver() = previous_; }
    };
} // namespace Dal::Script::Detail
