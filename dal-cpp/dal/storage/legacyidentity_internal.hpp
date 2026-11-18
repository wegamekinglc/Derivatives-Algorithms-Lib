#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    class DateTime_;
    class Storable_;
    namespace LegacyIdentity {
        String_ DateTimeString(const DateTime_& value);
        String_ ObjectJson(const Storable_& value);
    } // namespace LegacyIdentity
} // namespace Dal
