#pragma once

#include <dal/curve/curveparameterization.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/script/calibrationpreparation.hpp>

namespace Dal {
    struct CurveScriptRoute_ {
        String_ id_;
        bool projection_ = false;
        CollateralType_ collateral_ = CollateralType_::Value_::OIS;
        PeriodLength_ tenor_;
    };

    template <class T_> class CandidateCurveProvider_ {
        CurveCalibrationSpec_ geometry_;
        Vector_<CurveScriptRoute_> routes_;
        std::shared_ptr<Tape::DiscountCurve_<T_>> target_;

        [[nodiscard]] const CurveScriptRoute_& Route(const String_& id, bool projection) const;
        [[nodiscard]] T_ Discount(const CurveScriptRoute_& route, const Date_& from, const Date_& to) const;

    public:
        CandidateCurveProvider_(const CurveCalibrationSpec_& geometry, const Vector_<CurveScriptRoute_>& routes, const Vector_<T_>& parameters);
        [[nodiscard]] T_ Discount(const String_& route, const Date_& from, const Date_& to) const;
        [[nodiscard]] T_ Forward(const String_& route, const Date_& start, const Date_& end, const DayBasis_& basis) const;
        [[nodiscard]] const std::shared_ptr<Tape::DiscountCurve_<T_>>& Target() const { return target_; }
    };

    struct CurveInstrumentGeometry_ {
        Date_ today_;
        String_ ccy_, curveName_ = "calibrated";
        Vector_<Date_> knotDates_;
        CurveParameterization_ parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        CurveKnotPolicy_ knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        LogDfScheme_ logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;
        Handle_<DiscountCurve_> baseCurve_;
        bool calibrateDiscountCurve_ = true;
        CollateralType_ targetCollateral_ = CollateralType_::Value_::OIS;
        PeriodLength_ targetTenor_;
        DayBasis_ liborBasis_ = DayBasis::Act365F();
    };
    struct CurveInstrumentMarket_ {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
        Vector_<CurveScriptRoute_> routes_;
    };
    struct CurveInstrumentSolver_ {
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
        double initialGuess_ = .05, smoothingWeight_ = 1.;
        Vector_<> initialGuessPerNode_;
        int maxEvaluations_ = 200, maxRestarts_ = 20;
        Vector_<> parameterScales_;
        std::optional<double> fitWeight_, fitNormTolerance_;
    };
    struct CurveInstrumentCalibrationSpec_ {
        CurveInstrumentGeometry_ geometry_;
        CurveInstrumentMarket_ market_;
        Script::ScriptValuationSettings_ valuation_;
        Vector_<Handle_<CalibrationObservation_>> observations_;
        CurveInstrumentSolver_ solver_;
    };
    struct CurveInstrumentCalibrationOptions_ {
        CalibrationEvaluation_ evaluation_ = CalibrationEvaluation_::Value_::AutoFast;
        CalibrationDerivative_ jacobianMode_ = CalibrationDerivative_::Value_::FiniteDifference;
        double differenceStep_ = 1e-6;
        bool computeQuoteJacobian_ = false;
    };
    struct CurveInstrumentCalibrationResult_ {
        Handle_<DiscountCurve_> curve_;
        Handle_<CalibrationReport_> report_;
        std::shared_ptr<const CurveInstrumentCalibrationSpec_> resolvedSpec_;
        CalibrationObservationAxes_ axes_;
        Vector_<CalibrationPreparationCapability_> capabilities_;
        CurveDefinition_ definition_;
        Vector_<CurveFreeParameter_> freeParameters_;
        Vector_<> initialParameters_, parameterScales_, parameters_, coordinates_, modelMeasures_, rawResiduals_, scaledResiduals_;
        Matrix_<> nativeWeights_, weights_, quoteJacobian_;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::Value_::EXACT;
        std::optional<double> fitWeight_, fitNormTolerance_;
        double rawNorm_ = 0., scaledNorm_ = 0., maxAbsScaledResidual_ = 0.;
        int evaluations_ = 0, gradients_ = 0;
        int diagnosticEvaluations_ = 0, diagnosticGradients_ = 0;
        bool fitWithinTolerance_ = false;
        String_ termination_, planIdentity_, candidateIdentity_, objectiveIdentity_;
    };
    class CurveInstrumentCalibrationCandidate_ final : public Storable_ {
        CurveInstrumentCalibrationResult_ result_;

    public:
        explicit CurveInstrumentCalibrationCandidate_(CurveInstrumentCalibrationResult_ result);
        [[nodiscard]] const CurveInstrumentCalibrationResult_& Result() const { return result_; }
        void Write(Archive::Store_&) const override;
    };
    class PreparedCurveInstruments_ {
        CurveInstrumentCalibrationSpec_ spec_;
        CurveInstrumentCalibrationOptions_ options_;
        CurveDefinition_ definition_;
        CalibrationObservationAxes_ axes_;
        Vector_<PreparedCalibrationInstrument_> plans_;
        Vector_<CalibrationPreparationCapability_> capabilities_;
        std::shared_ptr<const CurveInstrumentCalibrationSpec_> frozenSpec_;
        String_ planIdentity_;

    public:
        explicit PreparedCurveInstruments_(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options = {});
        [[nodiscard]] const CurveDefinition_& Definition() const { return definition_; }
        [[nodiscard]] const CalibrationObservationAxes_& Axes() const { return axes_; }
        [[nodiscard]] const CurveInstrumentCalibrationSpec_& Spec() const { return spec_; }
        [[nodiscard]] CurveInstrumentCalibrationResult_ Describe() const;
        [[nodiscard]] CurveInstrumentCalibrationResult_ Evaluate(const Vector_<>& parameters, bool derivatives = true) const;
        [[nodiscard]] std::unique_ptr<Underdetermined::Jacobian_> Jacobian(const Vector_<>& parameters) const;
    };
    CurveInstrumentCalibrationResult_ CalibrateCurveInstruments(const CurveInstrumentCalibrationSpec_& spec,
                                                                const CurveInstrumentCalibrationOptions_& options = {});
    struct MultiCurveInstrumentCalibrationSpec_ {
        Vector_<CurveInstrumentCalibrationSpec_> stages_;
    };
    struct MultiCurveInstrumentCalibrationResult_ {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
        Vector_<CurveInstrumentCalibrationResult_> stages_;
    };
    MultiCurveInstrumentCalibrationResult_ CalibrateCurveInstruments(const MultiCurveInstrumentCalibrationSpec_& spec,
                                                                     const CurveInstrumentCalibrationOptions_& options = {});

    struct LegacyCurveObjectiveMapping_ {
        CurveCalibrationSpec_ nativeSpec_;
        CurveCalibrationOptions_ nativeOptions_;
        int rows_ = 0;
        double legacyTau_ = 0., legacyFitTolerance_ = 0., fitNormTolerance_ = 0.;
        std::optional<double> fitWeight_;
    };
    LegacyCurveObjectiveMapping_ ImportLegacyCurveObjective(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options = {});
    CurveCalibrationResult_ CalibrateLegacyCurve(const LegacyCurveObjectiveMapping_& imported);
} // namespace Dal
