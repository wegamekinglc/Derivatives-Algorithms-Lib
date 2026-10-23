#pragma once

#include <dal/curve/xccyscript.hpp>
#include <dal/model/xccygaussian.hpp>
#include <dal/script/observationplan.hpp>

/*IF--------------------------------------------------------------------------
enumeration XccyObservationKind
    Economic observation identity in the dedicated XCCY path
switchable
alternative RateFixing
alternative ResetFxFixing
alternative PaymentFxSpot
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
#include <dal/auto/MG_XccyObservationKind_enum.hpp>

    struct XccyObservationRequest_ {
        XccyObservationKind_ kind_;
        String_ currency_, tenor_, route_;
        Date_ start_, end_;
        double accrual_ = 0.;
    };
    namespace XccyScriptDetail {
        struct Plan_;
    } // namespace XccyScriptDetail
    template <class T_> class XccyScriptPathProvider_;
    template <class T_> class XccyScriptPath_ {
        AAD::Scenario_<T_> samples_;
        String_ planIdentity_, evaluationIdentity_;
        Handle_<XccyGaussianModelData_> data_;
        friend class XccyScriptPathProvider_<T_>;
        friend class PreparedXccyScript_;
        XccyScriptPath_(AAD::Scenario_<T_> samples, String_ identity, String_ evaluationIdentity, Handle_<XccyGaussianModelData_> data)
            : samples_(std::move(samples)), planIdentity_(std::move(identity)), evaluationIdentity_(std::move(evaluationIdentity)),
              data_(std::move(data)) {}

    public:
        [[nodiscard]] const AAD::Scenario_<T_>& Samples() const { return samples_; }
        [[nodiscard]] const String_& EvaluationIdentity() const { return evaluationIdentity_; }
    };
    template <class T_> struct XccyScriptPayment_ {
        Date_ date_;
        T_ discountedAmount_;
    };

    class PreparedXccyScript_ {
        std::shared_ptr<const XccyScriptDetail::Plan_> plan_;

    public:
        PreparedXccyScript_(const Handle_<CalibrationInstrument_>& instrument,
                            const CalibrationPreparationContext_& context,
                            const Handle_<XccyGaussianModelData_>& model);
        [[nodiscard]] const Vector_<Date_>& SampleDates() const;
        [[nodiscard]] const Vector_<AAD::SampleDef_>& DefLine() const;
        [[nodiscard]] const Vector_<XccyObservationRequest_>& Requests() const;
        [[nodiscard]] const ObservationPlan_& Observations() const;
        [[nodiscard]] const String_& PlanIdentity() const;
        [[nodiscard]] const Handle_<XccyGaussianModelData_>& ModelData() const;
        [[nodiscard]] const Handle_<CalibrationInstrument_>& Instrument() const;
        void RequireCompatible(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context) const;
        template <class T_> [[nodiscard]] Vector_<XccyScriptPayment_<T_>> EvaluatePayments(const XccyScriptPath_<T_>& path, bool compiled) const;
    };

    template <class T_ = double> class XccyScriptPathProvider_ {
        PreparedXccyScript_ prepared_;
        Handle_<XccyGaussianModelData_> data_;
        String_ evaluationIdentity_;
        std::unique_ptr<AAD::XccyGaussian_<T_>> model_;

    public:
        explicit XccyScriptPathProvider_(const PreparedXccyScript_& prepared, const Handle_<XccyGaussianModelData_>& candidate = {});
        [[nodiscard]] const AAD::XccyGaussian_<T_>& Model() const { return *model_; }
        [[nodiscard]] XccyScriptPath_<T_> Observe(const Vector_<AAD::XccyGaussianState_<T_>>& states) const;
        [[nodiscard]] XccyScriptPath_<T_> GeneratePath(const Vector_<>& normals) const;
    };
} // namespace Dal::Script
