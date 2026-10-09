//
// Created by Codex on 2026/10/9.
//

#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include <dal/script/detail/segmentedstate.hpp>

namespace Dal::Script::Detail {
    struct SegmentedEventTrace_ {
        Vector_<size_t> offsets_;
        Vector_<size_t> widths_;
        size_t words_ = 0;
    };

    struct SegmentedTraceView_ {
        const SegmentedEventTrace_& event_;
        size_t base_;
        Vector_<std::uint64_t>* values_;
    };

    class SegmentedTracePlan_ {
        Vector_<SegmentedEventTrace_> events_;
        Vector_<size_t> bases_;
        Vector_<Vector_<size_t>> schedule_;
        size_t maximum_ = 0;

        static std::array<size_t, VectorReduce + 1> InstructionWidths() {
            std::array<size_t, VectorReduce + 1> result{};
            for (const int op : {Add,  Sub, Multi, Div, Pow,    Max2, Min2,  Spot,     Equal,   Sup,      SupEqual,  And,        Or,
                                 Sqrt, Log, Exp,   Not, UMinus, True, False, FuzzyAnd, FuzzyOr, FuzzyNot, FuzzyTrue, FuzzyFalse, Discard})
                result[op] = 1;
            for (const int op : {AddConst, SubConst, ConstSub, MultiConst, DivConst, ConstDiv, PowConst,   ConstPow,  Max2Const,       Min2Const,
                                 Var,      Const,    Assign,   Pays,       If,       ConstVar, FuzzyEqual, FuzzyComp, LoadObservation, VectorAppend})
                result[op] = 2;
            for (const int op : {AssignConst, PaysConst, PaysOn, IfElse, FuzzyEqualDiscrete, FuzzyCompDiscrete, VectorAssign})
                result[op] = 3;
            result[PaysOnConst] = 4;
            result[VectorRead] = result[VectorReduce] = 6;
            return result;
        }

        [[nodiscard]] static size_t Sum(size_t lhs, size_t rhs) {
            REQUIRE2(rhs <= std::numeric_limits<size_t>::max() - lhs, "SegmentedTrace: word count overflow", ScriptError_);
            return lhs + rhs;
        }

        [[nodiscard]] static size_t Count(const Vector_<int>& stream, size_t position) {
            REQUIRE2(position < stream.size() && stream[position] >= 0, "SegmentedTrace: truncated or negative instruction operand", ScriptError_);
            return static_cast<size_t>(stream[position]);
        }

        [[nodiscard]] static size_t FuzzyWidth(const Vector_<int>& stream, size_t pc) {
            const size_t affected = Count(stream, Sum(pc, 3));
            const size_t vectorsPosition = Sum(Sum(pc, 4), affected);
            const size_t vectors = Count(stream, vectorsPosition);
            const size_t width = Sum(Sum(5, affected), vectors);
            REQUIRE2(width <= stream.size() - pc, "SegmentedTrace: truncated fuzzy branch header", ScriptError_);
            const size_t trueEnd = Count(stream, Sum(pc, 1));
            const size_t falseEnd = Count(stream, Sum(pc, 2));
            REQUIRE2(trueEnd >= pc + width && falseEnd >= trueEnd && falseEnd <= stream.size(), "SegmentedTrace: invalid fuzzy branch targets",
                     ScriptError_);
            return width;
        }

        [[nodiscard]] static bool ScalarTrace(int op) {
            if (op >= FuzzyEqual && op <= FuzzyCompDiscrete)
                return true;
            constexpr std::array<int, 7> traced{Max2, Max2Const, Min2, Min2Const, If, IfElse, FuzzyIf};
            return std::find(traced.begin(), traced.end(), op) != traced.end();
        }

        [[nodiscard]] static size_t VectorWords(const Vector_<int>& stream, size_t pc, const Vector_<size_t>& bounds) {
            const size_t index = Count(stream, pc + 1);
            const size_t kind = Count(stream, pc + 2);
            REQUIRE2(index < bounds.size() && kind <= static_cast<size_t>(NodeVectorReduce_::Kind_::Maximum),
                     "SegmentedTrace: invalid vector reduction index or kind", ScriptError_);
            const bool extreme =
                kind == static_cast<size_t>(NodeVectorReduce_::Kind_::Minimum) || kind == static_cast<size_t>(NodeVectorReduce_::Kind_::Maximum);
            return extreme ? Sum(bounds[index], 1) : 1;
        }

        [[nodiscard]] static SegmentedEventTrace_ Build(const Vector_<int>& stream, const Vector_<size_t>& bounds) {
            static const auto widths = InstructionWidths();
            SegmentedEventTrace_ result{Vector_<size_t>(stream.size(), std::numeric_limits<size_t>::max()), Vector_<size_t>(stream.size(), 0), 0};
            size_t pc = 0;
            while (pc < stream.size()) {
                const int op = stream[pc];
                REQUIRE2(op >= 0 && op <= VectorReduce, "SegmentedTrace: unknown instruction", ScriptError_);
                const size_t width = op == FuzzyIf ? FuzzyWidth(stream, pc) : widths[op];
                REQUIRE2(width && width <= stream.size() - pc, "SegmentedTrace: unsupported or truncated instruction", ScriptError_);
                const size_t words = op == VectorReduce ? VectorWords(stream, pc, bounds) : static_cast<size_t>(ScalarTrace(op));
                if (words) {
                    result.offsets_[pc] = result.words_;
                    result.widths_[pc] = words;
                    result.words_ = Sum(result.words_, words);
                }
                pc += width;
            }
            return result;
        }

    public:
        SegmentedTracePlan_(const ScriptCompiled_& program, const Vector_<size_t>& bounds, const Vector_<size_t>& eventSamples, size_t samples)
            : bases_(eventSamples.size()), schedule_(samples) {
            REQUIRE2(program.NodeStreams().size() == eventSamples.size() && program.ConstStreams().size() == eventSamples.size(),
                     "SegmentedTrace: program and event mapping differ", ScriptError_);
            Vector_<size_t> totals(samples, 0);
            for (size_t i = 0; i < eventSamples.size(); ++i) {
                const size_t sample = eventSamples[i];
                REQUIRE2(sample < samples && (i == 0 || sample >= eventSamples[i - 1]),
                         "SegmentedTrace: event samples must be ordered within the timeline", ScriptError_);
                events_.push_back(Build(program.NodeStreams()[i], bounds));
                bases_[i] = totals[sample];
                totals[sample] = Sum(totals[sample], events_.back().words_);
                schedule_[sample].push_back(i);
                maximum_ = std::max(maximum_, totals[sample]);
            }
        }

        [[nodiscard]] size_t MaxWords() const { return maximum_; }

        [[nodiscard]] const Vector_<size_t>& Events(size_t sample) const {
            REQUIRE2(sample < schedule_.size(), "SegmentedTrace: sample is outside the timeline", ScriptError_);
            return schedule_[sample];
        }

        [[nodiscard]] SegmentedTraceView_ View(size_t event, Vector_<std::uint64_t>* values) const {
            REQUIRE2(event < events_.size(), "SegmentedTrace: event is outside the program", ScriptError_);
            REQUIRE2(values && values->size() == MaxWords(), "SegmentedTrace: storage size must equal MaxWords", ScriptError_);
            return {events_[event], bases_[event], values};
        }
    };

    template <class T_> class SegmentedCompiledPolicy_ : public DefaultCompiledPolicy_ {
        const SegmentedObservationPlan_* observations_;
        const Vector_<T_>& values_;
        SegmentedTraceView_ traceView_;

        void Write(size_t pc, size_t word, std::uint64_t code) const {
            REQUIRE2(pc < traceView_.event_.offsets_.size() && word < traceView_.event_.widths_[pc],
                     "SegmentedTrace: callback exceeds its declared instruction slots", ScriptError_);
            const size_t offset = traceView_.event_.offsets_[pc];
            REQUIRE2(offset < traceView_.values_->size() && traceView_.base_ <= traceView_.values_->size() - offset &&
                         word < traceView_.values_->size() - offset - traceView_.base_,
                     "SegmentedTrace: callback exceeds storage", ScriptError_);
            (*traceView_.values_)[traceView_.base_ + offset + word] = code;
        }

        template <class C_> [[nodiscard]] static std::uint64_t Choice(double left, double right, C_ compare) {
            return left == right ? 3 : (compare(right, left) ? 2 : 1);
        }

        [[nodiscard]] static std::uint64_t ComparisonRange(double value, double lower, double upper) {
            if (value < lower)
                return 1;
            if (value == lower)
                return 2;
            if (value < upper)
                return 3;
            return value == upper ? 4 : 5;
        }

        [[nodiscard]] static std::uint64_t EqualityRange(double value, double lower, double upper) {
            if (value < lower)
                return 1;
            if (value == lower)
                return 2;
            if (value > upper)
                return 7;
            if (value == upper)
                return 6;
            return value == 0.0 ? 4 : (value < 0.0 ? 3 : 5);
        }

    public:
        static constexpr bool trace_ = true;

        SegmentedCompiledPolicy_(const SegmentedObservationPlan_* observations, const Vector_<T_>& values, SegmentedTraceView_ trace)
            : observations_(observations), values_(values), traceView_(trace) {}

        [[nodiscard]] T_ Read(size_t request, const EvalState_<T_>&) const {
            REQUIRE2(observations_, "SegmentedObservation: compiled read requires a compact plan", ScriptError_);
            return observations_->Read(request, values_);
        }

        template <class C_> void Extremum(size_t pc, const T_& left, const T_& right, C_ compare) const {
            Write(pc, 0, Choice(AAD::Value(left), AAD::Value(right), compare));
        }

        void Branch(size_t pc, bool taken) const { Write(pc, 0, taken ? 2 : 1); }

        void FuzzyComparison(size_t pc, const T_& value, double lower, double upper, bool equality) const {
            Write(pc, 0, equality ? EqualityRange(AAD::Value(value), lower, upper) : ComparisonRange(AAD::Value(value), lower, upper));
        }

        void FuzzyBranch(size_t pc, const T_& degree) const {
            const double value = AAD::Value(degree);
            Write(pc, 0, value > 1.0 - EPSILON ? 2 : (value < EPSILON ? 1 : 3));
        }

        void VectorReduction(size_t pc, NodeVectorReduce_::Kind_ kind, const Vector_<T_>& values) const {
            Write(pc, 0, static_cast<std::uint64_t>(values.size()) + 1);
            const bool minimum = kind == NodeVectorReduce_::Kind_::Minimum;
            if (!minimum && kind != NodeVectorReduce_::Kind_::Maximum)
                return;
            if (values.empty())
                return;
            Write(pc, 1, 1);
            double current = AAD::Value(values[0]);
            for (size_t i = 1; i < values.size(); ++i) {
                const double next = AAD::Value(values[i]);
                const std::uint64_t choice = minimum ? Choice(current, next, std::less<>()) : Choice(current, next, std::greater<>());
                Write(pc, i + 1, choice);
                if (choice == 2)
                    current = next;
            }
        }
    };
} // namespace Dal::Script::Detail
