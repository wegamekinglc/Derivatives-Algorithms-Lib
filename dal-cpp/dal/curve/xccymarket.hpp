//
// Created by GitHub Copilot on 2026/6/6.
//

#pragma once

#include <map>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>

namespace Dal {
    class DiscountCurve_;

    struct CurrencyPair_ {
        Ccy_ domestic_;
        Ccy_ foreign_;

        CurrencyPair_();
        CurrencyPair_(const Ccy_& domestic, const Ccy_& foreign);
        [[nodiscard]] CurrencyPair_ Reversed() const;
        [[nodiscard]] bool operator<(const CurrencyPair_& rhs) const;
        [[nodiscard]] bool operator==(const CurrencyPair_& rhs) const;
    };

    class CrossCurrencyMarket_ {
        std::map<Ccy_, Handle_<CurveBlock_>> curveBlocks_;
        std::map<CurrencyPair_, double> fxSpots_;
        std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves_;

    public:
        CrossCurrencyMarket_() = default;
        [[nodiscard]] Date_ Today() const;
        void SetCurveBlock(const Ccy_& ccy, const Handle_<CurveBlock_>& curveBlock);
        [[nodiscard]] const CurveBlock_& CurveBlock(const Ccy_& ccy) const;
        [[nodiscard]] const DiscountCurve_& DomesticDiscountCurve(const CurrencyPair_& pair,
                                                                  const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForeignDiscountCurve(const CurrencyPair_& pair,
                                                                 const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForwardCurve(const Ccy_& ccy,
                                                         const PeriodLength_& tenor,
                                                         const CollateralType_& collateral) const;
        void SetFxSpot(const CurrencyPair_& pair, double spot);
        void SetBasisCurve(const CurrencyPair_& pair, const Handle_<DiscountCurve_>& basisCurve);
        [[nodiscard]] double FxSpot(const CurrencyPair_& pair) const;
        [[nodiscard]] double BasisDiscountFactor(const CurrencyPair_& pair, const Date_& from, const Date_& to) const;
        [[nodiscard]] double FxForward(const CurrencyPair_& pair, const Date_& maturity) const;
        [[nodiscard]] double FxForward(const CurrencyPair_& pair,
                                       const Date_& from,
                                       const Date_& maturity,
                                       const CollateralType_& collateral) const;
    };

    class CrossCurrencySwap_ {
    public:
        struct Rate_ : noncopyable {
            virtual ~Rate_() = default;
            virtual double operator()(const CrossCurrencyMarket_& market) const = 0;
        };

    private:
        Date_ tradeDate_;
        Date_ start_;
        Date_ maturity_;
        double marketRate_;
        CurrencyPair_ pair_;
        double domesticNotional_;
        double foreignNotional_;
        RateIndexConvention_ domesticIndexConvention_;
        RateLegConvention_ domesticLegConvention_;
        RateIndexConvention_ foreignIndexConvention_;
        RateLegConvention_ foreignLegConvention_;
        CrossCurrencyConvention_ convention_;

    public:
        CrossCurrencySwap_(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const CurrencyPair_& pair,
                           double domesticNotional,
                           double foreignNotional,
                           const RateIndexConvention_& domesticIndexConvention,
                           const RateLegConvention_& domesticLegConvention,
                           const RateIndexConvention_& foreignIndexConvention,
                           const RateLegConvention_& foreignLegConvention,
                           const CrossCurrencyConvention_& convention = CrossCurrencyConvention_());
        [[nodiscard]] String_ Name() const;
        [[nodiscard]] pair<Date_, Date_> TimeSpan() const;
        [[nodiscard]] double MarketRate() const { return marketRate_; }
        [[nodiscard]] Handle_<Rate_> Precompute() const;
    };

    struct CrossCurrencyCalibrationDiagnostics_ {
        Vector_<String_> instrumentNames_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        double maxAbsResidual_ = 0.0;
    };

    struct CrossCurrencyFxForwardCurve_ {
        CurrencyPair_ pair_;
        Vector_<Date_> dates_;
        Vector_<> forwards_;
    };

    struct CrossCurrencyCalibrationSpec_ {
        CurrencyPair_ basisPair_;
        Handle_<CurveBlock_> domesticCurveBlock_;
        Handle_<CurveBlock_> foreignCurveBlock_;
        double fxSpot_ = 0.0;
        CollateralType_ fxForwardCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        Vector_<Handle_<CrossCurrencySwap_>> instruments_;
        Vector_<Date_> knotDates_;
        double tolerance_ = 1.0e-10;
        int maxEvaluations_ = 100;
    };

    struct CrossCurrencyCalibrationResult_ {
        CrossCurrencyMarket_ market_;
        std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves_;
        CrossCurrencyFxForwardCurve_ fxForwardCurve_;
        CrossCurrencyCalibrationDiagnostics_ diagnostics_;
    };

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec);
} // namespace Dal
