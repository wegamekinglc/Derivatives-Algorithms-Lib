//
// Created by Codex on 2026/9/25.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

// Keep small, nonzero diagnostics visible without switching to scientific notation.
inline std::string ExampleFloat(double value, int minDecimals = 2, int minSignificantDigits = 4) {
    if (std::isnan(value))
        return "nan";
    if (std::isinf(value))
        return std::signbit(value) ? "-inf" : "inf";
    int decimals = minDecimals;
    if (value != 0.0)
        decimals = std::max(decimals, minSignificantDigits - 1 - static_cast<int>(std::floor(std::log10(std::fabs(value)))));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}
