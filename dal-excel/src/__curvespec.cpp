//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal/math/cell.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal/utilities/dictionary.hpp>

/*IF--------------------------------------------------------------------------
public Calibrate_SingleCurve
    Calibrate a single yield curve from instruments and settings.
    Returns a result handle bundling the calibrated curve and the fit diagnostics.
&inputs
today is date
    The valuation/trade date
ccy is string
    Currency code (e.g. "USD")
instruments is handle[]
    Array of instrument handles (created with DEPOSIT.NEW, SWAP.NEW, etc.)
knotDates is date[]
    Knot dates (can be empty for auto-detection)
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional (key,value) settings. Keys: curveName, calibrateDiscountCurve, solveMode, parameterization, logDfScheme, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, targetCollateral, targetTenor, liborBasis
discountCurve is handle StorableDiscountCurve
    Optional discount curve (OIS-collateralized) needed when calibrating a forward curve (calibrateDiscountCurve=FALSE)
&outputs
result is handle StorableCurveCalibrationResult
    The calibration result (curve + diagnostics)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationResult_Get_Curve
    Extract the calibrated discount curve from a curve calibration result
&inputs
result is handle StorableCurveCalibrationResult
    The calibration result handle (from CALIBRATE.SINGLECURVE)
&outputs
curve is handle StorableDiscountCurve
    The calibrated discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationResult_Get
    Extract a diagnostic attribute from a curve calibration result.
    Use CALIBRATIONRESULT.GET.CURVE for the curve itself.
&inputs
result is handle StorableCurveCalibrationResult
    The calibration result handle (from CALIBRATE.SINGLECURVE)
attribute is string
    Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual
&outputs
value is cell[][]
    The requested diagnostic. Rate vectors (marketRates/modelRates/residuals) return
    as an Nx1 column; scalar stats (maxAbsResidual/rmsResidual) return as 1x1.
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

        // Helper: apply double-valued settings (type check once, then dispatch by key)
        void ApplyDoubleSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (!Cell::IsDouble(val))
                return;
            double d = Cell::ToDouble(val);
            if (key == "smoothingWeight")
                b.smoothingWeight_ = d;
            else if (key == "tolerance")
                b.tolerance_ = d;
            else if (key == "fitTolerance")
                b.fitTolerance_ = d;
            else if (key == "initialGuess")
                b.initialGuess_ = d;
        }

        // Helper: apply int-valued settings
        void ApplyIntSettings(const String_& key, const Cell_& val, CurveCalibrationSpecBuilder_& b) {
            if (!Cell::IsInt(val))
                return;
            int i = Cell::ToInt(val);
            if (key == "maxEvaluations")
                b.maxEvaluations_ = i;
            else if (key == "maxRestarts")
                b.maxRestarts_ = i;
        }

        // Helper: apply dictionary settings to a CurveCalibrationSpecBuilder_
        void ApplySettings(const Dictionary_& settings, CurveCalibrationSpecBuilder_& b) {
            for (const auto& kv : settings) {
                const String_& key = kv.first;
                const Cell_& val = kv.second;
                if (key == "calibrateDiscountCurve") {
                    if (Cell::IsBool(val))
                        b.calibrateDiscountCurve_ = Cell::ToBool(val);
                } else {
                    ApplyStringSettings(key, val, b);
                    ApplyDoubleSettings(key, val, b);
                    ApplyIntSettings(key, val, b);
                }
            }
        }

        void Calibrate_SingleCurve(const Date_& today,
                                    const String_& ccy,
                                    const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                    const Vector_<Date_>& knotDates,
                                    const Matrix_<Cell_>& settings,
                                    const Handle_<StorableDiscountCurve_>& discountCurve,
                                    Handle_<StorableCurveCalibrationResult_>* result) {
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

            // Optional discount curve for forward-curve calibration, keyed by the
            // calibration's target collateral (defaults to OIS). Forward calibration
            // (calibrateDiscountCurve=FALSE) requires it; discount calibration does not.
            if (discountCurve)
                builder.discountCurves_[builder.targetCollateral_] = discountCurve->val_;
            else
                REQUIRE(builder.calibrateDiscountCurve_,
                        "Forward-curve calibration (calibrateDiscountCurve=FALSE) requires a discountCurve input");

            // Build spec and calibrate via the dal-public interface
            auto spec = builder.Build();
            auto calibrated = Dal::CalibrateSingleCurve(spec);

            // Bundle curve + diagnostics into a single storable result handle
            result->reset(new StorableCurveCalibrationResult_(calibrated));
        }

        void CalibrationResult_Get_Curve(const Handle_<StorableCurveCalibrationResult_>& result,
                                          Handle_<StorableDiscountCurve_>* curve) {
            REQUIRE(result, "Invalid calibration result handle");
            curve->reset(new StorableDiscountCurve_(result->val_.curve_));
        }

        void CalibrationResult_Get(const Handle_<StorableCurveCalibrationResult_>& result,
                                    const String_& attribute,
                                    Matrix_<Cell_>* value) {
            REQUIRE(result, "Invalid calibration result handle");
            const auto& diag = result->val_.diagnostics_;
            if (attribute == "marketRates")
                *value = AsCellColumn(diag.marketRates_);
            else if (attribute == "modelRates")
                *value = AsCellColumn(diag.modelRates_);
            else if (attribute == "residuals")
                *value = AsCellColumn(diag.residuals_);
            else if (attribute == "maxAbsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.maxAbsResidual_));
            else if (attribute == "rmsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.rmsResidual_));
            else
                THROW("Unknown calibration attribute: " + attribute
                      + " (expected marketRates, modelRates, residuals, maxAbsResidual, or rmsResidual)");
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_SingleCurve_public.inc>
#include <dal-excel/auto/MG_CalibrationResult_Get_Curve_public.inc>
#include <dal-excel/auto/MG_CalibrationResult_Get_public.inc>
#endif
} // namespace Dal
