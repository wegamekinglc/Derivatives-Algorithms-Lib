#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/calibrationpreparation.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal::Script {
    struct NodePays_;
    struct NodeFix_;
} // namespace Dal::Script

namespace Dal {
    struct CalibrationCurveInstruction_ {
        char operation_ = 'c';
        size_t left_ = 0, right_ = 0;
        double value_ = 0.;
        bool dependent_ = false;
    };
    struct CalibrationCurvePayment_ {
        Date_ payment_;
        Vector_<CalibrationCurveInstruction_> program_;
        CalibrationDiagnostic_ source_;
    };
    struct CalibrationCurveObservation_ {
        String_ index_, route_;
        Date_ fixing_, start_, end_;
        bool projection_ = false;
        PeriodLength_ tenor_;
        DayBasis_ basis_ = DayBasis::Act365F();
        std::optional<double> known_;
        CalibrationDiagnostic_ source_;
    };
    struct CalibrationCurveFamilyDescriptor_ {
        Vector_<CalibrationCurvePayment_> payments_;
        Vector_<CalibrationCurveObservation_> observations_;
        double coupon_ = 0.;
    };
} // namespace Dal

namespace Dal::CurveScriptInternal {
    template <class T_> struct Affine_ {
        T_ a_ = T_(0.), b_ = T_(0.);
    };
    inline String_ Field(const CalibrationDiagnostic_& source) {
        return source.sourceRow_ ? "/source/rows/" + CalibrationInternal::OrdinalText(*source.sourceRow_ - 1) + "/text" : String_("/source/rows");
    }
    template <class T_> Affine_<T_> Binary(char operation, const Affine_<T_>& left, const Affine_<T_>& right, const CalibrationDiagnostic_& source) {
        if (operation == '*')
            return {left.a_ * right.b_ + left.b_ * right.a_, left.b_ * right.b_};
        if (operation == '/') {
            CalibrationInternal::Check(static_cast<double>(right.b_) != 0., "CalibrationNumericalFailure", Field(source),
                                       "nonzero coupon denominator", "", source);
            return {left.a_ / right.b_, left.b_ / right.b_};
        }
        const double sign = operation == '+' ? 1. : -1.;
        return {left.a_ + sign * right.a_, left.b_ + sign * right.b_};
    }
    template <class T_, class F_>
    Affine_<T_> Evaluate(const CalibrationCurvePayment_& flow, const F_& fixing, CalibrationStage_ stage = CalibrationStage_::Value_::Evaluate) {
        auto source = flow.source_;
        source.stage_ = stage;
        Vector_<Affine_<T_>> values;
        values.reserve(flow.program_.size());
        for (const auto& instruction : flow.program_) {
            auto value = instruction.operation_ == 'k' ? Affine_<T_>{T_(1.), T_(0.)} : Affine_<T_>{T_(0.), T_(instruction.value_)};
            if (instruction.operation_ == 'f')
                value.b_ = fixing(instruction.left_);
            else if (instruction.operation_ != 'c' && instruction.operation_ != 'k')
                value = Binary(instruction.operation_, values[instruction.left_], values[instruction.right_], source);
            CalibrationInternal::Check(std::isfinite(static_cast<double>(value.a_)) && std::isfinite(static_cast<double>(value.b_)),
                                       "CalibrationNumericalFailure", Field(source), "finite independent cashflow coefficients", "", source);
            values.push_back(value);
        }
        return values.back();
    }
    inline Affine_<double> FixedCoefficients(const CalibrationCurvePayment_& flow) {
        return Evaluate<double>(
            flow,
            [&](size_t) -> double {
                CalibrationInternal::Fail("UnsupportedCalibrationMeasure", "/discounts", "candidate provider for observed curve cashflows", "",
                                          flow.source_);
            },
            CalibrationStage_::Value_::Prepare);
    }
    String_ ProjectionRoute(const Ccy_& currency, const PeriodLength_& tenor);
    CalibrationCurveObservation_
    Observation(const Script::NodeFix_& node, const CalibrationPreparationContext_& context, const CalibrationDiagnostic_& source);
    String_ ContextIdentity(const CalibrationPreparationContext_& context, const String_& base);
    bool Candidate(const CalibrationInstrument_& instrument);
    std::shared_ptr<const CalibrationCurveFamilyDescriptor_> Lower(const CalibrationInstrument_& instrument,
                                                                   const CalibrationPreparationContext_& context,
                                                                   const Vector_<const Script::NodePays_*>& payments,
                                                                   const Vector_<CalibrationDiagnostic_>& sources);
} // namespace Dal::CurveScriptInternal
