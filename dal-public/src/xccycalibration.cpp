//
// Created by wegamekinglc on 2026/6/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/xccycalibration.hpp>

namespace Dal {

    CrossCurrencyCalibrationSpec_ CrossCurrencyCalibrationSpecBuilder_::Build() const {
        CrossCurrencyCalibrationSpec_ spec;
        spec.today_ = today_;
        spec.basisPair_ = basisPair_;
        spec.domesticCurveBlock_ = domesticCurveBlock_;
        spec.foreignCurveBlock_ = foreignCurveBlock_;
        spec.fxSpot_ = fxSpot_;
        spec.fxForwardCollateral_ = fxForwardCollateral_;
        spec.instruments_ = instruments_;
        spec.knotDates_ = knotDates_;
        spec.smoothingWeight_ = smoothingWeight_;
        spec.tolerance_ = tolerance_;
        spec.fitTolerance_ = fitTolerance_;
        spec.initialGuess_ = initialGuess_;
        spec.maxEvaluations_ = maxEvaluations_;
        spec.maxRestarts_ = maxRestarts_;
        spec.solveMode_ = solveMode_;
        return spec;
    }

    CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        return CalibrateCrossCurrencyMarket(spec);
    }

} // namespace Dal
