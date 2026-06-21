//
// Created by wegam on 2026/6/20.
//
// Thin storable wrappers for protocol value types, so they can be stored in the
// DAL repository and passed between Excel functions as handles.

#pragma once

#include "__platform.hpp"
#include <dal/storage/storable.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal-public/src/curveprotocol.hpp>

namespace Dal {

    // -- CollateralType_ wrapper
    struct StorableCollateralType_ : public Storable_ {
        CollateralType_ val_;
        explicit StorableCollateralType_(const CollateralType_& v) : Storable_("CollateralType", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- PeriodLength_ wrapper
    struct StorablePeriodLength_ : public Storable_ {
        PeriodLength_ val_;
        explicit StorablePeriodLength_(const PeriodLength_& v) : Storable_("PeriodLength", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- DayBasis_ wrapper
    struct StorableDayBasis_ : public Storable_ {
        DayBasis_ val_;
        explicit StorableDayBasis_(const DayBasis_& v) : Storable_("DayBasis", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- RateLegConvention_ wrapper
    struct StorableRateLegConvention_ : public Storable_ {
        RateLegConvention_ val_;
        explicit StorableRateLegConvention_(const RateLegConvention_& v) : Storable_("RateLegConvention", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- RateIndexConvention_ wrapper
    struct StorableRateIndexConvention_ : public Storable_ {
        RateIndexConvention_ val_;
        explicit StorableRateIndexConvention_(const RateIndexConvention_& v) : Storable_("RateIndexConvention", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CurrencyPair_ wrapper
    struct StorableCurrencyPair_ : public Storable_ {
        CurrencyPair_ val_;
        explicit StorableCurrencyPair_(const CurrencyPair_& v) : Storable_("CurrencyPair", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- YCInstrument_ wrapper (for single-ccy instruments)
    struct StorableYCInstrument_ : public Storable_ {
        Handle_<YCInstrument_> val_;
        explicit StorableYCInstrument_(const Handle_<YCInstrument_>& v) : Storable_("YCInstrument", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CrossCurrencySwap_ wrapper
    struct StorableCrossCurrencySwap_ : public Storable_ {
        Handle_<CrossCurrencySwap_> val_;
        explicit StorableCrossCurrencySwap_(const Handle_<CrossCurrencySwap_>& v) : Storable_("CrossCurrencySwap", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- DiscountCurve_ wrapper
    struct StorableDiscountCurve_ : public Storable_ {
        Handle_<DiscountCurve_> val_;
        explicit StorableDiscountCurve_(const Handle_<DiscountCurve_>& v) : Storable_("DiscountCurve", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CurveBlock_ wrapper
    struct StorableCurveBlock_ : public Storable_ {
        Handle_<CurveBlock_> val_;
        explicit StorableCurveBlock_(const Handle_<CurveBlock_>& v) : Storable_("CurveBlock", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CurveCalibrationSpec_ wrapper
    struct StorableCurveCalibrationSpec_ : public Storable_ {
        CurveCalibrationSpec_ val_;
        explicit StorableCurveCalibrationSpec_(const CurveCalibrationSpec_& v) : Storable_("CurveCalibrationSpec", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CrossCurrencyCalibrationSpec_ wrapper
    struct StorableCrossCurrencyCalibrationSpec_ : public Storable_ {
        CrossCurrencyCalibrationSpec_ val_;
        explicit StorableCrossCurrencyCalibrationSpec_(const CrossCurrencyCalibrationSpec_& v) : Storable_("CrossCurrencyCalibrationSpec", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

} // namespace Dal
