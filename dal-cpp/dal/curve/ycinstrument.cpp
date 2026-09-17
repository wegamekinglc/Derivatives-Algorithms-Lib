//
// Created by wegam on 2026/4/19.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {

    namespace {
        PeriodLength_ PeriodFromMonths(int months) {
            REQUIRE(months == 1 || months == 3 || months == 6 || months == 12, "Unsupported calibration instrument frequency");
            return PeriodLength_(String::FromInt(months) + "M");
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

        struct CouponPeriod_ {
            SchedulePeriod_ schedule_;
            AccrualPeriod_ accrual_;
        };

        // Fixed/float coupon-period pair shared by the three Swap_ factories.
        pair<Vector_<CouponPeriod_>, Vector_<CouponPeriod_>> BuildFixedAndFloatPeriods(const Date_& start,
                                                                                       const Date_& maturity,
                                                                                       const RateLegConvention_& fixedLegConvention,
                                                                                       const RateLegConvention_& floatLegConvention,
                                                                                       const RateIndexConvention_& floatIndexConvention) {
            return {BuildLegPeriods<CouponPeriod_>(start, maturity, fixedLegConvention, 0, Holidays::None()),
                    BuildLegPeriods<CouponPeriod_>(start,
                                                   maturity,
                                                   floatLegConvention,
                                                   floatIndexConvention.fixingLag_,
                                                   floatIndexConvention.fixingHolidays_)};
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
                const SchedulePeriod_ period = BuildSinglePeriodSchedule(start_, maturity_, convention_, CouponMonths(start_, maturity_));
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, convention_);
                return Tape::DepositRateFromCurves(forecast, period, convention_.dayBasis_);
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
                const SchedulePeriod_ period =
                    BuildSinglePeriodSchedule(start_, maturity_, convention_, SinglePeriodCouponMonths(convention_, start_, maturity_));
                const DiscountCurve_& forecast = ResolveForecastCurve(yc, fallback_, convention_);
                return Tape::ForwardRateFromCurves(forecast, period, convention_.dayBasis_, convexityAdjustment_);
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
                return Tape::SwapRateFromCurves(discount,
                                                forecast,
                                                tradeDate_,
                                                fixedPeriods_,
                                                floatPeriods_,
                                                floatIndexConvention_.dayBasis_);
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
                    const double fixing = Tape::ForwardRate<double>(spreadForecast,
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
                    const double fixing = Tape::ForwardRate<double>(referenceForecast,
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

    namespace Tape {
        // Curve-source policies for the templated rates: where the forecast and discount curves come
        // from. The single-curve source reads forecast == discount == ctx.curve_ (Phase A, guaranteed
        // by EligibleForAnalyticJacobian); the joint-block source routes through the projection block.
        template <class T_>
        struct YCCtxSource_ {
            using ctx_t = YCCtx_<T_>;
            using base_t = Rate_<T_>;

            static const DiscountCurve_<T_>& Discount(const YCCtx_<T_>& ctx, const RateIndexConvention_&) { return ctx.curve_; }
            static const DiscountCurve_<T_>& Forecast(const YCCtx_<T_>& ctx, const RateIndexConvention_&) { return ctx.curve_; }
        };

        template <class T_>
        struct JointBlockSource_ {
            using ctx_t = JointCurveBlock_<T_>;
            using base_t = JointRate_<T_>;

            static const DiscountCurve_<T_>& Discount(const JointCurveBlock_<T_>& block, const RateIndexConvention_& convention) {
                return block.Discount(convention.collateral_);
            }
            static const DiscountCurve_<T_>& Forecast(const JointCurveBlock_<T_>& block, const RateIndexConvention_& convention) {
                return ForecastCurve(block, convention);
            }
        };

        template <class T_, class Src_>
        class DepositRate_ : public Src_::base_t {
            Date_ start_;
            Date_ maturity_;
            RateIndexConvention_ convention_;
        public:
            DepositRate_(const Date_& start, const Date_& maturity, const RateIndexConvention_& convention)
                : start_(start), maturity_(maturity), convention_(convention) {}

            T_ operator()(const typename Src_::ctx_t& ctx) const override {
                const SchedulePeriod_ period = BuildSinglePeriodSchedule(start_, maturity_, convention_, CouponMonths(start_, maturity_));
                return DepositRateFromCurves(Src_::Forecast(ctx, convention_), period, convention_.dayBasis_);
            }
        };

        template <class T_, class Src_>
        class ForwardRate_ : public Src_::base_t {
            Date_ start_;
            Date_ maturity_;
            double convexityAdjustment_;
            RateIndexConvention_ convention_;
        public:
            ForwardRate_(const Date_& start,
                          const Date_& maturity,
                          double convexityAdjustment,
                          const RateIndexConvention_& convention)
                : start_(start), maturity_(maturity), convexityAdjustment_(convexityAdjustment), convention_(convention) {}

            T_ operator()(const typename Src_::ctx_t& ctx) const override {
                const SchedulePeriod_ period =
                    BuildSinglePeriodSchedule(start_, maturity_, convention_, SinglePeriodCouponMonths(convention_, start_, maturity_));
                return ForwardRateFromCurves(Src_::Forecast(ctx, convention_), period, convention_.dayBasis_, convexityAdjustment_);
            }
        };

        template <class T_, class Src_>
        class SwapRate_ : public Src_::base_t {
            Date_ tradeDate_;
            Vector_<CouponPeriod_> fixedPeriods_;
            Vector_<CouponPeriod_> floatPeriods_;
            RateIndexConvention_ floatIndexConvention_;
        public:
            SwapRate_(const Date_& tradeDate,
                       const Vector_<CouponPeriod_>& fixedPeriods,
                       const Vector_<CouponPeriod_>& floatPeriods,
                       const RateIndexConvention_& floatIndexConvention)
                : tradeDate_(tradeDate),
                  fixedPeriods_(fixedPeriods),
                  floatPeriods_(floatPeriods),
                  floatIndexConvention_(floatIndexConvention) {}

            T_ operator()(const typename Src_::ctx_t& ctx) const override {
                return SwapRateFromCurves(Src_::Discount(ctx, floatIndexConvention_),
                                          Src_::Forecast(ctx, floatIndexConvention_),
                                          tradeDate_,
                                          fixedPeriods_,
                                          floatPeriods_,
                                          floatIndexConvention_.dayBasis_);
            }
        };

        template <class T_> using DepositRateProj_ = DepositRate_<T_, JointBlockSource_<T_>>;
        template <class T_> using ForwardRateProj_ = ForwardRate_<T_, JointBlockSource_<T_>>;
        template <class T_> using SwapRateProj_ = SwapRate_<T_, JointBlockSource_<T_>>;
    } // namespace Tape

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

    template <class T_> Handle_<Tape::Rate_<T_>> Deposit_::PrecomputeT() const {
        return Handle_<Tape::Rate_<T_>>(new Tape::DepositRate_<T_, Tape::YCCtxSource_<T_>>(start_, maturity_, convention_));
    }

    template <class T_> Handle_<Tape::JointRate_<T_>> Deposit_::PrecomputeProjectionT() const {
        return Handle_<Tape::JointRate_<T_>>(new Tape::DepositRateProj_<T_>(start_, maturity_, convention_));
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

    template <class T_> Handle_<Tape::Rate_<T_>> FRA_::PrecomputeT() const {
        return Handle_<Tape::Rate_<T_>>(new Tape::ForwardRate_<T_, Tape::YCCtxSource_<T_>>(start_, maturity_, 0.0, convention_));
    }

    template <class T_> Handle_<Tape::JointRate_<T_>> FRA_::PrecomputeProjectionT() const {
        return Handle_<Tape::JointRate_<T_>>(new Tape::ForwardRateProj_<T_>(start_, maturity_, 0.0, convention_));
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

    template <class T_> Handle_<Tape::Rate_<T_>> Future_::PrecomputeT() const {
        return Handle_<Tape::Rate_<T_>>(new Tape::ForwardRate_<T_, Tape::YCCtxSource_<T_>>(start_, maturity_, convexityAdjustment_, convention_));
    }

    template <class T_> Handle_<Tape::JointRate_<T_>> Future_::PrecomputeProjectionT() const {
        return Handle_<Tape::JointRate_<T_>>(new Tape::ForwardRateProj_<T_>(start_, maturity_, convexityAdjustment_, convention_));
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
        const auto [fixedPeriods, floatPeriods] =
            BuildFixedAndFloatPeriods(start_, maturity_, fixedLegConvention_, floatLegConvention_, floatIndexConvention_);
        return Handle_<Rate_>(new SwapRate_(tradeDate_, fixedPeriods, floatPeriods, floatIndexConvention_, funding_yc));
    }

    template <class T_> Handle_<Tape::Rate_<T_>> Swap_::PrecomputeT() const {
        const auto [fixedPeriods, floatPeriods] =
            BuildFixedAndFloatPeriods(start_, maturity_, fixedLegConvention_, floatLegConvention_, floatIndexConvention_);
        return Handle_<Tape::Rate_<T_>>(
            new Tape::SwapRate_<T_, Tape::YCCtxSource_<T_>>(tradeDate_, fixedPeriods, floatPeriods, floatIndexConvention_));
    }

    template <class T_> Handle_<Tape::JointRate_<T_>> Swap_::PrecomputeProjectionT() const {
        const auto [fixedPeriods, floatPeriods] =
            BuildFixedAndFloatPeriods(start_, maturity_, fixedLegConvention_, floatLegConvention_, floatIndexConvention_);
        return Handle_<Tape::JointRate_<T_>>(new Tape::SwapRateProj_<T_>(tradeDate_, fixedPeriods, floatPeriods, floatIndexConvention_));
    }

    template Handle_<Tape::Rate_<Dal::AAD::Number_>> Deposit_::PrecomputeT<Dal::AAD::Number_>() const;
    template Handle_<Tape::Rate_<Dal::AAD::Number_>> FRA_::PrecomputeT<Dal::AAD::Number_>() const;
    template Handle_<Tape::Rate_<Dal::AAD::Number_>> Future_::PrecomputeT<Dal::AAD::Number_>() const;
    template Handle_<Tape::Rate_<Dal::AAD::Number_>> Swap_::PrecomputeT<Dal::AAD::Number_>() const;

    template Handle_<Tape::JointRate_<Dal::AAD::Number_>> Deposit_::PrecomputeProjectionT<Dal::AAD::Number_>() const;
    template Handle_<Tape::JointRate_<Dal::AAD::Number_>> FRA_::PrecomputeProjectionT<Dal::AAD::Number_>() const;
    template Handle_<Tape::JointRate_<Dal::AAD::Number_>> Future_::PrecomputeProjectionT<Dal::AAD::Number_>() const;
    template Handle_<Tape::JointRate_<Dal::AAD::Number_>> Swap_::PrecomputeProjectionT<Dal::AAD::Number_>() const;

    // Double instantiation exists so cross-family characterization tests can price through the
    // templated families in plain double arithmetic.
    template Handle_<Tape::Rate_<double>> Deposit_::PrecomputeT<double>() const;
    template Handle_<Tape::Rate_<double>> FRA_::PrecomputeT<double>() const;
    template Handle_<Tape::Rate_<double>> Future_::PrecomputeT<double>() const;
    template Handle_<Tape::Rate_<double>> Swap_::PrecomputeT<double>() const;

    template Handle_<Tape::JointRate_<double>> Deposit_::PrecomputeProjectionT<double>() const;
    template Handle_<Tape::JointRate_<double>> FRA_::PrecomputeProjectionT<double>() const;
    template Handle_<Tape::JointRate_<double>> Future_::PrecomputeProjectionT<double>() const;
    template Handle_<Tape::JointRate_<double>> Swap_::PrecomputeProjectionT<double>() const;

    namespace Tape {
        template <class T_>
        Handle_<JointRate_<T_>> ProjectionRateAt(const YCInstrument_& inst) {
            return VisitRate(
                inst,
                [](const Deposit_& d) { return d.PrecomputeProjectionT<T_>(); },
                [](const FRA_& f) { return f.PrecomputeProjectionT<T_>(); },
                [](const Future_& fu) { return fu.PrecomputeProjectionT<T_>(); },
                [](const Swap_& s) { return s.PrecomputeProjectionT<T_>(); });
        }

        template Handle_<JointRate_<Dal::AAD::Number_>> ProjectionRateAt<Dal::AAD::Number_>(const YCInstrument_&);
    } // namespace Tape

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
        const auto spreadPeriods = BuildLegPeriods<CouponPeriod_>(start_,
                                                   maturity_,
                                                   spreadLegConvention_,
                                                   spreadIndexConvention_.fixingLag_,
                                                   spreadIndexConvention_.fixingHolidays_);
        const auto referencePeriods = BuildLegPeriods<CouponPeriod_>(start_,
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
