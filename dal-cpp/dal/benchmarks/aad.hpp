//
// Created on 2026/10/04.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <dal/math/aad/statistics.hpp>
#include <dal/utilities/exceptions.hpp>
#include <iostream>

namespace Dal::Bench {
    inline void PrintTapeStatistics(const char* name, const AAD::Tape_& tape, size_t parameters, size_t outputs) {
        const auto usage = AAD::MeasureTape(tape);
        std::cout << "AAD_TAPE case=\"" << name << "\" nodes=" << usage.nodes_ << " edges=" << usage.edges_ << " blocks=" << usage.blocks_
                  << " live_bytes=" << usage.liveBytes_ << " occupied_bytes=" << usage.occupiedBytes_ << " capacity_bytes=" << usage.capacityBytes_
                  << " parameters=" << parameters << " outputs=" << outputs << " width=" << (tape.multi_ ? tape.numAdj_ : 1) << '\n';
    }

    inline void VerifyAadResult(double actual, double expected) {
        REQUIRE(std::isfinite(actual) && std::fabs(actual - expected) <= 1e-10 * std::max(1.0, std::fabs(expected)),
                "AAD benchmark result differs from the independent reference");
    }
} // namespace Dal::Bench
