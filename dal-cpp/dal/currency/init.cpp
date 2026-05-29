//
// Created by wegam on 2020/11/28.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/currency/init.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/protocol/clearer.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/time/holidays.hpp>

namespace Dal {

    bool CcyFacts_::init_ = false;
    std::mutex CcyFacts_::mutex_;

    void CcyFacts_::Init() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            Ccy::Conventions::SwapPayHolidays().XWrite().SetDefault(Holidays::None());
            Ccy::Conventions::LiborFixHolidays().XWrite().SetDefault(Holidays::None());
            Ccy::Conventions::LiborFixDays().XWrite().SetDefault(2);
            Ccy::Conventions::LiborFixDays().XWrite()(Ccy_("CNY"), 1);
            Ccy::Conventions::LiborFixHolidays().XWrite()(Ccy_("CNY"), Holidays_("CN.IB"));
            Ccy::Conventions::SwapFixedPeriod().XWrite().SetDefault(PeriodLength_("6M"));
            Ccy::Conventions::SwapFloatIndex().XWrite().SetDefault(FindRate(PeriodLength_("3M"), Clearer_("CME")));
            Ccy::Conventions::SwapFixedDayBasis().XWrite().SetDefault(DayBasis_("30_360"));

            RateIndexConvention_ oisConvention;
            oisConvention.dayBasis_ = DayBasis_("ACT_360");
            oisConvention.businessDayConvention_ = BizDayConvention_("Following");
            oisConvention.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
            Ccy::Conventions::OisIndex().XWrite().SetDefault(oisConvention);

            RateIndexConvention_ liborConvention;
            liborConvention.spotLag_ = 2;
            liborConvention.fixingLag_ = 2;
            liborConvention.useProjectionCurve_ = true;
            liborConvention.forecastTenor_ = PeriodLength_("3M");
            liborConvention.dayBasis_ = DayBasis_("ACT_360");
            liborConvention.businessDayConvention_ = BizDayConvention_("Following");
            liborConvention.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
            Ccy::Conventions::LiborDayBasis().XWrite().SetDefault(liborConvention.dayBasis_);
            Ccy::Conventions::LiborIndex().XWrite().SetDefault(liborConvention);

            RateIndexConvention_ cnyLiborConvention(liborConvention);
            cnyLiborConvention.spotLag_ = 1;
            cnyLiborConvention.fixingLag_ = 1;
            cnyLiborConvention.fixingHolidays_ = Holidays_("CN.IB");
            cnyLiborConvention.accrualHolidays_ = Holidays_("CN.IB");
            Ccy::Conventions::LiborIndex().XWrite()(Ccy_("CNY"), cnyLiborConvention);

            RateLegConvention_ fixedLegConvention;
            fixedLegConvention.paymentFrequency_ = PeriodLength_("6M");
            fixedLegConvention.dayBasis_ = DayBasis_("30_360");
            fixedLegConvention.businessDayConvention_ = BizDayConvention_("ModifiedFollowing");
            fixedLegConvention.paymentConvention_ = BizDayConvention_("ModifiedFollowing");
            Ccy::Conventions::SwapFixedLeg().XWrite().SetDefault(fixedLegConvention);

            RateLegConvention_ floatLegConvention;
            floatLegConvention.paymentFrequency_ = PeriodLength_("3M");
            floatLegConvention.dayBasis_ = DayBasis_("ACT_360");
            floatLegConvention.businessDayConvention_ = BizDayConvention_("ModifiedFollowing");
            floatLegConvention.paymentConvention_ = BizDayConvention_("ModifiedFollowing");
            Ccy::Conventions::SwapFloatLeg().XWrite().SetDefault(floatLegConvention);
            init_ = true;
        }
    }
} // namespace Dal
