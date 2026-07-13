/*IF--------------------------------------------------------------------------
enumeration XccyNotionalMode
    Cross-currency notional evolution rule
switchable
alternative FIXED
alternative RESETTABLE
alternative MARK_TO_MARKET
-IF-------------------------------------------------------------------------*/

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
#include <dal/auto/MG_XccyNotionalMode_enum.hpp>
} // namespace Dal
