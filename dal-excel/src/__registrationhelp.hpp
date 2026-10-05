//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <algorithm>

#include <dal-public/src/types.hpp>

namespace Dal::Excel {
    inline Vector_<String_> RegistrationHelp(const String_& names, const Vector_<String_>& help) {
        auto result = help;
        // Machinist adds the multi-output format argument after explicit input help.
        if (names.size() >= 7 && names.substr(names.size() - 7) == ",format" &&
            help.size() == static_cast<size_t>(std::count(names.begin(), names.end(), ',')))
            result.push_back("Desired screen layout of outputs; blank uses the default layout");
        return result;
    }
} // namespace Dal::Excel
