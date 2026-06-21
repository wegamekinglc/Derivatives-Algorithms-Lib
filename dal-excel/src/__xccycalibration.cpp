//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal/math/cell.hpp>
#include <dal-public/src/xccycalibration.hpp>
#include <dal/utilities/dictionary.hpp>

/*IF--------------------------------------------------------------------------
public Calibrate_XccyMarket
    Calibrate a cross-currency basis market from instruments and settings.
    Returns a result handle bundling the basis curve and the fit diagnostics.
&inputs
today is date
    The valuation/trade date
domesticCcy is string
    Domestic currency code (e.g. "USD")
foreignCcy is string
    Foreign currency code (e.g. "EUR")
domesticBlock is handle StorableCurveBlock
    The pre-calibrated domestic curve block
foreignBlock is handle StorableCurveBlock
    The pre-calibrated foreign curve block
instruments is handle[]
    Array of cross-currency swap instrument handles (created with CROSSCURRENCYSWAP.NEW)
knotDates is date[]
    Knot dates for the basis curve
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional two-column (key,value) settings. Keys: fxSpot, fxForwardCollateral, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode (EXACT|APPROXIMATE)
&outputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result (basis curve + diagnostics)
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyCalibrationResult_Get_BasisCurve
    Extract the basis discount curve from a cross-currency calibration result
&inputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)
&outputs
curve is handle StorableDiscountCurve
    The calibrated basis discount curve
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyCalibrationResult_Get
    Extract a diagnostic attribute from a cross-currency calibration result.
    Use XCCYCALIBRATIONRESULT.GET.BASISCURVE for the basis curve itself.
&inputs
result is handle StorableCrossCurrencyCalibrationResult
    The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)
attribute is string
    Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual
&outputs
value is cell[][]
    The requested diagnostic. Rate vectors (marketRates/modelRates/residuals) return
    as an Nx1 column; scalar stats (maxAbsResidual/rmsResidual) return as 1x1.
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void ApplyXccyStringSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (key == "fxForwardCollateral")
                b.fxForwardCollateral_ = CollateralType_(Cell::ToString(val));
            else if (key == "solveMode")
                b.solveMode_ = CurveSolveMode_(Cell::ToString(val));
        }

        void ApplyXccyDoubleSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (!Cell::IsDouble(val))
                return;
            double d = Cell::ToDouble(val);
            if (key == "fxSpot")
                b.fxSpot_ = d;
            else if (key == "smoothingWeight")
                b.smoothingWeight_ = d;
            else if (key == "tolerance")
                b.tolerance_ = d;
            else if (key == "fitTolerance")
                b.fitTolerance_ = d;
            else if (key == "initialGuess")
                b.initialGuess_ = d;
        }

        void ApplyXccyIntSettings(const String_& key, const Cell_& val, CrossCurrencyCalibrationSpecBuilder_& b) {
            if (!Cell::IsInt(val))
                return;
            int i = Cell::ToInt(val);
            if (key == "maxEvaluations")
                b.maxEvaluations_ = i;
            else if (key == "maxRestarts")
                b.maxRestarts_ = i;
        }

        void ApplyXccySettings(const Dictionary_& settings, CrossCurrencyCalibrationSpecBuilder_& b) {
            for (const auto& kv : settings) {
                const String_& key = kv.first;
                const Cell_& val = kv.second;
                ApplyXccyStringSettings(key, val, b);
                ApplyXccyDoubleSettings(key, val, b);
                ApplyXccyIntSettings(key, val, b);
            }
        }

        Matrix_<Cell_> AsColumn(const Vector_<>& v) {
            Matrix_<Cell_> m(v.size(), 1);
            for (int i = 0; i < v.size(); ++i)
                m(i, 0) = Cell_(v[i]);
            return m;
        }

        void Calibrate_XccyMarket(const Date_& today,
                                   const String_& domesticCcy,
                                   const String_& foreignCcy,
                                   const Handle_<StorableCurveBlock_>& domesticBlock,
                                   const Handle_<StorableCurveBlock_>& foreignBlock,
                                   const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                   const Vector_<Date_>& knotDates,
                                   const Matrix_<Cell_>& settings,
                                   Handle_<StorableCrossCurrencyCalibrationResult_>* result) {
            REQUIRE(domesticBlock, "Invalid domestic curve block handle");
            REQUIRE(foreignBlock, "Invalid foreign curve block handle");

            CrossCurrencyCalibrationSpecBuilder_ builder;
            builder.today_ = today;
            builder.basisPair_ = CurrencyPair_New(domesticCcy, foreignCcy);
            builder.domesticCurveBlock_ = domesticBlock->val_;
            builder.foreignCurveBlock_ = foreignBlock->val_;

            // Convert instrument wrappers to raw handles
            for (const auto& w : instrumentWrappers) {
                REQUIRE(w, "Invalid instrument handle");
                auto xccyInst = handle_cast<StorableCrossCurrencySwap_>(w);
                REQUIRE(xccyInst, "Instrument must be a cross-currency swap");
                builder.instruments_.push_back(xccyInst->val_);
            }

            builder.knotDates_ = knotDates;

            // Convert Matrix_<Cell_> to Dictionary_ and apply optional settings
            if (!settings.Empty()) {
                Dictionary_ dict;
                for (int i = 0; i < settings.Rows(); ++i) {
                    if (Cell::IsEmpty(settings(i, 0)))
                        break;
                    dict.Insert(Cell::ToString(settings(i, 0)), settings(i, 1));
                }
                ApplyXccySettings(dict, builder);
            }

            const CurrencyPair_ pair = builder.basisPair_;
            auto spec = builder.Build();
            auto calibrated = Dal::CalibrateXccyMarket(spec);

            // Resolve the basis curve for the calibrated currency pair
            auto it = calibrated.basisCurves_.find(pair);
            REQUIRE(it != calibrated.basisCurves_.end(), "Basis curve not found for requested currency pair");
            result->reset(new StorableCrossCurrencyCalibrationResult_(calibrated, it->second));
        }

        void XccyCalibrationResult_Get_BasisCurve(const Handle_<StorableCrossCurrencyCalibrationResult_>& result,
                                                    Handle_<StorableDiscountCurve_>* curve) {
            REQUIRE(result, "Invalid XCCY calibration result handle");
            curve->reset(new StorableDiscountCurve_(result->basisCurve_));
        }

        void XccyCalibrationResult_Get(const Handle_<StorableCrossCurrencyCalibrationResult_>& result,
                                         const String_& attribute,
                                         Matrix_<Cell_>* value) {
            REQUIRE(result, "Invalid XCCY calibration result handle");
            const auto& diag = result->val_.diagnostics_;
            if (attribute == "marketRates")
                *value = AsColumn(diag.marketRates_);
            else if (attribute == "modelRates")
                *value = AsColumn(diag.modelRates_);
            else if (attribute == "residuals")
                *value = AsColumn(diag.residuals_);
            else if (attribute == "maxAbsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.maxAbsResidual_));
            else if (attribute == "rmsResidual")
                *value = Matrix_<Cell_>(1, 1, Cell_(diag.rmsResidual_));
            else
                THROW("Unknown XCCY calibration attribute: " + attribute
                      + " (expected marketRates, modelRates, residuals, maxAbsResidual, or rmsResidual)");
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_XccyMarket_public.inc>
#include <dal-excel/auto/MG_XccyCalibrationResult_Get_BasisCurve_public.inc>
#include <dal-excel/auto/MG_XccyCalibrationResult_Get_public.inc>
#endif
} // namespace Dal
