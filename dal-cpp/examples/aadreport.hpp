//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal::Examples {
    inline void PrintReferenceHeader(const char* title) {
        std::cout << '\n'
                  << title << '\n'
                  << std::left << std::setw(36) << "Quantity" << std::right << std::setw(18) << "Native" << std::setw(18) << "Reference"
                  << std::setw(16) << "Absolute error" << '\n'
                  << std::string(88, '-') << '\n'
                  << std::fixed << std::setprecision(8);
    }

    inline void Compare(const char* quantity, double actual, double reference, double tolerance = 1e-10) {
        const double error = std::abs(actual - reference);
        std::cout << std::left << std::setw(36) << quantity << std::right << std::setw(18) << actual << std::setw(18) << reference << std::scientific
                  << std::setw(16) << error << std::fixed << '\n';
        REQUIRE(std::isfinite(actual) && error <= tolerance, String_("AAD example: ") + quantity + " reference mismatch");
    }
} // namespace Dal::Examples
