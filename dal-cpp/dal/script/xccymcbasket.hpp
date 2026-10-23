#pragma once

#include <dal/script/xccymc.hpp>

namespace Dal {
    namespace XccyMcBasketInternal {
        struct Producer_;
        struct TestAccess_;
    } // namespace XccyMcBasketInternal

    struct XccyMcBasketMember_ {
        String_ memberId_;
        Handle_<CalibrationInstrument_> instrument_;
        String_ measureId_;
    };
    struct XccyMcBasketMemberAllocation_ {
        Handle_<CalibrationInstrument_> instrument_;
        CalibrationPreparationContext_ frozenContext_;
        String_ measureUnit_;
        String_ memberId_, instrumentId_, instrumentIdentity_, sourceIdentity_, definitionIdentity_, bindingIdentity_, measureId_;
        String_ preparationIdentity_, contextIdentity_, compilerIdentity_, lowBindingIdentity_, highBindingIdentity_, nativeMapIdentity_;
        CalibrationMcMapping_ mapping_;
        Vector_<Date_> grid_;
        Script::ObservationPlan_ observations_{{}, {}};
        Vector_<Script::XccyObservationRequest_> requests_;
        size_t firstCoordinate_ = 0, coordinateEnd_ = 0;
        Vector_<XccyMcRange_> phaseRanges_, ranges_;
        bool deterministic_ = false;
    };
    struct XccyMcBasketAllocation_ {
        String_ version_ = "dal.xccy-gaussian-basket-coordinate-ranges/1", identity_, executionIdentity_;
        String_ streamVersion_ = "dal.mrg32-normal-paths/1", normalPrecision_ = "Precise", seekConvention_ = "absolute-scalar/1";
        XccyMonteCarloPhase_ phase_;
        int seed_ = 0, batches_ = 0, pathsPerBatch_ = 0;
        size_t reservedPaths_ = 2359296, coordinateEnd_ = 0, expectedConsumption_ = 0;
        Vector_<XccyMcBasketMemberAllocation_> members_;
    };
    struct XccyMcBasketExecutionHeader_ {
        String_ version_ = "dal.xccy-gaussian-mc-basket-execution/1", ruleId_ = "dal.xccy-gaussian-script-mc-basket/1";
        String_ planIdentity_, preparedModelIdentity_, derivativeMethod_;
        std::optional<String_> attemptedModelIdentity_, nativeMapIdentity_;
        XccyMcSettings_ settings_;
        Vector_<String_> parameterUnits_;
        bool candidateGeometryValidated_ = false;
    };
    struct XccyMcBasketMemberEstimate_ {
        CalibrationMcResult_ estimate_;
        CalibrationAffineCoefficients_ coefficients_;
        Matrix_<> coefficientCovariance_;
    };
    struct XccyMcBasketCompletedBatch_ {
        size_t ordinal_ = 0, paths_ = 0;
        XccyMcRange_ range_;
        Vector_<> firstNormals_, row_;
    };
    struct XccyMcBasketRetainedPath_ {
        size_t path_ = 0, firstCoordinate_ = 0;
        CalibrationAffineCoefficients_ coefficients_;
    };
    struct XccyMcBasketMemberProgress_ {
        String_ memberId_, state_ = "NotStarted";
        Vector_<XccyMcBasketCompletedBatch_> batches_;
        Vector_<XccyMcRange_> actualRanges_;
        Vector_<XccyMcBasketRetainedPath_> retainedPaths_;
        size_t consumedCoordinates_ = 0, completedPaths_ = 0, completedBatches_ = 0;
        std::optional<XccyMcBasketMemberEstimate_> completedEstimate_;
        std::optional<CalibrationDiagnostic_> failure_;
    };
    struct XccyMcBasketPartialSnapshot_ {
        String_ version_ = "dal.xccy-gaussian-mc-basket-partial/1";
        XccyMcBasketExecutionHeader_ header_;
        XccyMcBasketAllocation_ allocation_;
        Vector_<XccyMcBasketMemberProgress_> members_;
        size_t completedMembers_ = 0, consumedCoordinates_ = 0;
        std::optional<size_t> failedMember_, failedBatch_, failedPath_, observedCursor_;
        bool basketComplete_ = false;
    };
    struct XccyMcBasketAxis_ {
        String_ memberId_, label_, unit_;
    };
    struct XccyMcBasketSnapshot_ {
        String_ version_ = "dal.xccy-gaussian-mc-basket-result/1";
        XccyMcBasketExecutionHeader_ header_;
        XccyMcBasketAllocation_ allocation_;
        Vector_<XccyMcBasketMemberProgress_> members_;
        Vector_<XccyMcBasketAxis_> coefficientAxes_, outputAxes_;
        Vector_<Vector_<>> coefficientRows_;
        Vector_<> coefficientMeans_;
        Matrix_<> coefficientCovariance_, meanCovariance_, outputJacobian_, outputCovariance_;
        Handle_<XccyGaussianModelData_> model_;
        size_t consumedCoordinates_ = 0;
        bool basketComplete_ = true;
    };
    class XccyMcBasketPartial_ final {
        XccyMcBasketPartialSnapshot_ snapshot_;
        Handle_<CalibrationReport_> report_;
        XccyMcBasketPartial_(XccyMcBasketPartialSnapshot_ snapshot, Handle_<CalibrationReport_> report);
        friend struct XccyMcBasketInternal::Producer_;

    public:
        [[nodiscard]] const XccyMcBasketPartialSnapshot_& Snapshot() const { return snapshot_; }
        [[nodiscard]] const Handle_<CalibrationReport_>& Report() const { return report_; }
    };
    class BASE_EXPORT XccyMcBasketError_ final : public CalibrationError_ {
        Handle_<XccyMcBasketPartial_> partial_;

    public:
        XccyMcBasketError_(const std::string& file,
                           long line,
                           const std::string& function,
                           std::shared_ptr<const CalibrationFailure_> failure,
                           Handle_<XccyMcBasketPartial_> partial);
        [[nodiscard]] const Handle_<XccyMcBasketPartial_>& Partial() const noexcept { return partial_; }
    };
    class XccyMcBasketResult_ final {
        std::shared_ptr<const XccyMcBasketSnapshot_> snapshot_;
        Handle_<CalibrationReport_> report_;
        XccyMcBasketResult_(XccyMcBasketSnapshot_ snapshot, Handle_<CalibrationReport_> report);
        friend struct XccyMcBasketInternal::Producer_;

    public:
        [[nodiscard]] const XccyMcBasketSnapshot_& Snapshot() const { return *snapshot_; }
        [[nodiscard]] const Handle_<CalibrationReport_>& Report() const { return report_; }
    };
    class PreparedXccyMcBasket_ {
        struct Impl_;
        std::shared_ptr<const Impl_> impl_;
        friend struct XccyMcBasketInternal::TestAccess_;

    public:
        PreparedXccyMcBasket_(const Vector_<XccyMcBasketMember_>& members,
                              const CalibrationPreparationContext_& context,
                              const Handle_<XccyGaussianModelData_>& model,
                              const XccyMcSettings_& settings);
        [[nodiscard]] const XccyMcBasketAllocation_& Allocation() const;
        [[nodiscard]] const String_& PlanIdentity() const;
        [[nodiscard]] XccyMcBasketResult_ Evaluate(const Handle_<XccyGaussianModelData_>& candidate = {}) const;
    };
    void RequireXccyMcBasketIndependent(const XccyMcBasketResult_& first, const XccyMcBasketResult_& second, bool commonRandomNumbers = false);
} // namespace Dal
