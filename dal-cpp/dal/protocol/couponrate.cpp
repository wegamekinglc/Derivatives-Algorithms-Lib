//
// Created by wegam on 2022/1/29.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/clearer.hpp>
#include <dal/time/periodlength.hpp>

namespace Dal {
#include <dal/auto/MG_TradedRate_enum.inc>

    PeriodLength_ TradedRate_::Period() const {
        static const PeriodLength_ QUARTERLY("3M");
        static const PeriodLength_ SEMI("6M");
        switch (Switch()) {
        case Value_::LIBOR_3M_CME:
        case Value_::LIBOR_3M_LCH:
        case Value_::LIBOR_3M_FUT:
            return QUARTERLY;
        case Value_::LIBOR_6M_CME:
        case Value_::LIBOR_6M_LCH:
            return SEMI;
        default:
            THROW("can't find period length of traded rate");
        }
    }

    LiborRate_::LiborRate_(const DateTime_& fix_date, const Ccy_& ccy, const TradedRate_& rate)
        : fixDate_(fix_date), ccy_(ccy), rate_(rate) {}

    Clearer_ TradedRate_::Clearer() const {
        switch (Switch()) {
        case Value_::LIBOR_3M_CME:
        case Value_::LIBOR_3M_FUT:
        case Value_::LIBOR_6M_CME:
            return Clearer_::Value_::CME;
        case Value_::LIBOR_3M_LCH:
        case Value_::LIBOR_6M_LCH:
            return Clearer_::Value_::LCH;
        default:
            THROW("can't find clearinghouse for traded rate");
        }
    }

    TradedRate_ FindRate(const PeriodLength_& period, const Clearer_& clearer) {
        switch (clearer.Switch()) {
        case Clearer_::Value_::LCH:
            switch (period.Months()) {
            case 3:
                return TradedRate_::Value_::LIBOR_3M_LCH;
            case 6:
                return TradedRate_::Value_::LIBOR_6M_LCH;
            default:
                THROW("Can't find traded rate for period/clearinghouse combination");
            }
        case Clearer_::Value_::CME:
            switch (period.Months()) {
            case 3:
                return TradedRate_::Value_::LIBOR_3M_CME;
            case 6:
                return TradedRate_::Value_::LIBOR_6M_CME;
            default:
                THROW("Can't find traded rate for period/clearinghouse combination");
            }
        default:
            THROW("Can't find traded rate for period/clearinghouse combination");
        }
    }
} // namespace Dal