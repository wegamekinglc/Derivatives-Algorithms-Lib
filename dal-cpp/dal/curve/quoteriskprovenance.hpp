//
// Created by dal-implementer on 2026/8/31.
//

#pragma once

#include <map>
#include <memory>

#include <dal/curve/calibration.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    struct RatePricingMarket_;

    struct RateQuoteRiskProvenanceConfig_ {
        String_ calibrationId_;
        std::map<String_, String_> componentKeyByParameterBlock_;
    };

    struct RateQuoteRiskRange_ {
        String_ blockKey_;
        int offset_ = 0;
        int size_ = 0;
    };

    struct RateQuoteRiskParameterCoordinate_ {
        String_ blockKey_;
        int blockOrdinal_ = 0;
        int globalOrdinal_ = 0;
        Date_ date_;
        CurveFreeParameterComponent_ component_;
    };

    struct RateQuoteRiskQuoteCoordinate_ {
        String_ blockKey_;
        int blockOrdinal_ = 0;
        int globalOrdinal_ = 0;
        String_ displayName_;
        String_ unit_ = "DECIMAL_QUOTE";
    };

    struct RateQuoteRiskAxis_ {
        String_ scheme_;
        String_ fingerprint_;
        Vector_<RateQuoteRiskRange_> parameterRanges_;
        Vector_<RateQuoteRiskRange_> residualRanges_;
        Vector_<RateQuoteRiskParameterCoordinate_> parameters_;
        Vector_<RateQuoteRiskQuoteCoordinate_> quotes_;
    };

    struct RateQuoteRiskComponentState_ {
        String_ componentKey_;
        String_ fingerprint_;
    };

    struct RateQuoteRiskState_ {
        String_ scheme_;
        String_ fingerprint_;
        Vector_<RateQuoteRiskComponentState_> components_;
    };

    class RateQuoteRiskProvenance_ {
    public:
        struct Data_;

    private:
        std::shared_ptr<const Data_> data_;

        explicit RateQuoteRiskProvenance_(const std::shared_ptr<const Data_>& data);

        friend RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_&,
                                                                            const CurveCalibrationResult_&,
                                                                            const CurveCalibrationOptions_&,
                                                                            const RatePricingMarket_&,
                                                                            const RateQuoteRiskProvenanceConfig_&);
        friend RateQuoteRiskProvenance_ BuildJointXccyQuoteRiskProvenance(const JointXccyCalibrationSpec_&,
                                                                          const JointXccyCalibrationResult_&,
                                                                          const JointXccyCalibrationOptions_&,
                                                                          const RatePricingMarket_&,
                                                                          const RateQuoteRiskProvenanceConfig_&);
        friend RateQuoteRiskProvenance_ BuildStagedXccyBasisQuoteRiskProvenance(const CrossCurrencyCalibrationSpec_&,
                                                                                const CrossCurrencyCalibrationResult_&,
                                                                                const CrossCurrencyCalibrationOptions_&,
                                                                                const RatePricingMarket_&,
                                                                                const RateQuoteRiskProvenanceConfig_&);

    public:
        [[nodiscard]] const String_& Kind() const;
        [[nodiscard]] bool Available() const;
        [[nodiscard]] const String_& Reason() const;
        [[nodiscard]] const String_& CalibrationId() const;
        [[nodiscard]] const std::map<String_, String_>& ComponentKeyByParameterBlock() const;
        [[nodiscard]] const RateQuoteRiskAxis_& Axis() const;
        [[nodiscard]] const RateQuoteRiskState_& State() const;
        [[nodiscard]] const Matrix_<>& EffectiveInverse() const;
        [[nodiscard]] double Tolerance() const;
    };

    [[nodiscard]] const String_& RateQuoteRiskAxisFingerprintScheme();
    [[nodiscard]] const String_& RateQuoteRiskStateFingerprintScheme();

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CurveCalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config);

    RateQuoteRiskProvenance_ BuildJointXccyQuoteRiskProvenance(const JointXccyCalibrationSpec_& spec,
                                                               const JointXccyCalibrationResult_& result,
                                                               const JointXccyCalibrationOptions_& options,
                                                               const RatePricingMarket_& boundMarket,
                                                               const RateQuoteRiskProvenanceConfig_& config);

    RateQuoteRiskProvenance_ BuildStagedXccyBasisQuoteRiskProvenance(const CrossCurrencyCalibrationSpec_& spec,
                                                                     const CrossCurrencyCalibrationResult_& result,
                                                                     const CrossCurrencyCalibrationOptions_& options,
                                                                     const RatePricingMarket_& boundMarket,
                                                                     const RateQuoteRiskProvenanceConfig_& config);
} // namespace Dal
