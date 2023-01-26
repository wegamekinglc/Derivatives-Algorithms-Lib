//
// Created by wegam on 2022/5/5.
//

#include <dal/math/operators.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/optiontype.hpp>

namespace Dal {
#include <dal/auto/MG_OptionType_enum.inc>

    double OptionType_::Payout(double spot, double strike) const {
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