//
// Created by dal-implementer on 2026/6/14.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration LogDfScheme
    Selection of interpolation scheme for a LOG_DISCOUNT curve
switchable
alternative LOG_LINEAR
alternative LOG_CUBIC_NATURAL
alternative MIXED
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_LogDfScheme_enum.hpp>
} // namespace Dal
