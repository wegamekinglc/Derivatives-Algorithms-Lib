#pragma once

#include <variant>

#include <dal/calibration/protocol.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/script/instrument.hpp>

/*IF--------------------------------------------------------------------------
enumeration CalibrationEvaluation
    Requested script calibration execution
switchable
alternative AutoFast
alternative RequireFast
alternative GenericDeterministic
alternative ScriptMC
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationBackend
    Admitted script calibration executor
switchable
alternative FastDeterministic
alternative FastGaussian
alternative GenericDeterministic
alternative ScriptMC
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
enumeration CalibrationDerivative
    Requested or admitted native parameter derivative
switchable
alternative Auto
alternative Analytic
alternative AAD
alternative FiniteDifference
alternative None
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_CalibrationBackend_enum.hpp>
#include <dal/auto/MG_CalibrationDerivative_enum.hpp>
#include <dal/auto/MG_CalibrationEvaluation_enum.hpp>

    struct CalibrationSamplingSpec_ {
        int paths_ = 0, seed_ = 0;
        String_ generator_ = "irn";
    };
    struct CalibrationEvaluationOptions_ {
        CalibrationEvaluation_ evaluation_ = CalibrationEvaluation_::Value_::AutoFast;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::Auto;
        bool allowMonteCarloCalibration_ = false;
        std::optional<CalibrationSamplingSpec_> sampling_;
    };
    struct CalibrationPreparationContext_ {
        Script::ScriptValuationSettings_ valuation_;
        Ccy_ currency_;
        String_ discountRoute_;
        Handle_<GSRCurveData_> curve_;
        Handle_<ModelData_> model_;
        String_ conventionState_ = "dal.fixed-dates/1";
        double smoothing_ = 0.;
    };
    struct CalibrationCashflow_ {
        Date_ eventDate_;
        std::optional<double> amount_;
        double a_ = 0., b_ = 0.;
        int sourceRow_ = 0, statement_ = 0;
        String_ observationIndex_;
        std::optional<Date_> fixingDate_;
    };
    struct CalibrationDepositDescriptor_ {
        Date_ start_, maturity_;
        double notional_ = 0., coupon_ = 0., accrual_ = 0.;
    };
    struct CalibrationBondOptionDescriptor_ {
        Date_ expiry_, maturity_;
        double notional_ = 0., strike_ = 0.;
        bool call_ = true;
        String_ index_;
    };
    using CalibrationFastDescriptor_ = std::variant<CalibrationDepositDescriptor_, CalibrationBondOptionDescriptor_>;
    struct CalibrationPreparationCapability_ {
        String_ ruleId_;
        CalibrationBackend_ backend_;
        CalibrationDerivative_ derivativeMethod_;
        bool sharedPrice_ = true, affine_ = false;
    };
    struct CalibrationAffineCoefficients_ {
        double a_ = 0., b_ = 0.;
        Vector_<> da_, db_;
    };
    struct CalibrationMeasureValue_ {
        double value_ = 0.;
        Vector_<> derivatives_;
    };
    class PreparedCalibrationInstrument_ {
        Handle_<CalibrationInstrument_> instrument_;
        CalibrationMeasureSpec_ measure_;
        CalibrationPreparationCapability_ capability_;
        CalibrationFastDescriptor_ descriptor_;
        Vector_<CalibrationCashflow_> cashflows_;
        String_ compilerIdentity_, contextIdentity_, planIdentity_;
        std::optional<CalibrationSamplingSpec_> sampling_;

        PreparedCalibrationInstrument_() = default;

        friend PreparedCalibrationInstrument_ PrepareCalibrationInstrument(const Handle_<CalibrationInstrument_>&,
                                                                           const String_&,
                                                                           const CalibrationPreparationContext_&,
                                                                           const CalibrationEvaluationOptions_&);

    public:
        [[nodiscard]] const Handle_<CalibrationInstrument_>& Instrument() const { return instrument_; }
        [[nodiscard]] const CalibrationMeasureSpec_& Measure() const { return measure_; }
        [[nodiscard]] const CalibrationPreparationCapability_& Capability() const { return capability_; }
        [[nodiscard]] const CalibrationFastDescriptor_& Descriptor() const { return descriptor_; }
        [[nodiscard]] const Vector_<CalibrationCashflow_>& Cashflows() const { return cashflows_; }
        [[nodiscard]] const String_& CompilerIdentity() const { return compilerIdentity_; }
        [[nodiscard]] const String_& ContextIdentity() const { return contextIdentity_; }
        [[nodiscard]] const String_& PlanIdentity() const { return planIdentity_; }
        [[nodiscard]] const std::optional<CalibrationSamplingSpec_>& Sampling() const { return sampling_; }
        void RequireCompatible(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context) const;
        [[nodiscard]] CalibrationAffineCoefficients_ Coefficients(const Vector_<>& discounts, const Matrix_<>& discountDerivatives = {}) const;
        [[nodiscard]] CalibrationMeasureValue_ AffineMeasure(const CalibrationAffineCoefficients_& coefficients) const;
        [[nodiscard]] double Price(const Vector_<>& discounts) const;
    };
    PreparedCalibrationInstrument_ PrepareCalibrationInstrument(const Handle_<CalibrationInstrument_>& instrument,
                                                                const String_& measureId,
                                                                const CalibrationPreparationContext_& context,
                                                                const CalibrationEvaluationOptions_& options = {});
} // namespace Dal
