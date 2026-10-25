#pragma once

#include <dal-public/src/xccymontecarlo.hpp>
#include <dal/currency/currencydata.hpp>

namespace Dal::Examples {
    inline CrossCurrencySwapConfig_ XccyContract() {
        using namespace Dal;
        CrossCurrencySwapConfig_ config;
        config.domesticNotional_ = 110.;
        config.foreignNotional_ = 100.;
        auto& c = config.convention_;
        c.initialNotionalExchange_ = c.finalNotionalExchange_ = true;
        c.domesticLeg_.paymentFrequency_ = PeriodLength_("3M");
        c.foreignLeg_.paymentFrequency_ = PeriodLength_("6M");
        c.foreignLeg_.paymentLag_ = 2;
        for (auto* leg : {&c.domesticLeg_, &c.foreignLeg_}) {
            leg->dayBasis_ = DayBasis_("ACT_360");
            leg->businessDayConvention_ = leg->paymentConvention_ = BizDayConvention_("Unadjusted");
        }
        c.domesticIndex_.forecastTenor_ = PeriodLength_("3M");
        c.foreignIndex_.forecastTenor_ = PeriodLength_("6M");
        for (auto* index : {&c.domesticIndex_, &c.foreignIndex_}) {
            index->useProjectionCurve_ = true;
            index->fixingLag_ = 2;
            index->dayBasis_ = DayBasis_("ACT_360");
        }
        c.domesticIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("USD"));
        c.foreignIndex_.fixingHolidays_ = Ccy::Conventions::LiborFixHolidays()(Ccy_("EUR"));
        config.domesticRateFixing_ = {"IR:USD,LIBOR_3M_LCH", 0, 0};
        config.foreignRateFixing_ = {"IR:EUR,LIBOR_6M_LCH", 0, 0};
        return config;
    }
} // namespace Dal::Examples
