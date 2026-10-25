#pragma once

#include <dal/script/xccymc.hpp>

namespace Dal::XccyMcInternal {
    struct CouponViews_ {
        double low_, high_;
    };
    class CoefficientExecution_ {
    public:
        virtual ~CoefficientExecution_() = default;
        [[nodiscard]] virtual CalibrationAffineCoefficients_ Next(const Vector_<>& normals, CalibrationMcResult_* estimate) = 0;
    };
    struct Member_ {
        PreparedCalibrationInstrument_ admitted_;
        CouponViews_ views_;
        CalibrationPreparationContext_ frozenContext_;
        Script::PreparedXccyScript_ zero_, unit_;
        Handle_<XccyGaussianModelData_> model_;
        XccyMcSettings_ settings_;
        CalibrationMcMapping_ mapping_;
        XccyMcReservation_ reservation_;
        String_ identity_;
        double coupon_;

        Member_(const Handle_<CalibrationInstrument_>& instrument,
                const String_& measure,
                const CalibrationPreparationContext_& context,
                const Handle_<XccyGaussianModelData_>& model,
                const XccyMcSettings_& settings);
        void Preflight(const Handle_<XccyGaussianModelData_>& data) const;
        [[nodiscard]] std::unique_ptr<CoefficientExecution_> Execution(const Handle_<XccyGaussianModelData_>& data) const;
    };
    void RequireDerivatives(const XccyGaussianModelData_& model, const XccyMcSettings_& settings);
    void Accumulate(const CalibrationAffineCoefficients_& c, size_t paths, CalibrationMcBatchValue_* batch);
    void FinishBatch(double coupon, CalibrationMcBatchValue_* batch);
    [[nodiscard]] String_ MappingIdentity(const CalibrationMcMapping_& mapping);
    void Summarize(XccyMcResult_* result, const PreparedCalibrationInstrument_& admitted, double coupon);
    [[nodiscard]] Handle_<CalibrationReport_> Report(const XccyMcResult_& result,
                                                     const PreparedCalibrationInstrument_& admitted,
                                                     const XccyGaussianModelData_& model,
                                                     const XccyMcSettings_& settings);
} // namespace Dal::XccyMcInternal
