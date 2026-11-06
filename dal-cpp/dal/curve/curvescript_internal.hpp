#pragma once

#include <array>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/compoundedoisnumerics_internal.hpp>
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
        size_t source_ = 0;
    };
    struct CalibrationCurvePayment_ {
        Date_ payment_;
        Vector_<CalibrationCurveInstruction_> program_;
        CalibrationDiagnostic_ source_;
        Vector_<CalibrationDiagnostic_> locations_;
    };
    struct CalibrationCurveObservation_ {
        String_ index_, route_;
        Date_ fixing_, start_, end_;
        bool projection_ = false;
        PeriodLength_ tenor_;
        DayBasis_ basis_ = DayBasis::Act365F();
        std::optional<double> known_;
        CalibrationDiagnostic_ source_;
        bool overnight_ = false;
        String_ fixingKey_;
        size_t tableRow_ = 0;
    };
    struct CalibrationCurveFamilyDescriptor_ {
        Vector_<CalibrationCurvePayment_> payments_;
        Vector_<CalibrationCurveObservation_> observations_;
        double coupon_ = 0.;
        bool compounded_ = false;
        int annuitySign_ = 0;
        double minAbsAnnuity_ = 0.;
    };
} // namespace Dal

namespace Dal::CurveScriptInternal {
    inline void CheckCompoundedAnnuity(const CalibrationCurveFamilyDescriptor_& descriptor, double annuity) {
        if (!descriptor.compounded_ || descriptor.payments_.empty())
            return;
        auto diagnostic = descriptor.payments_.front().source_;
        diagnostic.stage_ = CalibrationStage_::Value_::Evaluate;
        CalibrationInternal::Check(std::isfinite(annuity) && annuity * descriptor.annuitySign_ >= descriptor.minAbsAnnuity_,
                                   "InvalidCalibrationAnnuity", "/annuity", "actual finite annuity with frozen sign and minimum magnitude", "",
                                   diagnostic);
    }
    template <class T_> struct Affine_ {
        T_ a_ = T_(0.), b_ = T_(0.);
    };
    inline String_ Field(const CalibrationDiagnostic_& source) {
        return source.sourceRow_ ? "/source/rows/" + CalibrationInternal::OrdinalText(*source.sourceRow_ - 1) + "/text" : String_("/source/rows");
    }
    inline void Require(bool valid, const char* code, const CalibrationDiagnostic_& source, const char* expected) {
        if (!valid)
            CalibrationInternal::Fail(code, source.field_.value_or(Field(source)), expected, "", source);
    }
    inline void RequireNumerical(bool valid, const CalibrationDiagnostic_& source, const char* expected) {
        Require(valid, "CalibrationNumericalFailure", source, expected);
    }
    template <class T_> T_ CheckedGrowth(const T_& value, const CalibrationDiagnostic_& source, CalibrationStage_ stage) {
        if (!(std::isfinite(static_cast<double>(value)) && static_cast<double>(value) > 0.)) {
            auto diagnostic = source;
            diagnostic.stage_ = stage;
            RequireNumerical(false, diagnostic, "finite positive overnight factor and cumulative product");
        }
        return value;
    }
    template <class T_>
    T_ Binary(char operation,
              const T_& left,
              const T_& right,
              const CalibrationDiagnostic_& source,
              CalibrationStage_ stage = CalibrationStage_::Value_::Evaluate) {
        if (operation == 'p')
            return CheckedGrowth<T_>(left * right, source, stage);
        if (operation == 's') {
            const T_ value = left + right;
            CheckedGrowth(std::exp(static_cast<double>(value)), source, stage);
            return value;
        }
        if (operation == 'e')
            return CompoundedOisInternal::ExpMinusOne(left);
        if (operation == '*')
            return left * right;
        if (operation == '/') {
            RequireNumerical(static_cast<double>(right) != 0., source, "nonzero coupon denominator");
            return left / right;
        }
        const double sign = operation == '+' ? 1. : -1.;
        return left + sign * right;
    }
    template <class T_>
    Affine_<T_> Binary(char operation,
                       const Affine_<T_>& left,
                       const Affine_<T_>& right,
                       const CalibrationDiagnostic_& source,
                       CalibrationStage_ stage = CalibrationStage_::Value_::Evaluate) {
        const T_ independent = Binary(operation, left.b_, right.b_, source, stage);
        if (operation == 'e')
            return {T_(0.), independent};
        if (operation == '*' || operation == 'p')
            return {left.a_ * right.b_ + left.b_ * right.a_, independent};
        if (operation == '/')
            return {left.a_ / right.b_, independent};
        return {Binary(operation, left.a_, right.a_, source), independent};
    }
    template <class T_, class F_>
    T_ ForwardRate(const CalibrationCurveInstruction_& instruction, const F_& fixing, const CalibrationDiagnostic_& source) {
        const T_ discount = fixing(instruction.left_);
        RequireNumerical(static_cast<double>(discount) != 0., source, "nonzero coupon denominator");
        RequireNumerical(instruction.value_ != 0., source, "nonzero coupon denominator");
        return (T_(1.) / discount - T_(1.)) / instruction.value_;
    }
    inline bool IsObservation(char operation) { return operation == 'f' || operation == 'r' || operation == 'g' || operation == 'l'; }
    template <class T_, class F_>
    T_ ObservationValue(const CalibrationCurveInstruction_& instruction,
                        const F_& fixing,
                        const CalibrationDiagnostic_& source,
                        CalibrationStage_ stage) {
        if (instruction.operation_ == 'g')
            return CheckedGrowth<T_>(T_(1.) + instruction.value_ * fixing(instruction.left_), source, stage);
        if (instruction.operation_ == 'l') {
            const T_ increment = instruction.value_ * fixing(instruction.left_);
            CheckedGrowth(1. + static_cast<double>(increment), source, stage);
            return CompoundedOisInternal::LogOnePlus(increment);
        }
        if (instruction.operation_ == 'f')
            return fixing(instruction.left_);
        return ForwardRate<T_>(instruction, fixing, source);
    }
    template <class T_> struct ProgramValues_ {
        static constexpr size_t LOCAL_SIZE = 16;
        std::array<T_, LOCAL_SIZE> local_;
        Vector_<T_> overflow_;
        size_t size_ = 0;

        void Reset(size_t count) {
            size_ = 0;
            overflow_.clear();
            if (count > LOCAL_SIZE)
                overflow_.reserve(count - LOCAL_SIZE);
        }
        void Push(const T_& value) {
            if (size_ < LOCAL_SIZE)
                local_[size_] = value;
            else
                overflow_.push_back(value);
            ++size_;
        }
        const T_& operator[](size_t index) const { return index < LOCAL_SIZE ? local_[index] : overflow_[index - LOCAL_SIZE]; }
        const T_& Back() const { return size_ > LOCAL_SIZE ? overflow_.back() : local_[size_ - 1]; }
    };
    template <class T_> struct SmallProgramValues_ {
        static constexpr size_t LOCAL_SIZE = ProgramValues_<T_>::LOCAL_SIZE;
        std::array<T_, LOCAL_SIZE> values_;
        size_t size_ = 0;

        void Reset(size_t count) {
            REQUIRE(count <= LOCAL_SIZE, "small program scratch capacity exceeded");
            size_ = 0;
        }
        void Push(const T_& value) { values_[size_++] = value; }
        const T_& operator[](size_t index) const { return values_[index]; }
        const T_& Back() const { return values_[size_ - 1]; }
    };
    template <class T_, class F_, class S_>
    Affine_<T_> Evaluate(const CalibrationCurvePayment_& flow, const F_& fixing, CalibrationStage_ stage, S_* scratch) {
        auto source = flow.source_;
        source.stage_ = stage;
        auto& values = *scratch;
        values.Reset(flow.program_.size());
        for (const auto& instruction : flow.program_) {
            const auto& location = instruction.source_ ? flow.locations_[instruction.source_ - 1] : source;
            auto value = instruction.operation_ == 'k' ? Affine_<T_>{T_(1.), T_(0.)} : Affine_<T_>{T_(0.), T_(instruction.value_)};
            if (IsObservation(instruction.operation_))
                value.b_ = ObservationValue<T_>(instruction, fixing, location, stage);
            else if (instruction.operation_ != 'c' && instruction.operation_ != 'k')
                value = Binary(instruction.operation_, values[instruction.left_], values[instruction.right_], location, stage);
            values.Push(value);
        }
        const auto result = values.Back();
        RequireNumerical(std::isfinite(static_cast<double>(result.a_)) && std::isfinite(static_cast<double>(result.b_)), source,
                         "finite independent cashflow coefficients");
        return result;
    }
    template <class T_, class F_>
    Affine_<T_> Evaluate(const CalibrationCurvePayment_& flow, const F_& fixing, CalibrationStage_ stage = CalibrationStage_::Value_::Evaluate) {
        ProgramValues_<Affine_<T_>> local;
        return Evaluate<T_>(flow, fixing, stage, &local);
    }
    template <class T_, class F_, class S_>
    T_ EvaluateIndependent(const CalibrationCurvePayment_& flow, const F_& fixing, CalibrationStage_ stage, S_* scratch) {
        auto source = flow.source_;
        source.stage_ = stage;
        auto& values = *scratch;
        values.Reset(flow.program_.size());
        for (const auto& instruction : flow.program_) {
            const auto& location = instruction.source_ ? flow.locations_[instruction.source_ - 1] : source;
            T_ value = T_(0.);
            if (instruction.operation_ == 'c')
                value = T_(instruction.value_);
            else if (IsObservation(instruction.operation_))
                value = ObservationValue<T_>(instruction, fixing, location, stage);
            else if (instruction.operation_ == 'k') {
                CalibrationInternal::Check(false, "UnsupportedCalibrationMeasure", Field(source), "K-independent cashflow program", "", source);
            } else {
                value = Binary(instruction.operation_, values[instruction.left_], values[instruction.right_], location, stage);
            }
            values.Push(value);
        }
        const auto result = values.Back();
        RequireNumerical(std::isfinite(static_cast<double>(result)), source, "finite independent cashflow coefficient");
        return result;
    }
    template <class T_, class F_>
    T_ EvaluateIndependent(const CalibrationCurvePayment_& flow, const F_& fixing, CalibrationStage_ stage = CalibrationStage_::Value_::Evaluate) {
        ProgramValues_<T_> local;
        return EvaluateIndependent<T_>(flow, fixing, stage, &local);
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
    String_ ContextIdentity(const CalibrationPreparationContext_& context, const String_& base, const CalibrationDiagnostic_& diagnostic);
    bool Candidate(const CalibrationInstrument_& instrument);
    std::shared_ptr<const CalibrationCurveFamilyDescriptor_> Lower(const CalibrationInstrument_& instrument,
                                                                   const CalibrationPreparationContext_& context,
                                                                   const Vector_<const Script::NodePays_*>& payments,
                                                                   const Vector_<CalibrationDiagnostic_>& sources);
} // namespace Dal::CurveScriptInternal
