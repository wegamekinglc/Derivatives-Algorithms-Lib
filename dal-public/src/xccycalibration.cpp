//
// Created by wegamekinglc on 2026/6/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal-public/src/xccycalibration.hpp>

namespace Dal {

    CrossCurrencyCalibrationSpec_ CrossCurrencyCalibrationSpecBuilder_::Build() const {
        return CrossCurrencyCalibrationSpec_{
            today_, basisPair_, domesticCurveBlock_, foreignCurveBlock_,
            fxSpot_, fxForwardCollateral_, instruments_, knotDates_,
            smoothingWeight_, tolerance_, fitTolerance_, initialGuess_,
            maxEvaluations_, maxRestarts_, solveMode_
        };
    }

    CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec) {
        return CalibrateCrossCurrencyMarket(spec);
    }

} // namespace Dal
