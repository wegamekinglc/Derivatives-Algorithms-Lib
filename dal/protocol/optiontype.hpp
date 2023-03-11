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
method template<class T_> T_ Payout(T_ spot, T_ strike) const;
method OptionType_ Opposite() const;
-IF------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_OptionType_enum.hpp>
    template <class T_>
    T_ OptionType_::Payout(T_ spot, T_ strike) const {
        switch (Switch()) {
        case Value_::CALL:
            return max(0.0, spot - strike);
        case Value_::PUT:
            return max(0.0, strike - spot);
        case Value_::STRADDLE:
            return fabs(spot - strike);
        default:
            THROW("invalid option type");
        }
    }
}

