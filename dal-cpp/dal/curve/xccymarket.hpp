//
// Created by GitHub Copilot on 2026/6/6.
//

#pragma once

#include <map>
#include <dal/platform/platform.hpp>
#include <dal/currency/currency.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/math/matrix/matrixs.hpp>
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
        Ccy_ domesticCcy_;
        Ccy_ foreignCcy_;
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        double fxSpot_ = 0.0;
        Handle_<DiscountCurve_> basisCurve_;

    public:
        CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock,
                             const Handle_<CurveBlock_>& foreignBlock,
                             double fxSpot);
        void SetBasisCurve(const Handle_<DiscountCurve_>& basisCurve);
        [[nodiscard]] Date_ Today() const;
        [[nodiscard]] const Ccy_& DomesticCcy() const { return domesticCcy_; }
        [[nodiscard]] const Ccy_& ForeignCcy() const { return foreignCcy_; }
        [[nodiscard]] const CurveBlock_& DomesticBlock() const { return *domesticBlock_; }
        [[nodiscard]] const CurveBlock_& ForeignBlock() const { return *foreignBlock_; }
        [[nodiscard]] const DiscountCurve_& DomesticDiscountCurve(const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForeignDiscountCurve(const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& DomesticForwardCurve(const PeriodLength_& tenor,
                                                                 const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForeignForwardCurve(const PeriodLength_& tenor,
                                                                const CollateralType_& collateral) const;
        [[nodiscard]] double FxSpot() const { return fxSpot_; }
        [[nodiscard]] double BasisDiscountFactor(const Date_& from, const Date_& to) const;
        [[nodiscard]] double FxForward(const Date_& maturity) const;
        [[nodiscard]] double FxForward(const Date_& from, const Date_& maturity, const CollateralType_& collateral) const;
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
        CrossCurrencyConvention_ convention_;

    public:
        CrossCurrencySwap_(const Date_& tradeDate,
                           const Date_& start,
                           const Date_& maturity,
                           double marketRate,
                           const CurrencyPair_& pair,
                           double domesticNotional,
                           double foreignNotional,
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
        Matrix_<> effJacobianInverse_;
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
        bool usedApproximateFit_ = false;
    };

    struct CrossCurrencyFxForwardCurve_ {
        CurrencyPair_ pair_;
        Vector_<Date_> dates_;
        Vector_<> forwards_;
    };

    struct CrossCurrencyCalibrationSpec_ {
        Date_ today_;
        CurrencyPair_ basisPair_;
        Handle_<CurveBlock_> domesticCurveBlock_;
        Handle_<CurveBlock_> foreignCurveBlock_;
        double fxSpot_ = 0.0;
        CollateralType_ fxForwardCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        Vector_<Handle_<CrossCurrencySwap_>> instruments_;
        Vector_<Date_> knotDates_;
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-10;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.0;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    struct CrossCurrencyCalibrationResult_ {
        CrossCurrencyMarket_ market_;
        std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves_;
        CrossCurrencyFxForwardCurve_ fxForwardCurve_;
        CrossCurrencyCalibrationDiagnostics_ diagnostics_;

        CrossCurrencyCalibrationResult_(const CrossCurrencyMarket_& market,
                                        const std::map<CurrencyPair_, Handle_<DiscountCurve_>>& basisCurves,
                                        const CrossCurrencyFxForwardCurve_& fxForwardCurve,
                                        const CrossCurrencyCalibrationDiagnostics_& diagnostics)
            : market_(market), basisCurves_(basisCurves), fxForwardCurve_(fxForwardCurve), diagnostics_(diagnostics) {}
    };

    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec);
} // namespace Dal
