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
        explicit StorableCollateralType_(const CollateralType_& v) : val_(v) {}
    };

    // -- PeriodLength_ wrapper
    struct StorablePeriodLength_ : public Storable_ {
        PeriodLength_ val_;
        explicit StorablePeriodLength_(const PeriodLength_& v) : val_(v) {}
    };

    // -- DayBasis_ wrapper
    struct StorableDayBasis_ : public Storable_ {
        DayBasis_ val_;
        explicit StorableDayBasis_(const DayBasis_& v) : val_(v) {}
    };

    // -- RateLegConvention_ wrapper
    struct StorableRateLegConvention_ : public Storable_ {
        RateLegConvention_ val_;
        explicit StorableRateLegConvention_(const RateLegConvention_& v) : val_(v) {}
    };

    // -- RateIndexConvention_ wrapper
    struct StorableRateIndexConvention_ : public Storable_ {
        RateIndexConvention_ val_;
        explicit StorableRateIndexConvention_(const RateIndexConvention_& v) : val_(v) {}
    };

    // -- CurrencyPair_ wrapper
    struct StorableCurrencyPair_ : public Storable_ {
        CurrencyPair_ val_;
        explicit StorableCurrencyPair_(const CurrencyPair_& v) : val_(v) {}
    };

    // -- YCInstrument_ wrapper (for single-ccy instruments)
    struct StorableYCInstrument_ : public Storable_ {
        Handle_<YCInstrument_> val_;
        explicit StorableYCInstrument_(const Handle_<YCInstrument_>& v) : val_(v) {}
    };

    // -- CrossCurrencySwap_ wrapper
    struct StorableCrossCurrencySwap_ : public Storable_ {
        Handle_<CrossCurrencySwap_> val_;
        explicit StorableCrossCurrencySwap_(const Handle_<CrossCurrencySwap_>& v) : val_(v) {}
    };

    // -- DiscountCurve_ wrapper
    struct StorableDiscountCurve_ : public Storable_ {
        Handle_<DiscountCurve_> val_;
        explicit StorableDiscountCurve_(const Handle_<DiscountCurve_>& v) : val_(v) {}
    };

    // -- CurveBlock_ wrapper
    struct StorableCurveBlock_ : public Storable_ {
        Handle_<CurveBlock_> val_;
        explicit StorableCurveBlock_(const Handle_<CurveBlock_>& v) : val_(v) {}
    };

    // -- CurveCalibrationSpec_ wrapper
    struct StorableCurveCalibrationSpec_ : public Storable_ {
        CurveCalibrationSpec_ val_;
        explicit StorableCurveCalibrationSpec_(const CurveCalibrationSpec_& v) : val_(v) {}
    };

    // -- CrossCurrencyCalibrationSpec_ wrapper
    struct StorableCrossCurrencyCalibrationSpec_ : public Storable_ {
        CrossCurrencyCalibrationSpec_ val_;
        explicit StorableCrossCurrencyCalibrationSpec_(const CrossCurrencyCalibrationSpec_& v) : val_(v) {}
    };

} // namespace Dal
