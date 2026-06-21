//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal/utilities/dictionary.hpp>

/*IF--------------------------------------------------------------------------
public CalibrateSingleCurve
    Calibrate a single yield curve from instruments and settings.
    Returns the calibrated curve handle plus diagnostics (market rates, model rates, residuals, maxAbsResidual, rmsResidual)
&inputs
today is date
    The valuation/trade date
ccy is string
    Currency code (e.g. "USD")
instruments is handle[]
    Array of instrument handles (created with DA.DEPOSIT.NEW, DA.SWAP.NEW, etc.)
knotDates is date[]
    Knot dates (can be empty for auto-detection)
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional settings as a two-column range. Supported keys:
    curveName (string), calibrateDiscountCurve (boolean), smoothingWeight (number),
    tolerance (number), fitTolerance (number), maxEvaluations (integer), maxRestarts (integer),
    initialGuess (number), solveMode (string: EXACT|APPROXIMATE),
    parameterization (string: PIECEWISE_LINEAR_FWD|PIECEWISE_CONSTANT_FWD|ZERO_RATE|LOG_DISCOUNT),
    logDfScheme (string: LOG_LINEAR|LOG_CUBIC_NATURAL|MIXED),
    liborBasis (string), targetCollateral (string), targetTenor (string)
&outputs
curve is handle StorableDiscountCurve
    The calibrated discount curve
marketRates is number[]
    Market quoted rates for each instrument
modelRates is number[]
    Model-implied rates for each instrument
residuals is number[]
    Rate residuals (model - market)
maxAbsResidual is number
    Maximum absolute residual
rmsResidual is number
    Root-mean-square residual
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        // Helper: apply string-based settings
        void ApplyStringSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (key == "curveName")
                b.curveName_ = Cell::ToString(val);
            else if (key == "solveMode")
                b.solveMode_ = CurveSolveMode_(Cell::ToString(val));
            else if (key == "parameterization")
                b.parameterization_ = CurveParameterization_(Cell::ToString(val));
            else if (key == "logDfScheme")
                b.logDfScheme_ = LogDfScheme_(Cell::ToString(val));
            else if (key == "liborBasis")
                b.liborBasis_ = DayBasis_(Cell::ToString(val));
            else if (key == "targetCollateral")
                b.targetCollateral_ = CollateralType_(Cell::ToString(val));
            else if (key == "targetTenor")
                b.targetTenor_ = PeriodLength_(Cell::ToString(val));
        }

        // Helper: apply numeric/bool settings
        void ApplyNumericSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (key == "calibrateDiscountCurve") {
                if (Cell::IsBool(val))
                    b.calibrateDiscountCurve_ = Cell::ToBool(val);
            } else if (key == "smoothingWeight") {
                if (Cell::IsDouble(val))
                    b.smoothingWeight_ = Cell::ToDouble(val);
            } else if (key == "tolerance") {
                if (Cell::IsDouble(val))
                    b.tolerance_ = Cell::ToDouble(val);
            } else if (key == "fitTolerance") {
                if (Cell::IsDouble(val))
                    b.fitTolerance_ = Cell::ToDouble(val);
            } else if (key == "maxEvaluations") {
                if (Cell::IsInt(val))
                    b.maxEvaluations_ = Cell::ToInt(val);
            } else if (key == "maxRestarts") {
                if (Cell::IsInt(val))
                    b.maxRestarts_ = Cell::ToInt(val);
            } else if (key == "initialGuess") {
                if (Cell::IsDouble(val))
                    b.initialGuess_ = Cell::ToDouble(val);
            }
        }

        // Helper: apply dictionary settings to a CurveCalibrationSpecBuilder_
        void ApplySettings(const Dictionary_& settings, CurveCalibrationSpecBuilder_& b) {
            for (const auto& kv : settings) {
                const String_& key = kv.first;
                const Cell_& val = kv.second;
                ApplyStringSettings(key, val, b);
                ApplyNumericSettings(key, val, b);
            }
        }

        void CalibrateSingleCurve(const Date_& today,
                                   const String_& ccy,
                                   const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                   const Vector_<Date_>& knotDates,
                                   const Matrix_<Cell_>& settings,
                                   Handle_<StorableDiscountCurve_>* curve,
                                   Vector_<>* marketRates,
                                   Vector_<>* modelRates,
                                   Vector_<>* residuals,
                                   double* maxAbsResidual,
                                   double* rmsResidual) {
            CurveCalibrationSpecBuilder_ builder;
            builder.today_ = today;
            builder.ccy_ = ccy;

            // Convert instrument wrappers to raw handles
            for (const auto& w : instrumentWrappers) {
                REQUIRE(w, "Invalid instrument handle");
                auto ycInst = handle_cast<StorableYCInstrument_>(w);
                REQUIRE(ycInst, "Instrument must be a YC instrument");
                builder.instruments_.push_back(ycInst->val_);
            }

            // Set knot dates
            builder.knotDates_ = knotDates;

            // Convert Matrix_<Cell_> to Dictionary_ and apply optional settings
            if (!settings.Empty()) {
                Dictionary_ dict;
                for (int i = 0; i < settings.Rows(); ++i) {
                    if (Cell::IsEmpty(settings(i, 0)))
                        break;
                    dict.Insert(Cell::ToString(settings(i, 0)), settings(i, 1));
                }
                ApplySettings(dict, builder);
            }

            // Build spec and calibrate
            auto spec = builder.Build();
            auto result = Dal::CalibrateSingleCurve(spec);

            // Output curve
            curve->reset(new StorableDiscountCurve_(result.curve_));

            // Output diagnostics
            const auto& diag = result.diagnostics_;
            *marketRates = diag.marketRates_;
            *modelRates = diag.modelRates_;
            *residuals = diag.residuals_;
            *maxAbsResidual = diag.maxAbsResidual_;
            *rmsResidual = diag.rmsResidual_;
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrateSingleCurve_public.inc>
#endif
}
