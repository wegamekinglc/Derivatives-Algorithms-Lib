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

#include <dal/curve/ycctx.hpp>

namespace Dal {
    class YieldCurve_;
    class String_;

    namespace Tape {
        // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
        template <class T_> struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual T_ operator()(const YCCtx_<T_>& ctx) const = 0;
        };

        // Forward-declared so the instrument classes below can declare PrecomputeProjectionT<T_>
        // member templates. Defined in jointrate.hpp (which includes this header).
        template <class T_> struct JointRate_;
        template <class T_> struct JointCurveBlock_;
    } // namespace Tape

    class YCInstrument_ : noncopyable {
    public:
        virtual ~YCInstrument_() = default;
        [[nodiscard]] virtual String_ Name() const = 0;
        [[nodiscard]] virtual double MarketRate() const = 0;
        [[nodiscard]] virtual pair<Date_, Date_> TimeSpan() const = 0;
        // The real trade date. Distinct from TimeSpan().first (the effective/spot start): a
        // spot-started instrument has tradeDate == spotLag business days before start_. Phase A's
        // templated rates read DF(tradeDate_, p), so eligibility must be checked against this
        // accessor, not the start proxy that silently misprices spot-started residual rows.
        [[nodiscard]] virtual Date_ TradeDate() const = 0;

        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual double operator()(const YieldCurve_& yc) const = 0;
        };

        [[nodiscard]] virtual Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const = 0;

        // See docs/methodology/yield_curve_jacobian.md §Joint Multi-Curve Analytic Jacobian.
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
        [[nodiscard]] Date_ TradeDate() const override { return tradeDate_; }
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
        template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PrecomputeT() const;
        template <class T_> [[nodiscard]] Handle_<Tape::JointRate_<T_>> PrecomputeProjectionT() const;
        [[nodiscard]] const RateIndexConvention_& FloatConvention() const { return convention_; }
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
        [[nodiscard]] Date_ TradeDate() const override { return tradeDate_; }
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
        template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PrecomputeT() const;
        template <class T_> [[nodiscard]] Handle_<Tape::JointRate_<T_>> PrecomputeProjectionT() const;
        [[nodiscard]] const RateIndexConvention_& FloatConvention() const { return convention_; }
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
        [[nodiscard]] Date_ TradeDate() const override { return tradeDate_; }
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
        template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PrecomputeT() const;
        template <class T_> [[nodiscard]] Handle_<Tape::JointRate_<T_>> PrecomputeProjectionT() const;
        [[nodiscard]] const RateIndexConvention_& FloatConvention() const { return convention_; }
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
        [[nodiscard]] Date_ TradeDate() const override { return tradeDate_; }
        [[nodiscard]] double MarketRate() const override { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute(const Handle_<YieldCurve_>& funding_yc) const override;
        template <class T_> [[nodiscard]] Handle_<Tape::Rate_<T_>> PrecomputeT() const;
        template <class T_> [[nodiscard]] Handle_<Tape::JointRate_<T_>> PrecomputeProjectionT() const;
        [[nodiscard]] const RateIndexConvention_& FloatConvention() const { return floatIndexConvention_; }
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
        [[nodiscard]] Date_ TradeDate() const override { return tradeDate_; }
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
