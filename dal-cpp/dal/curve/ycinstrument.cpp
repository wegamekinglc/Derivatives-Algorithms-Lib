//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {

    namespace {
        PeriodLength_ PeriodFromMonths(int months) {
            REQUIRE(months == 1 || months == 3 || months == 6 || months == 12, "Unsupported calibration instrument frequency");
            return PeriodLength_(String::FromInt(months) + "M");
        }

        Handle_<DayBasis::Context_> SinglePeriodContext(const Date_& start, const Date_& maturity, int couponMonths) {
            const bool singleCouponPeriodIsLast = true;
            return Handle_<DayBasis::Context_>(new DayBasis::Context_(singleCouponPeriodIsLast, start, maturity, couponMonths));
        }

        const DiscountCurve_& ResolveDiscountCurve(const YieldCurve_& yc,
                                                   const Handle_<YieldCurve_>& fallback,
                                                   const CollateralType_& collateral) {
            const YieldCurve_& primary = yc;
            if (primary.HasDiscount(collateral))
                return primary.Discount(collateral);
            REQUIRE(fallback, "Instrument pricing requires a discount curve context");
            REQUIRE(fallback->HasDiscount(collateral), "Fallback pricing context does not contain requested discount curve");
            return fallback->Discount(collateral);
        }

        const DiscountCurve_& ResolveForecastCurve(const YieldCurve_& yc,
                                                   const Handle_<YieldCurve_>& fallback,
                                                   const RateIndexConvention_& convention) {
            if (!convention.useProjectionCurve_)
                return ResolveDiscountCurve(yc, fallback, convention.collateral_);
            if (yc.HasForward(convention.forecastTenor_))
                return yc.Forward(convention.forecastTenor_, convention.collateral_);
            REQUIRE(fallback, "Instrument pricing requires a forecast curve context");
            REQUIRE(fallback->HasForward(convention.forecastTenor_), "Fallback pricing context does not contain requested forward curve");
            return fallback->Forward(convention.forecastTenor_, convention.collateral_);
        }

        double ForwardRate(const DiscountCurve_& forecast,
                           const Date_& start,
                           const Date_& maturity,
                           const DayBasis_& basis,
                           const Handle_<DayBasis::Context_>& context) {
            const double fwdDf = forecast(start, maturity);
            return (1.0 / fwdDf - 1.0) / basis(start, maturity, context.get());
        }

        AccrualPeriod_ MakeAccrualPeriod(const SchedulePeriod_& period, const DayBasis_& basis) {
            return AccrualPeriod_(period.accrualStart_, period.accrualEnd_, 1.0, basis, period.dayCountContext_, period.isStub_);
        }

        struct CouponPeriod_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };

        Vector_<CouponPeriod_> BuildLegPeriods(const Date_& start,
                                               const Date_& maturity,
                                               const RateLegConvention_& legConvention,
                                               int fixingLag,
                                               const Holidays_& fixingHolidays) {
            Vector_<CouponPeriod_> retval;
            for (const auto& period : MakeSchedulePeriods(start,
                                                          maturity,
                                                          legConvention.paymentFrequency_,
                                                          legConvention.accrualHolidays_,
                                                          fixingLag,
                                                          fixingHolidays,
                                                          legConvention.paymentLag_,
                                                          legConvention.paymentHolidays_,
                                                          DateGeneration_("Forward"),
                                                          legConvention.businessDayConvention_,
                                                          legConvention.paymentConvention_,
                                                          legConvention.endOfMonth_)) {
                retval.push_back({period, MakeAccrualPeriod(period, legConvention.dayBasis_)});
            }
            return retval;
        }

        class DepositRate_ : public YCInstrument_::Rate_ {
            Date_ start_;
            Date_ maturity_;
            RateIndexConvention_ convention_;
            Handle_<YieldCurve_> fallback_;
        public:
            DepositRate_(const Date_& start,
                         const Date_& maturity,
                         const RateIndexConvention_& convention,
                         const Handle_<YieldCurve_>& fallback)
                : start_(start), maturity_(maturity), convention_(convention), fallback_(fallback) {}

            double operator()(const YieldCurve_& yc) const override {
                SchedulePeriod_ period;
                period.unadjustedStart_ = start_;
                period.unadjustedEnd_ = maturity_;
                period.accrualStart_ = Holidays::Adjust(convention_.accrualHolidays_, start_, convention_.businessDayConvention_);
                period.accrualEnd_ = Holidays::Adjust(convention_.accrualHolidays_, maturity_, convention_.businessDayConvention_);
                period.dayCountContext_ = SinglePeriodContext(start_, maturity_, CouponMonths(start_, maturity_));
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, convention_);
                return ForwardRate(forecast,
                                   period.accrualStart_,
                                   period.accrualEnd_,
                                   convention_.dayBasis_,
                                   period.dayCountContext_);
            }
        };

        class ForwardRate_ : public YCInstrument_::Rate_ {
            Date_ start_;
            Date_ maturity_;
            double convexityAdjustment_;
            RateIndexConvention_ convention_;
            Handle_<YieldCurve_> fallback_;
        public:
            ForwardRate_(const Date_& start,
                         const Date_& maturity,
                         double convexityAdjustment,
                         const RateIndexConvention_& convention,
                         const Handle_<YieldCurve_>& fallback)
                : start_(start),
                  maturity_(maturity),
                  convexityAdjustment_(convexityAdjustment),
                  convention_(convention),
                  fallback_(fallback) {}

            double operator()(const YieldCurve_& yc) const override {
                SchedulePeriod_ period;
                period.unadjustedStart_ = start_;
                period.unadjustedEnd_ = maturity_;
                period.accrualStart_ = Holidays::Adjust(convention_.accrualHolidays_, start_, convention_.businessDayConvention_);
                period.accrualEnd_ = Holidays::Adjust(convention_.accrualHolidays_, maturity_, convention_.businessDayConvention_);
                period.dayCountContext_ = SinglePeriodContext(start_,
                                                              maturity_,
                                                              convention_.useProjectionCurve_
                                                                  ? convention_.forecastTenor_.Months()
                                                                  : CouponMonths(start_, maturity_));
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, convention_);
                return ForwardRate(forecast,
                                   period.accrualStart_,
                                   period.accrualEnd_,
                                   convention_.dayBasis_,
                                   period.dayCountContext_) - convexityAdjustment_;
            }
        };

        class SwapRate_ : public YCInstrument_::Rate_ {
            Date_ tradeDate_;
            Vector_<CouponPeriod_> fixedPeriods_;
            Vector_<CouponPeriod_> floatPeriods_;
            RateIndexConvention_ floatIndexConvention_;
            Handle_<YieldCurve_> fallback_;
        public:
            SwapRate_(const Date_& tradeDate,
                      const Vector_<CouponPeriod_>& fixedPeriods,
                      const Vector_<CouponPeriod_>& floatPeriods,
                      const RateIndexConvention_& floatIndexConvention,
                      const Handle_<YieldCurve_>& fallback)
                : tradeDate_(tradeDate),
                  fixedPeriods_(fixedPeriods),
                  floatPeriods_(floatPeriods),
                  floatIndexConvention_(floatIndexConvention),
                  fallback_(fallback) {}

            double operator()(const YieldCurve_& yc) const override {
                const DiscountCurve_& discount = ResolveDiscountCurve(yc, fallback_, floatIndexConvention_.collateral_);
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, floatIndexConvention_);

                double annuity = 0.0;
                for (const auto& period : fixedPeriods_)
                    annuity += period.accrual_.dcf_ * discount(tradeDate_, period.schedule_.paymentDate_);
                REQUIRE(annuity > 0.0, "Swap pricing requires positive fixed-leg annuity");

                double floatPv = 0.0;
                for (const auto& period : floatPeriods_) {
                    const double fixing = ForwardRate(forecast,
                                                      period.schedule_.accrualStart_,
                                                      period.schedule_.accrualEnd_,
                                                      floatIndexConvention_.dayBasis_,
                                                      period.schedule_.dayCountContext_);
                    floatPv += fixing * period.accrual_.dcf_ * discount(tradeDate_, period.schedule_.paymentDate_);
                }
                return floatPv / annuity;
            }
        };

        class BasisSwapRate_ : public YCInstrument_::Rate_ {
            Date_ tradeDate_;
            Vector_<CouponPeriod_> spreadPeriods_;
            Vector_<CouponPeriod_> referencePeriods_;
            RateIndexConvention_ spreadIndexConvention_;
            RateIndexConvention_ referenceIndexConvention_;
            Handle_<YieldCurve_> fallback_;
        public:
            BasisSwapRate_(const Date_& tradeDate,
                           const Vector_<CouponPeriod_>& spreadPeriods,
                           const Vector_<CouponPeriod_>& referencePeriods,
                           const RateIndexConvention_& spreadIndexConvention,
                           const RateIndexConvention_& referenceIndexConvention,
                           const Handle_<YieldCurve_>& fallback)
                : tradeDate_(tradeDate),
                  spreadPeriods_(spreadPeriods),
                  referencePeriods_(referencePeriods),
                  spreadIndexConvention_(spreadIndexConvention),
                  referenceIndexConvention_(referenceIndexConvention),
                  fallback_(fallback) {}

            double operator()(const YieldCurve_& yc) const override {
                const DiscountCurve_& discount = ResolveDiscountCurve(yc, fallback_, spreadIndexConvention_.collateral_);
                const DiscountCurve_& spreadForecast = ResolveForecastCurve(yc, fallback_, spreadIndexConvention_);
                const DiscountCurve_& referenceForecast = ResolveForecastCurve(yc, fallback_, referenceIndexConvention_);

                double spreadBasePv = 0.0;
                double spreadAnnuity = 0.0;
                for (const auto& period : spreadPeriods_) {
                    const double fixing = ForwardRate(spreadForecast,
                                                      period.schedule_.accrualStart_,
                                                      period.schedule_.accrualEnd_,
                                                      spreadIndexConvention_.dayBasis_,
                                                      period.schedule_.dayCountContext_);
                    const double df = discount(tradeDate_, period.schedule_.paymentDate_);
                    spreadBasePv += fixing * period.accrual_.dcf_ * df;
                    spreadAnnuity += period.accrual_.dcf_ * df;
                }
                REQUIRE(spreadAnnuity > 0.0, "Basis swap pricing requires positive spread-leg annuity");

                double referencePv = 0.0;
                for (const auto& period : referencePeriods_) {
                    const double fixing = ForwardRate(referenceForecast,
                                                      period.schedule_.accrualStart_,
                                                      period.schedule_.accrualEnd_,
                                                      referenceIndexConvention_.dayBasis_,
                                                      period.schedule_.dayCountContext_);
                    referencePv += fixing * period.accrual_.dcf_ * discount(tradeDate_, period.schedule_.paymentDate_);
                }

                return (referencePv - spreadBasePv) / spreadAnnuity;
            }
        };
    } // namespace

    Deposit_::Deposit_(const Date_& today, const Date_& maturity, double marketRate, const DayBasis_& basis)
        : Deposit_(today, today, maturity, marketRate, RateIndexConvention_{0, 0, false, PeriodLength_("3M"), basis}) {}

    Deposit_::Deposit_(const Date_& tradeDate,
                       const Date_& start,
                       const Date_& maturity,
                       double marketRate,
                       const RateIndexConvention_& convention)
        : tradeDate_(tradeDate), start_(start), maturity_(maturity), marketRate_(marketRate), convention_(convention) {}

    Deposit_::~Deposit_() = default;

    String_ Deposit_::Name() const { return "Deposit"; }

    pair<Date_, Date_> Deposit_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Deposit_::Precompute(const Handle_<YieldCurve_>& funding_yc) const {
        return Handle_<Rate_>(new DepositRate_(start_, maturity_, convention_, funding_yc));
    }

    FRA_::FRA_(const Date_& tradeDate,
               const Date_& start,
               const Date_& maturity,
               double marketRate,
               const RateIndexConvention_& convention)
        : tradeDate_(tradeDate), start_(start), maturity_(maturity), marketRate_(marketRate), convention_(convention) {}

    FRA_::~FRA_() = default;

    String_ FRA_::Name() const { return "FRA"; }

    pair<Date_, Date_> FRA_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> FRA_::Precompute(const Handle_<YieldCurve_>& funding_yc) const {
        return Handle_<Rate_>(new ForwardRate_(start_, maturity_, 0.0, convention_, funding_yc));
    }

    Future_::Future_(const Date_& tradeDate,
                     const Date_& start,
                     const Date_& maturity,
                     double marketRate,
                     const RateIndexConvention_& convention,
                     double convexityAdjustment)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          convexityAdjustment_(convexityAdjustment),
          convention_(convention) {}

    Future_::~Future_() = default;

    String_ Future_::Name() const { return "Future"; }

    pair<Date_, Date_> Future_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Future_::Precompute(const Handle_<YieldCurve_>& funding_yc) const {
        return Handle_<Rate_>(new ForwardRate_(start_, maturity_, convexityAdjustment_, convention_, funding_yc));
    }

    Swap_::Swap_(const Date_& today, const Date_& maturity, double marketRate, int freqMonths, const DayBasis_& basis)
        : Swap_(today,
                today,
                maturity,
                marketRate,
                RateLegConvention_{0, PeriodFromMonths(freqMonths), basis},
                RateIndexConvention_{0, 0, false, PeriodFromMonths(freqMonths), basis},
                RateLegConvention_{0, PeriodFromMonths(freqMonths), basis}) {}

    Swap_::Swap_(const Date_& tradeDate,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const RateLegConvention_& fixedLegConvention,
                 const RateIndexConvention_& floatIndexConvention,
                 const RateLegConvention_& floatLegConvention)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          fixedLegConvention_(fixedLegConvention),
          floatIndexConvention_(floatIndexConvention),
          floatLegConvention_(floatLegConvention) {}

    Swap_::~Swap_() = default;

    String_ Swap_::Name() const { return "Swap"; }

    pair<Date_, Date_> Swap_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> Swap_::Precompute(const Handle_<YieldCurve_>& funding_yc) const {
        const auto fixedPeriods = BuildLegPeriods(start_,
                                                  maturity_,
                                                  fixedLegConvention_,
                                                  0,
                                                  Holidays::None());
        const auto floatPeriods = BuildLegPeriods(start_,
                                                  maturity_,
                                                  floatLegConvention_,
                                                  floatIndexConvention_.fixingLag_,
                                                  floatIndexConvention_.fixingHolidays_);
        return Handle_<Rate_>(new SwapRate_(tradeDate_, fixedPeriods, floatPeriods, floatIndexConvention_, funding_yc));
    }

    OISSwap_::OISSwap_(const Date_& tradeDate,
                       const Date_& start,
                       const Date_& maturity,
                       double marketRate,
                       const RateLegConvention_& fixedLegConvention,
                       const RateIndexConvention_& overnightConvention,
                       const RateLegConvention_& floatLegConvention)
        : Swap_(tradeDate, start, maturity, marketRate, fixedLegConvention, overnightConvention, floatLegConvention) {}

    OISSwap_::~OISSwap_() = default;

    String_ OISSwap_::Name() const { return "OISSwap"; }

    BasisSwap_::BasisSwap_(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const RateIndexConvention_& spreadIndexConvention,
                           const RateLegConvention_& spreadLegConvention,
                           const RateIndexConvention_& referenceIndexConvention,
                           const RateLegConvention_& referenceLegConvention)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          spreadIndexConvention_(spreadIndexConvention),
          spreadLegConvention_(spreadLegConvention),
          referenceIndexConvention_(referenceIndexConvention),
          referenceLegConvention_(referenceLegConvention) {}

    BasisSwap_::~BasisSwap_() = default;

    String_ BasisSwap_::Name() const { return "BasisSwap"; }

    pair<Date_, Date_> BasisSwap_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<YCInstrument_::Rate_> BasisSwap_::Precompute(const Handle_<YieldCurve_>& funding_yc) const {
        const auto spreadPeriods = BuildLegPeriods(start_,
                                                   maturity_,
                                                   spreadLegConvention_,
                                                   spreadIndexConvention_.fixingLag_,
                                                   spreadIndexConvention_.fixingHolidays_);
        const auto referencePeriods = BuildLegPeriods(start_,
                                                      maturity_,
                                                      referenceLegConvention_,
                                                      referenceIndexConvention_.fixingLag_,
                                                      referenceIndexConvention_.fixingHolidays_);
        return Handle_<Rate_>(new BasisSwapRate_(tradeDate_,
                                                 spreadPeriods,
                                                 referencePeriods,
                                                 spreadIndexConvention_,
                                                 referenceIndexConvention_,
                                                 funding_yc));
    }

    STIR_::STIR_(const Date_& today,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const DayBasis_& basis)
        : STIR_(today, start, maturity, marketRate, RateIndexConvention_{0, 0, false, PeriodLength_("3M"), basis}) {}

    STIR_::STIR_(const Date_& tradeDate,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const RateIndexConvention_& convention)
        : FRA_(tradeDate, start, maturity, marketRate, convention) {}

    STIR_::~STIR_() = default;

    String_ STIR_::Name() const { return "STIR"; }

} // namespace Dal
