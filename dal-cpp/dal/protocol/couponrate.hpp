//
// Created by wegam on 2022/1/28.
//

#pragma once

#include <dal/currency/currency.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/noncopyable.hpp>

/*IF--------------------------------------------------------------------------
enumeration TradedRate
   Quantities for which forecast curves are formed
switchable
alternative LIBOR_3M_CME
   3M Libor underlying CME swaps
alternative LIBOR_3M_LCH
   3M Libor underlying LCH swaps
alternative LIBOR_3M_FUT
   3M Libor underlying futures
alternative LIBOR_6M_CME
   6M Libor underlying CME swaps
alternative LIBOR_6M_LCH
   6M Libor underlying LCH swaps
method PeriodLength_ Period() const;
method Clearer_ Clearer() const;
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class PeriodLength_;
    class Clearer_;

    struct CouponRate_ : noncopyable {
        virtual ~CouponRate_() = default;
    };

    struct FixedRate_ : CouponRate_ {
        double rate_;
        explicit FixedRate_(double rate) : rate_(rate) {}
    };

#include <dal/auto/MG_TradedRate_enum.hpp>

    TradedRate_ FindRate(const PeriodLength_& period, const Clearer_& ch);

    struct LiborRate_ : CouponRate_ {
        DateTime_ fixDate_;
        Ccy_ ccy_;
        TradedRate_ rate_;
        LiborRate_(const DateTime_& fix_date, const Ccy_& ccy, const TradedRate_& rate);
    };

    struct SummedRate_ : CouponRate_ {
        Vector_<std::pair<double, Handle_<CouponRate_>>> rates_;
    };
} // namespace Dal
