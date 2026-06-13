//
// Created by GitHub Copilot on 2026/6/6.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {
    namespace {
        struct XccyCouponPeriod_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };

        AccrualPeriod_ MakeAccrualPeriod(const SchedulePeriod_& period, const DayBasis_& basis) {
            return AccrualPeriod_(period.accrualStart_, period.accrualEnd_, 1.0, basis, period.dayCountContext_, period.isStub_);
        }

        Vector_<XccyCouponPeriod_> BuildLegPeriods(const Date_& start,
                                                   const Date_& maturity,
                                                   const RateLegConvention_& legConvention,
                                                   int fixingLag,
                                                   const Holidays_& fixingHolidays) {
            Vector_<XccyCouponPeriod_> retval;
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

        double ForwardRate(const DiscountCurve_& forecast,
                           const XccyCouponPeriod_& period,
                           const DayBasis_& basis) {
            const double df = forecast(period.schedule_.accrualStart_, period.schedule_.accrualEnd_);
            REQUIRE(df > 0.0, "Cross-currency forward-rate calculation requires positive discount factors");
            return (1.0 / df - 1.0) / basis(period.schedule_.accrualStart_,
                                            period.schedule_.accrualEnd_,
                                            period.schedule_.dayCountContext_.get());
        }

        double CouponPv(const Vector_<XccyCouponPeriod_>& periods,
                        const DiscountCurve_& discount,
                        const DiscountCurve_& forecast,
                        const Date_& valueDate,
                        double notional,
                        const RateIndexConvention_& indexConvention,
                        double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                retval += notional * fixing * period.accrual_.dcf_ * discount(valueDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        double ConvertedCouponPv(const Vector_<XccyCouponPeriod_>& periods,
                                 const CrossCurrencyMarket_& market,
                                 const DiscountCurve_& discount,
                                 const DiscountCurve_& forecast,
                                 const Date_& valueDate,
                                 double notional,
                                 const RateIndexConvention_& indexConvention,
                                 double spread) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double fixing = ForwardRate(forecast, period, indexConvention.dayBasis_) + spread;
                const double discountFactor = discount(valueDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(valueDate, period.schedule_.paymentDate_);
                retval += notional * fixing * period.accrual_.dcf_ * discountFactor * market.FxSpot();
            }
            return retval;
        }

        double ConvertedAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                                const CrossCurrencyMarket_& market,
                                const DiscountCurve_& discount,
                                const Date_& valueDate,
                                double notional) {
            double retval = 0.0;
            for (const auto& period : periods) {
                const double discountFactor = discount(valueDate, period.schedule_.paymentDate_)
                                              / market.BasisDiscountFactor(valueDate, period.schedule_.paymentDate_);
                retval += notional * period.accrual_.dcf_ * discountFactor * market.FxSpot();
            }
            return retval;
        }

        double DomesticAnnuity(const Vector_<XccyCouponPeriod_>& periods,
                               const DiscountCurve_& discount,
                               const Date_& valueDate) {
            double retval = 0.0;
            for (const auto& period : periods) {
                retval += period.accrual_.dcf_ * discount(valueDate, period.schedule_.paymentDate_);
            }
            return retval;
        }

        class CrossCurrencySwapRate_ : public CrossCurrencySwap_::Rate_ {
            Date_ maturity_;
            CurrencyPair_ pair_;
            double domesticNotional_;
            double foreignNotional_;
            Vector_<XccyCouponPeriod_> domesticPeriods_;
            Vector_<XccyCouponPeriod_> foreignPeriods_;
            RateIndexConvention_ domesticIndexConvention_;
            RateIndexConvention_ foreignIndexConvention_;
            CrossCurrencyConvention_ convention_;

        public:
            CrossCurrencySwapRate_(const Date_& maturity,
                                   const CurrencyPair_& pair,
                                   double domesticNotional,
                                   double foreignNotional,
                                   const Vector_<XccyCouponPeriod_>& domesticPeriods,
                                   const Vector_<XccyCouponPeriod_>& foreignPeriods,
                                   const RateIndexConvention_& domesticIndexConvention,
                                   const RateIndexConvention_& foreignIndexConvention,
                                   const CrossCurrencyConvention_& convention)
                : maturity_(maturity),
                  pair_(pair),
                  domesticNotional_(domesticNotional),
                  foreignNotional_(foreignNotional),
                  domesticPeriods_(domesticPeriods),
                  foreignPeriods_(foreignPeriods),
                  domesticIndexConvention_(domesticIndexConvention),
                  foreignIndexConvention_(foreignIndexConvention),
                  convention_(convention) {}

            double operator()(const CrossCurrencyMarket_& market) const override {
                REQUIRE(pair_.domestic_ == market.DomesticCcy() && pair_.foreign_ == market.ForeignCcy(),
                        "Cross-currency swap currency pair does not match the pricing market orientation");
                const Date_ valueDate = market.Today();
                REQUIRE(!domesticPeriods_.empty() && !foreignPeriods_.empty(), "Cross-currency swap requires scheduled periods on both legs");
                REQUIRE(valueDate <= domesticPeriods_.front().schedule_.accrualStart_ &&
                            valueDate <= foreignPeriods_.front().schedule_.accrualStart_,
                        "Cross-currency swap pricing of in-progress swaps (evaluation date after swap start) is not supported");
                const DiscountCurve_& domesticDiscount = market.DomesticDiscountCurve(domesticIndexConvention_.collateral_);
                const DiscountCurve_& domesticForecast = market.DomesticForwardCurve(domesticIndexConvention_.forecastTenor_,
                                                                                     domesticIndexConvention_.collateral_);
                const DiscountCurve_& foreignForecast = market.ForeignForwardCurve(foreignIndexConvention_.forecastTenor_,
                                                                                   foreignIndexConvention_.collateral_);
                const DiscountCurve_& foreignDiscount = market.ForeignDiscountCurve(foreignIndexConvention_.collateral_);

                const double domesticBase = CouponPv(domesticPeriods_,
                                                     domesticDiscount,
                                                     domesticForecast,
                                                     valueDate,
                                                     domesticNotional_,
                                                     domesticIndexConvention_,
                                                     0.0);
                const double domesticSpreadAnnuity = domesticNotional_ * DomesticAnnuity(domesticPeriods_, domesticDiscount, valueDate);
                const double foreignBase = ConvertedCouponPv(foreignPeriods_,
                                                             market,
                                                             foreignDiscount,
                                                             foreignForecast,
                                                             valueDate,
                                                             foreignNotional_,
                                                             foreignIndexConvention_,
                                                             0.0);
                const double foreignSpreadAnnuity = ConvertedAnnuity(foreignPeriods_,
                                                                     market,
                                                                     foreignDiscount,
                                                                     valueDate,
                                                                     foreignNotional_);

                double domesticPv = domesticBase;
                double foreignPv = foreignBase;
                if (convention_.initialNotionalExchange_) {
                    const Date_ domesticStart = domesticPeriods_.front().schedule_.accrualStart_;
                    const Date_ foreignStart = foreignPeriods_.front().schedule_.accrualStart_;
                    domesticPv -= domesticNotional_ * domesticDiscount(valueDate, domesticStart);
                    foreignPv -= foreignNotional_ * foreignDiscount(valueDate, foreignStart)
                                 / market.BasisDiscountFactor(valueDate, foreignStart) * market.FxSpot();
                }
                if (convention_.finalNotionalExchange_) {
                    domesticPv += domesticNotional_ * domesticDiscount(valueDate, maturity_);
                    foreignPv += foreignNotional_ * foreignDiscount(valueDate, maturity_)
                                 / market.BasisDiscountFactor(valueDate, maturity_) * market.FxSpot();
                }

                if (convention_.spreadOnForeignLeg_) {
                    REQUIRE(foreignSpreadAnnuity > 0.0, "Cross-currency swap requires positive foreign spread annuity");
                    return (domesticPv - foreignPv) / foreignSpreadAnnuity;
                }
                REQUIRE(domesticSpreadAnnuity > 0.0, "Cross-currency swap requires positive domestic spread annuity");
                return (foreignPv - domesticPv) / domesticSpreadAnnuity;
            }
        };
    } // namespace

    CurrencyPair_::CurrencyPair_() : domestic_(Ccy_::Value_::USD), foreign_(Ccy_::Value_::EUR) {}

    CurrencyPair_::CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign) : domestic_(domestic), foreign_(foreign) {
        REQUIRE(!(domestic_ == foreign_), "CurrencyPair_ requires two distinct currencies");
    }

    CurrencyPair_ CurrencyPair_::Reversed() const { return CurrencyPair_(foreign_, domestic_); }

    bool CurrencyPair_::operator<(const CurrencyPair_& rhs) const {
        if (domestic_ < rhs.domestic_ || rhs.domestic_ < domestic_)
            return domestic_ < rhs.domestic_;
        return foreign_ < rhs.foreign_;
    }

    bool CurrencyPair_::operator==(const CurrencyPair_& rhs) const {
        return domestic_ == rhs.domestic_ && foreign_ == rhs.foreign_;
    }

    CrossCurrencySwap_::CrossCurrencySwap_(const Date_& tradeDate,
                                           const Date_& start,
                                           const Date_& maturity,
                                           double marketRate,
                                           const CurrencyPair_& pair,
                                           double domesticNotional,
                                           double foreignNotional,
                                           const CrossCurrencyConvention_& convention)
        : tradeDate_(tradeDate),
          start_(start),
          maturity_(maturity),
          marketRate_(marketRate),
          pair_(pair),
          domesticNotional_(domesticNotional),
          foreignNotional_(foreignNotional),
          convention_(convention) {
        REQUIRE(domesticNotional_ > 0.0 && foreignNotional_ > 0.0, "CrossCurrencySwap_ requires positive notionals");
        REQUIRE(maturity_ > start_, "CrossCurrencySwap_ requires maturity after start");
    }

    String_ CrossCurrencySwap_::Name() const { return "CrossCurrencySwap"; }

    pair<Date_, Date_> CrossCurrencySwap_::TimeSpan() const { return {start_, maturity_}; }

    Handle_<CrossCurrencySwap_::Rate_> CrossCurrencySwap_::Precompute() const {
        REQUIRE(!convention_.resettableNotional_, "Resettable cross-currency notionals are not implemented");
        REQUIRE(!convention_.markToMarketNotional_, "Mark-to-market cross-currency notionals are not implemented");
        const auto domesticPeriods = BuildLegPeriods(start_,
                                                     maturity_,
                                                     convention_.domesticLeg_,
                                                     convention_.domesticIndex_.fixingLag_,
                                                     convention_.domesticIndex_.fixingHolidays_);
        const auto foreignPeriods = BuildLegPeriods(start_,
                                                    maturity_,
                                                    convention_.foreignLeg_,
                                                    convention_.foreignIndex_.fixingLag_,
                                                    convention_.foreignIndex_.fixingHolidays_);
        return Handle_<Rate_>(new CrossCurrencySwapRate_(maturity_,
                                                         pair_,
                                                         domesticNotional_,
                                                         foreignNotional_,
                                                         domesticPeriods,
                                                         foreignPeriods,
                                                         convention_.domesticIndex_,
                                                         convention_.foreignIndex_,
                                                         convention_));
    }
} // namespace Dal
