//
// Created by wegam on 2022/5/5.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/math/aad/operators.hpp>

namespace Dal {
#include <dal/auto/MG_OptionType_enum.inc>

    double OptionType_::Payout(double spot, double strike) const {
        switch (Switch()) {
        case Value_::CALL:
            return Max(0.0, spot - strike);
        case Value_::PUT:
            return Max(0.0, strike - spot);
        case Value_::STRADDLE:
            return Fabs(spot - strike);
        default:
            THROW("invalid option type");
        }
    }
}