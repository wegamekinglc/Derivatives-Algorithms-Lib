//
// Created by wegamekinglc on 2026/6/20.
//

#pragma once

#include <dal/curve/xccycalibration.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {

    struct CrossCurrencyCalibrationSpecBuilder_ {
        Date_ today_;
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
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;

        [[nodiscard]] CrossCurrencyCalibrationSpec_ Build() const;
    };

    CrossCurrencyCalibrationResult_ CalibrateXccyMarket(const CrossCurrencyCalibrationSpec_& spec);

} // namespace Dal
