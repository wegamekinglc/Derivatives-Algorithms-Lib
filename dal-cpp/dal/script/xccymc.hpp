#pragma once

#include <dal/script/calibrationmc.hpp>
#include <dal/script/xccyobservations.hpp>

/*IF--------------------------------------------------------------------------
enumeration XccyMonteCarloPhase
    Explicit independent XCCY sampling reservation
switchable
alternative Fit
alternative Validation
alternative HeldOut
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_XccyMonteCarloPhase_enum.hpp>

    [[nodiscard]] XccyMonteCarloPhase_ ParseXccyMonteCarloPhase(const String_& token);

    struct XccyMcSettings_ {
        XccyMonteCarloPhase_ phase_;
        int batches_ = 0, pathsPerBatch_ = 0, seed_ = 1024;
        String_ streamVersion_ = "dal.mrg32-normal-paths/1";
        bool compiled_ = Script::DEFAULT_COMPILED, aad_ = false, retainPaths_ = false;
        Vector_<String_> parameterLabels_;
    };
    struct XccyMcRange_ {
        size_t firstPath_ = 0, pathEnd_ = 0, firstCoordinate_ = 0, coordinateEnd_ = 0;
    };
    struct XccyMcReservation_ {
        String_ version_ = "dal.xccy-gaussian-phase-ranges/1", identity_, mappingIdentity_;
        XccyMonteCarloPhase_ phase_;
        size_t reservedFirst_ = 0, reservedEnd_ = 0, dimension_ = 0;
        int seed_ = 0;
        Vector_<XccyMcRange_> ranges_;
    };
    [[nodiscard]] XccyMcReservation_ PrepareXccyMcReservation(const XccyMcSettings_& settings, const CalibrationMcMapping_& mapping);
    void RequireXccyMcIndependent(const XccyMcReservation_& first, const XccyMcReservation_& second, bool commonRandomNumbers = false);

    struct XccyMcResult_ {
        String_ version_ = "dal.xccy-gaussian-mc-result/1", ruleId_ = "dal.xccy-gaussian-script-mc/1";
        String_ modelIdentity_;
        CalibrationMcMapping_ mapping_;
        XccyMcReservation_ reservation_;
        CalibrationMcResult_ estimate_;
        CalibrationAffineCoefficients_ coefficients_;
        Matrix_<> coefficientCovariance_;
        Vector_<CalibrationAffineCoefficients_> pathCoefficients_;
        Vector_<Vector_<>> firstNormals_;
        size_t consumedCoordinates_ = 0;
        Handle_<CalibrationReport_> report_;
    };
    struct XccyMcValidation_ {
        CalibrationMcValidation_ validation_;
        bool gradientCombinedPassed_ = false;
        Vector_<> gradientErrorIndicators_;
    };
    [[nodiscard]] XccyMcValidation_
    ValidateXccyMc(const XccyMcResult_& value, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision);
    class PreparedXccyMc_ {
        struct Impl_;
        std::shared_ptr<const Impl_> impl_;

    public:
        PreparedXccyMc_(const Handle_<CalibrationInstrument_>& instrument,
                        const String_& measure,
                        const CalibrationPreparationContext_& context,
                        const Handle_<XccyGaussianModelData_>& model,
                        const XccyMcSettings_& settings);
        [[nodiscard]] const CalibrationMcMapping_& Mapping() const;
        [[nodiscard]] const XccyMcReservation_& Reservation() const;
        [[nodiscard]] const String_& PlanIdentity() const;
        [[nodiscard]] XccyMcResult_ Evaluate(const Handle_<XccyGaussianModelData_>& candidate = {}) const;
    };
} // namespace Dal
