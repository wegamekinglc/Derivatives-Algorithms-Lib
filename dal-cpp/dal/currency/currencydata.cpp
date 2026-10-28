//
// Created by wegam on 2022/2/2.
//

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/time/holidays.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/facts.hpp>
#include <dal/utilities/factsinternal.hpp>

namespace Dal {
    namespace Ccy {
#define SINGLETON_FACT_ACCESSOR(type, func)                                                                                                          \
    const Fact_<type>& Conventions::func() { RETURN_STATIC(FactInternal::StoredFact_<Ccy_, type>); }

        SINGLETON_FACT_ACCESSOR(Holidays_, SwapPayHolidays);
        SINGLETON_FACT_ACCESSOR(Holidays_, LiborFixHolidays);
        SINGLETON_FACT_ACCESSOR(int, LiborFixDays);
        SINGLETON_FACT_ACCESSOR(DayBasis_, LiborDayBasis);
        SINGLETON_FACT_ACCESSOR(PeriodLength_, SwapFixedPeriod);
        SINGLETON_FACT_ACCESSOR(TradedRate_, SwapFloatIndex);
        SINGLETON_FACT_ACCESSOR(DayBasis_, SwapFixedDayBasis);
        SINGLETON_FACT_ACCESSOR(RateIndexConvention_, OisIndex);
        SINGLETON_FACT_ACCESSOR(RateIndexConvention_, LiborIndex);
        SINGLETON_FACT_ACCESSOR(RateLegConvention_, SwapFixedLeg);
        SINGLETON_FACT_ACCESSOR(RateLegConvention_, SwapFloatLeg);
        SINGLETON_FACT_ACCESSOR(CrossCurrencyConvention_, Xcs);
    } // namespace Ccy
} // namespace Dal
