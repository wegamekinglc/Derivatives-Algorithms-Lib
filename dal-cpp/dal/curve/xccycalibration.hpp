//
// Created by GitHub Copilot on 2026/6/6.
//

#pragma once

#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/time/datetime.hpp>
#include <map>

namespace Dal {
    namespace Tape {
        template <class T_> class DiscountCurve_;
    }
    using DiscountCurve_ = Tape::DiscountCurve_<double>;

    class CrossCurrencyMarket_ {
        Ccy_ domesticCcy_;
        Ccy_ foreignCcy_;
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        double fxSpot_ = 0.0;
        Handle_<DiscountCurve_> basisCurve_;
        DateTime_ valuationTime_;
        Ccy_ collateralCurrency_;
        Handle_<MarketFixingSnapshot_> fixings_;

    public:
        CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock, const Handle_<CurveBlock_>& foreignBlock, double fxSpot);
        CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock,
                             const Handle_<CurveBlock_>& foreignBlock,
                             double fxSpot,
                             const DateTime_& valuationTime,
                             const Ccy_& collateralCurrency,
                             const Handle_<MarketFixingSnapshot_>& fixings = Handle_<MarketFixingSnapshot_>());
        void SetBasisCurve(const Handle_<DiscountCurve_>& basisCurve);
        [[nodiscard]] Date_ Today() const { return valuationTime_.Date(); }
        [[nodiscard]] const DateTime_& ValuationTime() const { return valuationTime_; }
        [[nodiscard]] const Ccy_& CollateralCurrency() const { return collateralCurrency_; }
        [[nodiscard]] const Handle_<MarketFixingSnapshot_>& Fixings() const { return fixings_; }
        [[nodiscard]] const Ccy_& DomesticCcy() const { return domesticCcy_; }
        [[nodiscard]] const Ccy_& ForeignCcy() const { return foreignCcy_; }
        [[nodiscard]] const CurveBlock_& DomesticBlock() const { return *domesticBlock_; }
        [[nodiscard]] const CurveBlock_& ForeignBlock() const { return *foreignBlock_; }
        [[nodiscard]] const DiscountCurve_& DomesticDiscountCurve(const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForeignDiscountCurve(const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& DomesticForwardCurve(const PeriodLength_& tenor, const CollateralType_& collateral) const;
        [[nodiscard]] const DiscountCurve_& ForeignForwardCurve(const PeriodLength_& tenor, const CollateralType_& collateral) const;
        [[nodiscard]] double FxSpot() const { return fxSpot_; }
        [[nodiscard]] const DiscountCurve_* BasisCurve() const { return basisCurve_.get(); }
        [[nodiscard]] double BasisDiscountFactor(const Date_& from, const Date_& to) const;
        [[nodiscard]] double FxForward(const Date_& maturity) const;
        [[nodiscard]] double FxForward(const Date_& from, const Date_& maturity, const CollateralType_& collateral) const;
    };

    struct CrossCurrencyCalibrationDiagnostics_ {
        Vector_<String_> instrumentNames_;
        Vector_<Date_> parameterKnotDates_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        Matrix_<> effJacobianInverse_;
        // Unscaled at-solution forward Jacobian; empty for bumped or approximate solves.
        Matrix_<> jacobian_;
        double residualTolerance_ = 0.0;
        String_ jacobianScaling_;
        String_ effJacobianInverseScaling_;
        String_ jacobianAvailability_;
        String_ effJacobianInverseAvailability_;
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
        DateTime_ valuationTime_;
        Ccy_ collateralCurrency_;
        Handle_<MarketFixingSnapshot_> fixings_;
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
        Vector_<double> initialGuessPerNode_;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
    };

    struct CrossCurrencyCalibrationOptions_ {
        CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
        bool computeEffJacobianInverse_ = true;
        bool computeForwardJacobian_ = true;
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
    CrossCurrencyCalibrationResult_ CalibrateCrossCurrencyMarket(const CrossCurrencyCalibrationSpec_& spec,
                                                                 const CrossCurrencyCalibrationOptions_& options);
    [[nodiscard]] AnalyticEligibilityReport_ ValidateCrossCurrencyAnalyticEligibility(const CrossCurrencyCalibrationSpec_& spec);
} // namespace Dal
