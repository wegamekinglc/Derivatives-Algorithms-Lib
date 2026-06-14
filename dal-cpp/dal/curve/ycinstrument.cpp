//
// Created by wegam on 2026/4/19.
//

#include <map>
#include <set>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
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

        // Helper: detect whether the trade-date of a swap/basis-swap equals the discount curve's
        // anchor. We treat this as "the anchor is reachable as tradeDate_"; for tradeDate != anchor
        // the override returns empty and the bumped fallback engages.
        //
        // We need the discount curve's anchor date. DiscountLogDF_ exposes NodeDates(); we use
        // dynamic_cast. If the target is not a DiscountLogDF_, we return false (conservative --
        // forces the fallback).
        bool TradeDateIsAnchor(const DiscountCurve_& dc, const Date_& tradeDate) {
            const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&dc);
            if (logDf == nullptr)
                return false;
            return logDf->NodeDates().front() == tradeDate;
        }

        // Get the discount curve's anchor date, or empty if the curve is not a DiscountLogDF_.
        // The anchor is the curve's node-0 date -- the pinned logDF=0 reference.
        Handle_<Date_> CurveAnchor(const DiscountCurve_& dc) {
            const auto* logDf = dynamic_cast<const DiscountLogDF_*>(&dc);
            if (logDf == nullptr)
                return Handle_<Date_>();
            return Handle_<Date_>(new Date_(logDf->NodeDates().front()));
        }

        // Per-fixing primal value and anchor-DF derivatives for the Ibor-style fixing
        //   fixing = (D_s/D_m - 1)/yf,   with D_s = DF(anchor, s), D_m = DF(anchor, m).
        // Quotient rule collapses to dFixing/dD_s = 1/(yf*D_m), dFixing/dD_m = -D_s/(yf*D_m^2).
        // yf <= 0 (degenerate stub) yields zero derivatives and a primal of 0; the caller must
        // decide whether to use the returned fixing -- for the legs we touch this matches the
        // operator() ForwardRate shape exactly (ForwardRate divides by yf, so yf=0 is undefined
        // upstream and never reached here for a calibrated curve).
        struct FixingDecomp_ {
            double fixing_;
            double dFixing_dDs_;
            double dFixing_dDm_;
        };

        FixingDecomp_ DecomposeFixing(double Ds, double Dm, double yf) {
            if (yf <= 0.0)
                return {0.0, 0.0, 0.0};
            return {(Ds / Dm - 1.0) / yf, 1.0 / (yf * Dm), -Ds / (yf * Dm * Dm)};
        }

        // Accumulate a float leg into pv and per-anchor-date derivative map. tradeDate == anchor
        // (caller has already enforced), so DF(anchor, p) = D_p and its anchor-DF derivative is 1
        // on date p; the fixing contributes anchor-DF derivatives on s and m. Each period adds
        // fixing * dcf * DF(anchor, p) to pv, and the matching three contributions to dPvDanchor.
        void AccumulateFloatLeg(const DiscountCurve_& discount,
                                const Date_& tradeDate,
                                const Vector_<CouponPeriod_>& periods,
                                const DayBasis_& dayBasis,
                                double* pv,
                                std::map<Date_, double>* dPvDanchor) {
            for (const auto& period : periods) {
                const double dcf = period.accrual_.dcf_;
                const Date_& s = period.schedule_.accrualStart_;
                const Date_& m = period.schedule_.accrualEnd_;
                const Date_& p = period.schedule_.paymentDate_;
                const double yf = dayBasis(s, m, period.schedule_.dayCountContext_.get());
                const double Dm = discount(tradeDate, m);
                const double Ds = discount(tradeDate, s);
                const FixingDecomp_ f = DecomposeFixing(Ds, Dm, yf);
                const double dfTradeP = discount(tradeDate, p);
                *pv += f.fixing_ * dcf * dfTradeP;
                (*dPvDanchor)[s] += f.dFixing_dDs_ * dcf * dfTradeP;
                (*dPvDanchor)[m] += f.dFixing_dDm_ * dcf * dfTradeP;
                (*dPvDanchor)[p] += f.fixing_ * dcf; // dDF(anchor, p)/dD_p = 1 when trade=anchor
            }
        }

        // Accumulate a fixed-leg annuity: sum dcf * DF(anchor, p) into annuity and the matching
        // per-date dAnnuity/dD_p = dcf contributions into dAnnuityDanchor. tradeDate == anchor
        // is assumed (caller-enforced).
        void AccumulateFixedAnnuity(const DiscountCurve_& discount,
                                    const Date_& tradeDate,
                                    const Vector_<CouponPeriod_>& periods,
                                    double* annuity,
                                    std::map<Date_, double>* dAnnuityDanchor) {
            for (const auto& period : periods) {
                const double dcf = period.accrual_.dcf_;
                const Date_& p = period.schedule_.paymentDate_;
                *annuity += dcf * discount(tradeDate, p);
                (*dAnnuityDanchor)[p] += dcf;
            }
        }

        // Quotient rule on rate = numer / denom. For each date that appears in either derivative
        // map, the weight is (dNumer * denom - numer * dDenom) / denom^2; drop exact zeros so the
        // returned vector stays sparse. The caller supplies the numerator-scaled derivative map
        // (e.g. for a swap, dNumer = dFloatPv; for a basis swap, dNumer = dReferencePv - dSpreadBasePv
        // precomputed into a single map).
        Vector_<pair<Date_, double>> QuotientDerivs(double numer,
                                                    double denom,
                                                    const std::map<Date_, double>& dNumerDanchor,
                                                    const std::map<Date_, double>& dDenomDanchor) {
            std::set<Date_> allDates;
            for (const auto& [d, _] : dNumerDanchor)
                allDates.insert(d);
            for (const auto& [d, _] : dDenomDanchor)
                allDates.insert(d);
            Vector_<pair<Date_, double>> retval;
            retval.reserve(allDates.size());
            const double denomSq = denom * denom;
            for (const auto& d : allDates) {
                const double dN = dNumerDanchor.count(d) ? dNumerDanchor.at(d) : 0.0;
                const double dD = dDenomDanchor.count(d) ? dDenomDanchor.at(d) : 0.0;
                const double w = (dN * denom - numer * dD) / denomSq;
                if (w != 0.0)
                    retval.emplace_back(d, w);
            }
            return retval;
        }

        // Subtract one anchor-date derivative map from another, returning the per-date difference.
        // Used to fold a basis swap's (reference - spreadBase) numerator derivative into a single
        // map before the quotient rule.
        std::map<Date_, double> SubtractDerivMaps(const std::map<Date_, double>& lhs, const std::map<Date_, double>& rhs) {
            std::map<Date_, double> out = lhs;
            for (const auto& [d, v] : rhs)
                out[d] -= v;
            return out;
        }

        // Common guard for single-fixing instruments (Deposit, FRA, Future): the analytic
        // derivative engages only when the target slot is DISCOUNT and the forecast curve
        // resolves to the discount curve. Returns the resolved discount curve reference, or
        // nullptr if the guard fails (caller should return empty in that case).
        const DiscountCurve_* ResolveDiscountIfEligible(const YieldCurve_& yc,
                                                        const Handle_<YieldCurve_>& fallback,
                                                        YCInstrument_::Rate_::Target_ target,
                                                        const RateIndexConvention_& convention) {
            if (target != YCInstrument_::Rate_::Target_::DISCOUNT || convention.useProjectionCurve_)
                return nullptr;
            const DiscountCurve_& fc = ResolveForecastCurve(yc, fallback, convention);
            const DiscountCurve_& discount = ResolveDiscountCurve(yc, fallback, convention.collateral_);
            return (&fc == &discount) ? &discount : nullptr;
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

            // rate r = (1/DF(s,m) - 1)/yf, where DF(s,m) = D_m/D_s and D_t = DF(target, anchor, t).
            // Treating yf as constant: dr/dD_s = 1/(yf*D_m), dr/dD_m = -D_s/(yf*D_m^2).
            // Convexity adjustment is a constant (zero for deposits) and contributes no derivative.
            //
            // Only the calibrated TARGET curve's DFs appear; other curves in the YieldCurve_ are
            // treated as constant. The override supports only DISCOUNT-target calibrations where
            // the forecast curve resolves to the discount curve (useProjectionCurve_ == false).
            [[nodiscard]] Vector_<pair<Date_, double>>
            DRateDDiscount(const YieldCurve_& yc, Target_ target) const override {
                const DiscountCurve_* discount = ResolveDiscountIfEligible(yc, fallback_, target, convention_);
                if (discount == nullptr)
                    return {};
                Handle_<Date_> anchor = CurveAnchor(*discount);
                if (anchor.IsEmpty())
                    return {};
                const Date_ accrualStart = Holidays::Adjust(convention_.accrualHolidays_, start_, convention_.businessDayConvention_);
                const Date_ accrualEnd = Holidays::Adjust(convention_.accrualHolidays_, maturity_, convention_.businessDayConvention_);
                const auto ctx = SinglePeriodContext(start_, maturity_, CouponMonths(start_, maturity_));
                const double yf = convention_.dayBasis_(accrualStart, accrualEnd, ctx.get());
                if (yf <= 0.0)
                    return {};
                const double Ds = (*discount)(*anchor, accrualStart);
                const double Dm = (*discount)(*anchor, accrualEnd);
                const FixingDecomp_ f = DecomposeFixing(Ds, Dm, yf);
                Vector_<pair<Date_, double>> retval;
                retval.reserve(2);
                retval.emplace_back(accrualStart, f.dFixing_dDs_);
                retval.emplace_back(accrualEnd, f.dFixing_dDm_);
                return retval;
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

            // Same shape as DepositRate_ -- the convexity adjustment is constant.
            [[nodiscard]] Vector_<pair<Date_, double>>
            DRateDDiscount(const YieldCurve_& yc, Target_ target) const override {
                const DiscountCurve_* discount = ResolveDiscountIfEligible(yc, fallback_, target, convention_);
                if (discount == nullptr)
                    return {};
                Handle_<Date_> anchor = CurveAnchor(*discount);
                if (anchor.IsEmpty())
                    return {};
                const Date_ accrualStart = Holidays::Adjust(convention_.accrualHolidays_, start_, convention_.businessDayConvention_);
                const Date_ accrualEnd = Holidays::Adjust(convention_.accrualHolidays_, maturity_, convention_.businessDayConvention_);
                const auto ctx = SinglePeriodContext(start_,
                                                     maturity_,
                                                     convention_.useProjectionCurve_ ? convention_.forecastTenor_.Months() : CouponMonths(start_, maturity_));
                const double yf = convention_.dayBasis_(accrualStart, accrualEnd, ctx.get());
                if (yf <= 0.0)
                    return {};
                const double Ds = (*discount)(*anchor, accrualStart);
                const double Dm = (*discount)(*anchor, accrualEnd);
                const FixingDecomp_ f = DecomposeFixing(Ds, Dm, yf);
                Vector_<pair<Date_, double>> retval;
                retval.reserve(2);
                retval.emplace_back(accrualStart, f.dFixing_dDs_);
                retval.emplace_back(accrualEnd, f.dFixing_dDm_);
                return retval;
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

            // Quotient rule: rate = floatPv / annuity.
            // dRate/dDF(target, t) = (dFloatPv/dDF(t) * annuity - floatPv * dAnnuity/dDF(t)) / annuity^2.
            // The fixed leg contributes annuity only; each payment p has dAnnuity/dDF(target, anchor, p) =
            // dcf * dDF(target, trade, p)/dDF(target, anchor, p) = dcf / DF(target, anchor, trade).
            // When tradeDate == anchor, DF(target, anchor, trade) = 1 and the contribution simplifies.
            // The float leg's fixings carry fixing_k = (D_s_k/D_m_k - 1)/yf_k which differentiates to
            // two anchor-DF terms per fixing (accrualStart, accrualEnd), plus the payment-date DF.
            //
            // Only the calibrated TARGET curve's DFs appear; other curves are constant.
            [[nodiscard]] Vector_<pair<Date_, double>>
            DRateDDiscount(const YieldCurve_& yc, Target_ target) const override {
                if (target != Target_::DISCOUNT || floatIndexConvention_.useProjectionCurve_)
                    return {};
                const DiscountCurve_& discount = ResolveDiscountCurve(yc, fallback_, floatIndexConvention_.collateral_);
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, floatIndexConvention_);
                if (&forecast != &discount)
                    return {};
                if (!TradeDateIsAnchor(discount, tradeDate_))
                    return {}; // CP1 supports only tradeDate == anchor

                std::map<Date_, double> dAnnuityDanchor;
                double annuity = 0.0;
                AccumulateFixedAnnuity(discount, tradeDate_, fixedPeriods_, &annuity, &dAnnuityDanchor);
                REQUIRE(annuity > 0.0, "Swap pricing requires positive fixed-leg annuity");

                std::map<Date_, double> dFloatDanchor;
                double floatPv = 0.0;
                AccumulateFloatLeg(discount,
                                   tradeDate_,
                                   floatPeriods_,
                                   floatIndexConvention_.dayBasis_,
                                   &floatPv,
                                   &dFloatDanchor);
                return QuotientDerivs(floatPv, annuity, dFloatDanchor, dAnnuityDanchor);
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

            // Quotient rule on rate = (referencePv - spreadBasePv) / spreadAnnuity.
            // Same shape as SwapRate_ but with three leg accumulations. The reference leg contributes
            // only to the numerator (referencePv); the spread leg contributes to both numerator
            // (spreadBasePv) and denominator (spreadAnnuity).
            [[nodiscard]] Vector_<pair<Date_, double>>
            DRateDDiscount(const YieldCurve_& yc, Target_ target) const override {
                if (target != Target_::DISCOUNT || spreadIndexConvention_.useProjectionCurve_ || referenceIndexConvention_.useProjectionCurve_)
                    return {};
                const DiscountCurve_& discount = ResolveDiscountCurve(yc, fallback_, spreadIndexConvention_.collateral_);
                const DiscountCurve_& spreadForecast = ResolveForecastCurve(yc, fallback_, spreadIndexConvention_);
                const DiscountCurve_& referenceForecast = ResolveForecastCurve(yc, fallback_, referenceIndexConvention_);
                if (&spreadForecast != &discount || &referenceForecast != &discount)
                    return {};
                if (!TradeDateIsAnchor(discount, tradeDate_))
                    return {};

                std::map<Date_, double> dSpreadBaseDanchor, dSpreadAnnuityDanchor, dReferenceDanchor;
                double spreadBasePv = 0.0, spreadAnnuity = 0.0, referencePv = 0.0;
                // Spread leg: contributes to spreadBasePv (numerator) AND spreadAnnuity (denominator).
                AccumulateFloatLeg(discount,
                                   tradeDate_,
                                   spreadPeriods_,
                                   spreadIndexConvention_.dayBasis_,
                                   &spreadBasePv,
                                   &dSpreadBaseDanchor);
                AccumulateFixedAnnuity(discount, tradeDate_, spreadPeriods_, &spreadAnnuity, &dSpreadAnnuityDanchor);
                REQUIRE(spreadAnnuity > 0.0, "Basis swap pricing requires positive spread-leg annuity");
                // Reference leg: contributes only to referencePv (numerator). No annuity contribution.
                AccumulateFloatLeg(discount,
                                   tradeDate_,
                                   referencePeriods_,
                                   referenceIndexConvention_.dayBasis_,
                                   &referencePv,
                                   &dReferenceDanchor);
                // numerator = referencePv - spreadBasePv; denominator = spreadAnnuity.
                const double numer = referencePv - spreadBasePv;
                const std::map<Date_, double> dNumerDanchor = SubtractDerivMaps(dReferenceDanchor, dSpreadBaseDanchor);
                return QuotientDerivs(numer, spreadAnnuity, dNumerDanchor, dSpreadAnnuityDanchor);
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
