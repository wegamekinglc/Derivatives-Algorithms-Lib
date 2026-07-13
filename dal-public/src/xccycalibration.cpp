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

    CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec) { return CalibrateCrossCurrencyMarket(spec); }

} // namespace Dal
