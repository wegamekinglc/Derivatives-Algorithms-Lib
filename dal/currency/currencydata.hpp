//
// Created by wegam on 2022/2/2.
// Per-currency data (e.g. conventions)
//

#pragma once

#include <dal/currency/currency.hpp>
#include <dal/utilities/facts.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    class Holidays_;
    class PeriodLength_;
    class TradedRate_;

    namespace Ccy {
        template <class T_> using Fact_ = OneFact_<Ccy_, T_>;

        namespace Conventions {
            const Fact_<Holidays_>& SwapPayHolidays();
            const Fact_<Holidays_>& LiborFixHolidays();
            const Fact_<int>& LiborFixDays();
            const Fact_<DayBasis_>& LiborDayBasis();
            const Fact_<PeriodLength_>& SwapFixedPeriod();
            const Fact_<TradedRate_>& SwapFloatIndex();
            const Fact_<DayBasis_>& SwapFixedDayBasis();
        }
    }
}