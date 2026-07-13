//
// Created by wegamekinglc on 2026/6/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/xccycalibration.hpp>

namespace Dal {

    CrossCurrencyCalibrationSpec_ CrossCurrencyCalibrationSpecBuilder_::Build() const {
        CrossCurrencyCalibrationSpec_ result;
        result.today_ = today_;
        result.valuationTime_ = valuationTime_;
        result.collateralCurrency_ = collateralCurrency_;
        result.fixings_ = fixings_;
        result.basisPair_ = basisPair_;
        result.domesticCurveBlock_ = domesticCurveBlock_;
        result.foreignCurveBlock_ = foreignCurveBlock_;
        result.fxSpot_ = fxSpot_;
        result.fxForwardCollateral_ = fxForwardCollateral_;
        result.instruments_ = instruments_;
        result.knotDates_ = knotDates_;
        result.smoothingWeight_ = smoothingWeight_;
        result.tolerance_ = tolerance_;
        result.fitTolerance_ = fitTolerance_;
        result.initialGuess_ = initialGuess_;
        result.maxEvaluations_ = maxEvaluations_;
        result.maxRestarts_ = maxRestarts_;
        result.solveMode_ = solveMode_;
        return result;
    }

    JointXccyCalibrationSpec_ JointXccyCalibrationSpecBuilder_::Build() const {
        JointXccyCalibrationSpec_ result;
        result.valuationTime_ = valuationTime_;
        result.pair_ = pair_;
        result.collateralCurrency_ = collateralCurrency_;
        result.fxSpot_ = fxSpot_;
        result.domestic_ = domestic_;
        result.foreign_ = foreign_;
        result.basis_ = basis_;
        result.fixings_ = fixings_;
        result.tolerance_ = solverOptions_.tolerance_;
        result.fitTolerance_ = solverOptions_.fitTolerance_;
        result.initialGuess_ = solverOptions_.initialGuess_;
        result.maxEvaluations_ = solverOptions_.maxEvaluations_;
        result.maxRestarts_ = solverOptions_.maxRestarts_;
        result.solveMode_ = solverOptions_.solveMode_;
        return result;
    }

    const Handle_<CurveBlock_>& JointXccyResultDomesticBlock(const JointXccyCalibrationResult_& result) { return result.domesticCurveBlock_; }

    const Handle_<CurveBlock_>& JointXccyResultForeignBlock(const JointXccyCalibrationResult_& result) { return result.foreignCurveBlock_; }

    const Handle_<DiscountCurve_>& JointXccyResultBasisCurve(const JointXccyCalibrationResult_& result) { return result.basisCurve_; }

    const CrossCurrencyFxForwardCurve_& JointXccyResultFxForwards(const JointXccyCalibrationResult_& result) { return result.fxForwardCurve_; }

    const Vector_<>& JointXccyResultMarketRates(const JointXccyCalibrationResult_& result) { return result.marketRates_; }

    const Vector_<>& JointXccyResultModelRates(const JointXccyCalibrationResult_& result) { return result.modelRates_; }

    const Vector_<>& JointXccyResultResiduals(const JointXccyCalibrationResult_& result) { return result.residuals_; }

    const Matrix_<>& JointXccyResultJacobian(const JointXccyCalibrationResult_& result) { return result.jacobianAtSolution_; }

    const Vector_<CalibrationBlockRange_>& JointXccyResultParameterRanges(const JointXccyCalibrationResult_& result) {
        return result.parameterRanges_;
    }

    const Vector_<CalibrationBlockRange_>& JointXccyResultResidualRanges(const JointXccyCalibrationResult_& result) { return result.residualRanges_; }

    CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec) { return CalibrateCrossCurrencyMarket(spec); }

} // namespace Dal
