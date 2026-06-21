//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include "__curve_storable.hpp"
#include <dal-public/src/xccycalibration.hpp>
#include <dal/utilities/dictionary.hpp>

/*IF--------------------------------------------------------------------------
public CalibrateXccyMarket
    Calibrate a cross-currency basis market from instruments and settings
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
    Array of cross-currency swap instrument handles (created with DA.CROSS.CURRENCY.SWAP.NEW)
knotDates is date[]
    Knot dates for the basis curve
&optional
settings is cell[][]
    &$.Cols() == 2 || $.Empty()\must have two columns (key, value)
    Optional settings as a two-column range. Supported keys:
    fxSpot (number), fxForwardCollateral (string), smoothingWeight (number),
    tolerance (number), fitTolerance (number), initialGuess (number),
    maxEvaluations (integer), maxRestarts (integer), solveMode (string: EXACT|APPROXIMATE)
&outputs
basisCurve is handle StorableDiscountCurve
    The calibrated basis discount curve
maxAbsResidual is number
    Maximum absolute residual
rmsResidual is number
    Root-mean-square residual
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

        void CalibrateXccyMarket(const Date_& today,
                                  const String_& domesticCcy,
                                  const String_& foreignCcy,
                                  const Handle_<StorableCurveBlock_>& domesticBlock,
                                  const Handle_<StorableCurveBlock_>& foreignBlock,
                                  const Vector_<Handle_<Storable_>>& instrumentWrappers,
                                  const Vector_<Date_>& knotDates,
                                  const Matrix_<Cell_>& settings,
                                  Handle_<StorableDiscountCurve_>* basisCurve,
                                  double* maxAbsResidual,
                                  double* rmsResidual) {
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
                ApplyXccySettings(dict, builder);
            }

            // Build spec and calibrate
            auto spec = builder.Build();
            auto result = Dal::CalibrateXccyMarket(spec);

            // Output basis curve (take the first one from the map)
            *basisCurve = Handle_<StorableDiscountCurve_>();
            for (const auto& kv : result.basisCurves_) {
                basisCurve->reset(new StorableDiscountCurve_(kv.second));
                break; // return first basis curve
            }

            // Output diagnostics
            const auto& diag = result.diagnostics_;
            *maxAbsResidual = diag.maxAbsResidual_;
            *rmsResidual = diag.rmsResidual_;
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrateXccyMarket_public.inc>
#endif
}
