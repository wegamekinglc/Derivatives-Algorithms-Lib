//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    class YieldCurve_;
    class String_;
    class DiscountCurve_;

    // A single discount-factor query against a concrete (resolved) curve. The curve_ pointer
    // identifies which curve supplies the DF; callers map it to a scalar value (see EvalRatePlan).
    struct YCDiscountQuery_ {
        const DiscountCurve_* curve_ = nullptr;
        Date_ from_;
        Date_ to_;
    };

    // A simple forward rate: (1 / DF(from,to) - 1) / yearFraction.
    struct YCForwardTerm_ {
        YCDiscountQuery_ df_;
        double yearFraction_ = 0.0;
    };

    // A discounted coupon term: sign * [hasForward ? forward : 1] * accrualDcf * DF_discount(payDf).
    struct YCCouponTerm_ {
        bool hasForward_ = false;
        YCForwardTerm_ forward_;
        double accrualDcf_ = 0.0;
        YCDiscountQuery_ payDf_;
        double sign_ = 1.0;
    };

    // A fully-resolved, scalar-independent evaluation plan for an instrument's par rate.
    // All resolution (which curve), scheduling, day-count fractions and forward year-fractions
    // are baked in as doubles; only the discount factors are left abstract so the same plan can
    // be evaluated in double (production) or in an AAD scalar (calibration gradient).
    struct RatePlan_ {
        enum class Kind_ { SINGLE_FORWARD, RATIO };
        Kind_ kind_ = Kind_::RATIO;
        // SINGLE_FORWARD: rate = forward(single_) - convexityAdjustment_.
        YCForwardTerm_ single_;
        double convexityAdjustment_ = 0.0;
        // RATIO: rate = sum(numerator_) / sum(denominator_).
        Vector_<YCCouponTerm_> numerator_;
        Vector_<YCCouponTerm_> denominator_;
    };

    // Evaluate a RatePlan_ for scalar type T_. `df` maps a YCDiscountQuery_ to a T_-valued discount
    // factor. This is the single source of truth shared by the production double pricing path and the
    // AAD (Number_) calibration-gradient path, so the two can never diverge.
    template <class T_, class DfFn_>
    T_ EvalRatePlan(const RatePlan_& plan, DfFn_&& df) {
        auto forward = [&df](const YCForwardTerm_& f) -> T_ {
            T_ d = df(f.df_);
            return (1.0 / d - 1.0) / f.yearFraction_;
        };
        if (plan.kind_ == RatePlan_::Kind_::SINGLE_FORWARD)
            return forward(plan.single_) - plan.convexityAdjustment_;

        T_ numerator(0.0);
        for (const auto& term : plan.numerator_) {
            T_ coupon = term.hasForward_ ? forward(term.forward_) : T_(1.0);
            numerator = numerator + term.sign_ * coupon * term.accrualDcf_ * df(term.payDf_);
        }
        T_ denominator(0.0);
        for (const auto& term : plan.denominator_)
            denominator = denominator + term.accrualDcf_ * df(term.payDf_);
        return numerator / denominator;
    }

    class YCInstrument_ : noncopyable {
    public:
        virtual ~YCInstrument_() = default;
        [[nodiscard]] virtual String_ Name() const = 0;
        [[nodiscard]] virtual double MarketRate() const = 0;
        [[nodiscard]] virtual pair<Date_, Date_> TimeSpan() const = 0;

        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            // Resolve curves/schedules against `yc` and produce a scalar-independent evaluation plan.
            // The returned plan holds pointers into curves owned by `yc`; it must not outlive `yc`.
            [[nodiscard]] virtual RatePlan_ MakePlan(const YieldCurve_& yc) const = 0;
            // Production double pricing: evaluate the plan with plain discount-factor lookups.
            double operator()(const YieldCurve_& yc) const;
        };

        [[nodiscard]] virtual Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const = 0;
    };

    class Deposit_ : public YCInstrument_ {
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        RateIndexConvention_ convention_;
    public:
        Deposit_(const Date_& today, const Date_& maturity, double marketRate, const DayBasis_& basis);
        Deposit_(const Date_& tradeDate,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const RateIndexConvention_& convention);
        ~Deposit_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class FRA_ : public YCInstrument_ {
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        RateIndexConvention_ convention_;
    public:
        FRA_(const Date_& tradeDate,
             const Date_& start,
             const Date_& maturity,
             double marketRate,
             const RateIndexConvention_& convention);
        ~FRA_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class Future_ : public YCInstrument_ {
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        double convexityAdjustment_;
        RateIndexConvention_ convention_;
    public:
        Future_(const Date_& tradeDate,
                const Date_& start,
                const Date_& maturity,
                double marketRate,
                const RateIndexConvention_& convention,
                double convexityAdjustment = 0.0);
        ~Future_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class Swap_ : public YCInstrument_ {
    protected:
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        RateLegConvention_ fixedLegConvention_;
        RateIndexConvention_ floatIndexConvention_;
        RateLegConvention_ floatLegConvention_;
    public:
        Swap_(const Date_& today, const Date_& maturity, double marketRate, int freqMonths, const DayBasis_& basis);
        Swap_(const Date_& tradeDate,
              const Date_& start,
              const Date_& maturity,
              double marketRate,
              const RateLegConvention_& fixedLegConvention,
              const RateIndexConvention_& floatIndexConvention,
              const RateLegConvention_& floatLegConvention);
        ~Swap_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class OISSwap_ : public Swap_ {
    public:
        OISSwap_(const Date_& tradeDate,
                 const Date_& start,
                 const Date_& maturity,
                 double marketRate,
                 const RateLegConvention_& fixedLegConvention,
                 const RateIndexConvention_& overnightConvention,
                 const RateLegConvention_& floatLegConvention);
        ~OISSwap_() override;
        [[nodiscard]] String_ Name() const override;
    };

    class BasisSwap_ : public YCInstrument_ {
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        RateIndexConvention_ spreadIndexConvention_;
        RateLegConvention_ spreadLegConvention_;
        RateIndexConvention_ referenceIndexConvention_;
        RateLegConvention_ referenceLegConvention_;
    public:
        BasisSwap_(const Date_& tradeDate,
                   const Date_& start,
                   const Date_& maturity,
                   double marketRate,
                   const RateIndexConvention_& spreadIndexConvention,
                   const RateLegConvention_& spreadLegConvention,
                   const RateIndexConvention_& referenceIndexConvention,
                   const RateLegConvention_& referenceLegConvention);
        ~BasisSwap_() override;
        [[nodiscard]] String_ Name() const override;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const override;
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
    };

    class STIR_ : public FRA_ {
    public:
        STIR_(const Date_& today,
              const Date_& start,
              const Date_& maturity,
              double marketRate,
              const DayBasis_& basis);
        STIR_(const Date_& tradeDate,
              const Date_& start,
              const Date_& maturity,
              double marketRate,
              const RateIndexConvention_& convention);
        ~STIR_() override;
        [[nodiscard]] String_ Name() const override;
    };
} // namespace Dal
