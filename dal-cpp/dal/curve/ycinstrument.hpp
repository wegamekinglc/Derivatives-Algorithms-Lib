//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <utility>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    class YieldCurve_;
    class String_;

    class YCInstrument_ : noncopyable {
    public:
        virtual ~YCInstrument_() = default;
        [[nodiscard]] virtual String_ Name() const = 0;
        [[nodiscard]] virtual double MarketRate() const = 0;
        [[nodiscard]] virtual pair<Date_, Date_> TimeSpan() const = 0;

        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual double operator()(const YieldCurve_& yc) const = 0;

            // For each discount factor DF(anchor, paymentDate) the rate reads from the calibrated
            // curve, the derivative of the model rate w.r.t. that DF, evaluated on the supplied
            // yield curve. Returns (paymentDate, dRate/dDF(anchor, paymentDate)) pairs. Multiple
            // entries for the same date may be returned (e.g. an annuity coupon and a float fixing
            // on the same payment date); the Jacobian assembler SUMS by date during accumulation.
            //
            // `target` selects which curve slot is the calibrated target. CP1 supports only the
            // DISCOUNT-slot target -- discount-curve calibrations. Concrete overrides compute the
            // derivative w.r.t. that target curve's DFs; DFs from other curves in the YieldCurve_
            // are treated as constant. When the rate's forecast curve resolves to the discount
            // curve (useProjectionCurve_ == false, the PTIRDS / vanilla-swap convention), the
            // fixing-date DFs appear naturally because ResolveForecastCurve falls back to the
            // discount curve.
            //
            // Default returns empty -- the rate opts out of the analytic Jacobian and the
            // calibration silently falls back to per-instrument DF-bumping for that row.
            enum class Target_ { DISCOUNT, FORECAST };
            [[nodiscard]] virtual Vector_<pair<Date_, double>> DRateDDiscount(const YieldCurve_& yc, Target_ target) const { return {}; }
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
