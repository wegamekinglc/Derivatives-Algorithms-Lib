//
// Created by wegam on 2026/6/20.
//
// Thin storable wrappers for protocol value types, so they can be stored in the
// DAL repository and passed between Excel functions as handles.

#pragma once

#include "__platform.hpp"
#include <dal/math/cell.hpp>
#include <dal/storage/storable.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>

namespace Dal {

    struct StorableCollateralType_ : public Storable_ {
        CollateralType_ val_;
        explicit StorableCollateralType_(const CollateralType_& v) : Storable_("CollateralType", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorablePeriodLength_ : public Storable_ {
        PeriodLength_ val_;
        explicit StorablePeriodLength_(const PeriodLength_& v) : Storable_("PeriodLength", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableDayBasis_ : public Storable_ {
        DayBasis_ val_;
        explicit StorableDayBasis_(const DayBasis_& v) : Storable_("DayBasis", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableRateLegConvention_ : public Storable_ {
        RateLegConvention_ val_;
        explicit StorableRateLegConvention_(const RateLegConvention_& v) : Storable_("RateLegConvention", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableRateIndexConvention_ : public Storable_ {
        RateIndexConvention_ val_;
        explicit StorableRateIndexConvention_(const RateIndexConvention_& v) : Storable_("RateIndexConvention", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCurrencyPair_ : public Storable_ {
        CurrencyPair_ val_;
        explicit StorableCurrencyPair_(const CurrencyPair_& v) : Storable_("CurrencyPair", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableYCInstrument_ : public Storable_ {
        Handle_<YCInstrument_> val_;
        explicit StorableYCInstrument_(const Handle_<YCInstrument_>& v) : Storable_("YCInstrument", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCrossCurrencySwap_ : public Storable_ {
        Handle_<CrossCurrencySwap_> val_;
        explicit StorableCrossCurrencySwap_(const Handle_<CrossCurrencySwap_>& v) : Storable_("CrossCurrencySwap", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableDiscountCurve_ : public Storable_ {
        Handle_<DiscountCurve_> val_;
        explicit StorableDiscountCurve_(const Handle_<DiscountCurve_>& v) : Storable_("DiscountCurve", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCurveBlock_ : public Storable_ {
        Handle_<CurveBlock_> val_;
        explicit StorableCurveBlock_(const Handle_<CurveBlock_>& v) : Storable_("CurveBlock", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCurveCalibrationSpec_ : public Storable_ {
        CurveCalibrationSpec_ val_;
        explicit StorableCurveCalibrationSpec_(const CurveCalibrationSpec_& v) : Storable_("CurveCalibrationSpec", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCurveCalibrationResult_ : public Storable_ {
        CalibrationResult_ val_;
        explicit StorableCurveCalibrationResult_(const CalibrationResult_& v) : Storable_("CurveCalibrationResult", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCrossCurrencyCalibrationSpec_ : public Storable_ {
        CrossCurrencyCalibrationSpec_ val_;
        explicit StorableCrossCurrencyCalibrationSpec_(const CrossCurrencyCalibrationSpec_& v) : Storable_("CrossCurrencyCalibrationSpec", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CrossCurrencyCalibrationResult_ wrapper, returned by Calibrate_XccyMarket.
    // Holds the full result plus the basis curve resolved for the calibrated pair.
    struct StorableCrossCurrencyCalibrationResult_ : public Storable_ {
        CrossCurrencyCalibrationResult_ val_;
        Handle_<DiscountCurve_> basisCurve_;
        explicit StorableCrossCurrencyCalibrationResult_(const CrossCurrencyCalibrationResult_& v,
                                                          const Handle_<DiscountCurve_>& basis)
            : Storable_("CrossCurrencyCalibrationResult", String_()), val_(v), basisCurve_(basis) {}
        void Write(Archive::Store_&) const override {}
    };

    // Lay a double vector down as an Nx1 cell column, used by the calibration
    // result Get accessors to return rate vectors to Excel.
    inline Matrix_<Cell_> AsCellColumn(const Vector_<>& v) {
        Matrix_<Cell_> m(v.size(), 1);
        for (int i = 0; i < v.size(); ++i)
            m(i, 0) = Cell_(v[i]);
        return m;
    }

} // namespace Dal
