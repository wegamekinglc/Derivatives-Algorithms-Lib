#pragma once

#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyscript.hpp>

namespace Dal {
    struct XccyGaussianDynamics_;
    struct XccyGaussianModelData_;
    struct XccyInstrumentCalibrationSpec_ {
        XccyScriptContext_ context_;
        Handle_<CurveBlock_> domesticCurveBlock_, foreignCurveBlock_;
        double fxSpot_ = 0.;
        Vector_<Date_> knotDates_;
        Vector_<Handle_<CalibrationObservation_>> observations_;
        CurveInstrumentSolver_ solver_;
        Handle_<XccyGaussianDynamics_> gaussianDynamics_;
        CurveParameterization_ basisParameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;

        XccyInstrumentCalibrationSpec_() { solver_.initialGuess_ = 0.; }
    };
    struct XccyInstrumentCalibrationResult_ : CurveInstrumentCalibrationResult_ {
        std::shared_ptr<const XccyInstrumentCalibrationSpec_> xccySpec_;
        Handle_<XccyGaussianModelData_> gaussianModel_;
    };
    class XccyInstrumentCalibrationCandidate_ final : public Storable_ {
        XccyInstrumentCalibrationResult_ result_;

    public:
        explicit XccyInstrumentCalibrationCandidate_(XccyInstrumentCalibrationResult_ result);
        [[nodiscard]] const XccyInstrumentCalibrationResult_& Result() const { return result_; }
        void Write(Archive::Store_&) const override;
    };
    class PreparedXccyInstruments_ {
        XccyInstrumentCalibrationSpec_ spec_;
        CurveInstrumentCalibrationOptions_ options_;
        CurveDefinition_ definition_;
        CalibrationObservationAxes_ axes_;
        Vector_<PreparedCalibrationInstrument_> plans_;
        std::shared_ptr<const XccyInstrumentCalibrationSpec_> frozenSpec_;
        String_ planIdentity_;

    public:
        explicit PreparedXccyInstruments_(const XccyInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options = {});
        [[nodiscard]] const CurveDefinition_& Definition() const { return definition_; }
        [[nodiscard]] const CalibrationObservationAxes_& Axes() const { return axes_; }
        [[nodiscard]] XccyInstrumentCalibrationResult_ Describe() const;
        [[nodiscard]] XccyInstrumentCalibrationResult_ Evaluate(const Vector_<>& parameters, bool derivatives = true) const;
        [[nodiscard]] Vector_<> Residuals(const Vector_<>& parameters) const;
        [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Jacobian(const Vector_<>& parameters) const;
    };
    XccyInstrumentCalibrationResult_ CalibrateXccyInstruments(const XccyInstrumentCalibrationSpec_& spec,
                                                              const CurveInstrumentCalibrationOptions_& options = {});
} // namespace Dal
