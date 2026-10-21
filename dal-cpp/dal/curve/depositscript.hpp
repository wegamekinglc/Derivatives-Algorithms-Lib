#pragma once

#include <dal/curve/ycinstrument.hpp>
#include <dal/math/interp/interpweights.hpp>
#include <dal/script/calibrationpreparation.hpp>

namespace Dal {
    struct DepositScriptImportSettings_ {
        String_ instrumentId_;
        Ccy_ currency_;
        String_ discountRoute_;
        double notional_ = 1., minAbsAnnuity_ = 1e-12;
    };
    Handle_<CalibrationInstrument_> NewDepositScriptInstrument(const Deposit_& native, const DepositScriptImportSettings_& settings);

    struct DepositCurveValue_ {
        CalibrationAffineCoefficients_ coefficients_;
        CalibrationMeasureValue_ price_, parRate_;
        Vector_<String_> parameterLabels_;
        String_ candidateIdentity_;
    };
    class PreparedDepositCurve_ {
        PreparedCalibrationInstrument_ pricePlan_, parPlan_;
        Vector_<Date_> nodeDates_;
        Interp::InterpWeights_ maturityWeights_;

    public:
        PreparedDepositCurve_(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context);
        [[nodiscard]] const PreparedCalibrationInstrument_& PricePlan() const { return pricePlan_; }
        [[nodiscard]] DepositCurveValue_ Evaluate(const Handle_<GSRCurveData_>& candidate) const;
    };
    struct DepositCurveFit_ {
        Handle_<GSRCurveData_> curve_;
        DepositCurveValue_ value_;
        CalibrationObservationAxes_ axes_;
        double rawResidual_ = 0., scaledResidual_ = 0.;
        int evaluations_ = 0;
    };
    DepositCurveFit_ FitDepositCurveNode(const PreparedDepositCurve_& plan,
                                         const Handle_<CalibrationObservation_>& observation,
                                         const Handle_<GSRCurveData_>& initial,
                                         int freeNode,
                                         double lower,
                                         double upper,
                                         int maxEvaluations);
} // namespace Dal
