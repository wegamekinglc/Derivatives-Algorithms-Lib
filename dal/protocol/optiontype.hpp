//
// Created by wegam on 2022/5/5.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF-------------------------------------------------------
enumeration OptionType
    Call/put flag
switchable
alternative CALL C
alternative PUT P
alternative STRADDLE V C+P
method double Payout(double spot, double strike) const;
method OptionType_ Opposite() const;
-IF------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_OptionType_enum.hpp>


}

