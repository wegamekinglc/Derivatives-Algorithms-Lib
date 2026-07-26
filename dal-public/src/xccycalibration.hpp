//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/platform/platform.hpp>

#include <dal-public/src/curvespec.hpp>

namespace Dal {

    struct CrossCurrencyCalibrationSpecBuilder_ {
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
        // keep in sync with CurveSolverOptions_ in curvespec.hpp
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-10;
        double fitTolerance_ = 1.0e-6;
        double initialGuess_ = 0.0;
        Vector_<double> initialGuessPerNode_;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;

        [[nodiscard]] CrossCurrencyCalibrationSpec_ Build() const;
    };

    struct JointXccyCalibrationSpecBuilder_ {
        DateTime_ valuationTime_;
        CurrencyPair_ pair_;
        Ccy_ collateralCurrency_;
        double fxSpot_ = 0.0;
        JointCurrencyCurveSpec_ domestic_;
        JointCurrencyCurveSpec_ foreign_;
        XccyBasisCurveDeclaration_ basis_;
        Handle_<MarketFixingSnapshot_> fixings_;
        CurveSolverOptions_ solverOptions_;

        JointXccyCalibrationSpecBuilder_() { solverOptions_.initialGuess_ = 0.0; }
        [[nodiscard]] JointXccyCalibrationSpec_ Build() const;
    };

    [[nodiscard]] const Handle_<CurveBlock_>& JointXccyResultDomesticBlock(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Handle_<CurveBlock_>& JointXccyResultForeignBlock(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Handle_<DiscountCurve_>& JointXccyResultBasisCurve(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const CrossCurrencyFxForwardCurve_& JointXccyResultFxForwards(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Vector_<>& JointXccyResultMarketRates(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Vector_<>& JointXccyResultModelRates(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Vector_<>& JointXccyResultResiduals(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Matrix_<>& JointXccyResultJacobian(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Matrix_<>& JointXccyResultEffJacobianInverse(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Vector_<CalibrationBlockRange_>& JointXccyResultParameterRanges(const JointXccyCalibrationResult_& result);
    [[nodiscard]] const Vector_<CalibrationBlockRange_>& JointXccyResultResidualRanges(const JointXccyCalibrationResult_& result);

    [[nodiscard]] const CrossCurrencyCalibrationDiagnostics_& XccyResultDiagnostics(const CrossCurrencyCalibrationResult_& result);
    [[nodiscard]] const Matrix_<>& XccyResultJacobian(const CrossCurrencyCalibrationResult_& result);
    [[nodiscard]] const Matrix_<>& XccyResultEffJacobianInverse(const CrossCurrencyCalibrationResult_& result);
    [[nodiscard]] const Handle_<DiscountCurve_>& XccyResultBasisCurve(const CrossCurrencyCalibrationResult_& result);

    [[nodiscard]] CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec);
    [[nodiscard]] CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec,
                                                                      const CrossCurrencyCalibrationOptions_& options);

} // namespace Dal
