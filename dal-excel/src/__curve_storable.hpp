//
// Created by wegam on 2026/6/20.
//
// Thin storable wrappers for protocol value types, so they can be stored in the
// DAL repository and passed between Excel functions as handles.

#pragma once

#include <memory>

#include "__platform.hpp"
#include <dal-public/src/curvepricing.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/xccycalibration.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/cell.hpp>
#include <dal/storage/storable.hpp>

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

    struct StorableFxResetConvention_ : public Storable_ {
        FxResetConvention_ val_;
        explicit StorableFxResetConvention_(const FxResetConvention_& v) : Storable_("FxResetConvention", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableMarketFixingSnapshot_ : public Storable_ {
        Handle_<MarketFixingSnapshot_> val_;
        explicit StorableMarketFixingSnapshot_(const Handle_<MarketFixingSnapshot_>& v) : Storable_("MarketFixingSnapshot", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableFixingIdentity_ : public Storable_ {
        FixingIdentity_ val_;
        explicit StorableFixingIdentity_(const FixingIdentity_& v) : Storable_("FixingIdentity", String_()), val_(v) {}
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

    struct StorableCrossCurrencySwapConfig_ : public Storable_ {
        CrossCurrencySwapConfig_ val_;
        explicit StorableCrossCurrencySwapConfig_(const CrossCurrencySwapConfig_& v) : Storable_("CrossCurrencySwapConfig", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableDiscountCurve_ : public Storable_ {
        Handle_<DiscountCurve_> val_;
        explicit StorableDiscountCurve_(const Handle_<DiscountCurve_>& v) : Storable_("DiscountCurve", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableRateTradeDefinition_ : public Storable_ {
        RateTradeDefinition_ val_;
        explicit StorableRateTradeDefinition_(const RateTradeDefinition_& v) : Storable_("RateTradeDefinition", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableRatePricingMarket_ : public Storable_ {
        RatePricingMarket_ val_;
        explicit StorableRatePricingMarket_(const RatePricingMarket_& v) : Storable_("RatePricingMarket", String_()), val_(v) {}
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
        CurveCalibrationSpec_ spec_;
        CurveCalibrationOptions_ options_;
        StorableCurveCalibrationResult_(const CalibrationResult_& v, const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options)
            : Storable_("CurveCalibrationResult", String_()), val_(v), spec_(spec), options_(options) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableMultiCurveCalibrationResult_ : public Storable_ {
        MultiCurveCalibrationResult_ val_;
        explicit StorableMultiCurveCalibrationResult_(const MultiCurveCalibrationResult_& v)
            : Storable_("MultiCurveCalibrationResult", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableJointMultiCurveCalibrationResult_ : public Storable_ {
        JointMultiCurveCalibrationResult_ val_;
        explicit StorableJointMultiCurveCalibrationResult_(const JointMultiCurveCalibrationResult_& v)
            : Storable_("JointMultiCurveCalibrationResult", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableCrossCurrencyCalibrationSpec_ : public Storable_ {
        CrossCurrencyCalibrationSpec_ val_;
        explicit StorableCrossCurrencyCalibrationSpec_(const CrossCurrencyCalibrationSpec_& v)
            : Storable_("CrossCurrencyCalibrationSpec", String_()), val_(v) {}
        void Write(Archive::Store_&) const override {}
    };

    // -- CrossCurrencyCalibrationResult_ wrapper, returned by Calibrate_XccyMarket.
    // Holds the full result plus the basis curve resolved for the calibrated pair.
    struct StorableCrossCurrencyCalibrationResult_ : public Storable_ {
        CrossCurrencyCalibrationResult_ val_;
        CrossCurrencyCalibrationSpec_ spec_;
        CrossCurrencyCalibrationOptions_ options_;
        Handle_<DiscountCurve_> basisCurve_;
        StorableCrossCurrencyCalibrationResult_(const CrossCurrencyCalibrationResult_& v,
                                                const CrossCurrencyCalibrationSpec_& spec,
                                                const CrossCurrencyCalibrationOptions_& options,
                                                const Handle_<DiscountCurve_>& basis)
            : Storable_("CrossCurrencyCalibrationResult", String_()), val_(v), spec_(spec), options_(options), basisCurve_(basis) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableJointXccyCalibrationResult_ : public Storable_ {
        JointXccyCalibrationResult_ val_;
        JointXccyCalibrationSpec_ spec_;
        JointXccyCalibrationOptions_ options_;
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        Handle_<DiscountCurve_> basisCurve_;

        StorableJointXccyCalibrationResult_(const JointXccyCalibrationResult_& v,
                                            const JointXccyCalibrationSpec_& spec,
                                            const JointXccyCalibrationOptions_& options)
            : Storable_("JointXccyCalibrationResult", String_()), val_(v), spec_(spec), options_(options),
              domesticBlock_(JointXccyResultDomesticBlock(val_)), foreignBlock_(JointXccyResultForeignBlock(val_)),
              basisCurve_(JointXccyResultBasisCurve(val_)) {}
        void Write(Archive::Store_&) const override {}
    };

    struct StorableRateQuoteRiskProvenance_ : public Storable_ {
        const std::shared_ptr<const RateQuoteRiskProvenance_> val_;
        const String_ calibrationId_;
        const String_ kind_;
        const String_ reason_;

        explicit StorableRateQuoteRiskProvenance_(const RateQuoteRiskProvenance_& value)
            : Storable_("RateQuoteRiskProvenance", String_()), val_(std::make_shared<const RateQuoteRiskProvenance_>(value)),
              calibrationId_(value.CalibrationId()), kind_(value.Kind()), reason_(value.Reason()) {}
        StorableRateQuoteRiskProvenance_(const String_& calibrationId, const String_& kind, const String_& reason)
            : Storable_("RateQuoteRiskProvenance", String_()), calibrationId_(calibrationId), kind_(kind), reason_(reason) {}
        [[nodiscard]] bool Native() const { return static_cast<bool>(val_); }
        [[nodiscard]] const String_& AxisFingerprint() const {
            static const String_ empty;
            return val_ ? val_->Axis().fingerprint_ : empty;
        }
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
