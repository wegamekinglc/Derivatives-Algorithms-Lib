//
// Created by wegamekinglc on 2026/7/19.
//

#pragma once

#include <algorithm>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    // Calibration settings dictionaries must not silently drop misspelled keys:
    // name the offending key and the accepted set so sheet authors can fix it.
    inline void RequireKnownSettingsKey(const String_& key, const Vector_<String_>& validKeys) {
        if (std::find(validKeys.begin(), validKeys.end(), key) == validKeys.end())
            THROW("Unknown settings key '" + key + "' (valid keys: " + String::Accumulate(validKeys, ", ") + ")");
    }
} // namespace Dal
