//
// Created by Codex on 2026/10/16.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Dal::ReplayStudy {
    // Opcodes are private bytecode operands, not a public DAL enumeration.
    enum class Op_ : uint8_t { LEAF, ADD, SUB, MUL, DIV, EXP, LOG, MAX, MIN, SCRIPT_MAX, SCRIPT_MIN, GUARD };

    struct Operand_ {
        size_t slot_ = std::numeric_limits<size_t>::max();
        double literal_ = 0.0;
    };

    struct Node_ {
        Op_ op_;
        Operand_ left_, right_;
        double lower_ = 0.0, upper_ = 0.0;
        size_t source_ = std::numeric_limits<size_t>::max();
        int region_ = 0;
    };

    struct State_ {
        double value_ = 0.0, left_ = 0.0, right_ = 0.0, adjoint_ = 0.0;
    };

    class Graph_ {
        std::vector<Node_> nodes_;
        std::vector<State_> states_;
        size_t prefix_ = 0;
        bool ready_ = false;
        uint64_t generation_ = 0;
        uint64_t suffixEpoch_ = 0;

        [[nodiscard]] double Read(const Operand_& operand) const {
            return operand.slot_ == std::numeric_limits<size_t>::max() ? operand.literal_ : states_.at(operand.slot_).value_;
        }

        static void Arithmetic(Op_ op, double l, double r, State_* state) {
            switch (op) {
            case Op_::ADD:
                *state = {l + r, 1.0, 1.0, 0.0};
                break;
            case Op_::SUB:
                *state = {l - r, 1.0, -1.0, 0.0};
                break;
            case Op_::MUL:
                *state = {l * r, r, l, 0.0};
                break;
            case Op_::DIV:
                *state = {l / r, 1.0 / r, -l / r / r, 0.0};
                break;
            default:
                throw std::runtime_error("invalid arithmetic opcode");
            }
        }

        static void Unary(Op_ op, double l, State_* state) {
            switch (op) {
            case Op_::EXP:
                state->value_ = std::exp(l);
                state->left_ = state->value_;
                break;
            case Op_::LOG:
                state->value_ = std::log(l);
                state->left_ = 1.0 / l;
                break;
            default:
                throw std::runtime_error("invalid unary opcode");
            }
        }

        static void Selector(Op_ op, double l, double r, State_* state) {
            switch (op) {
            case Op_::MAX:
                *state = {std::max(l, r), double(l > r), double(r > l), 0.0};
                break;
            case Op_::MIN:
                *state = {std::min(l, r), double(l < r), double(r < l), 0.0};
                break;
            case Op_::SCRIPT_MAX:
                *state = {std::max(l, r), double(l >= r), double(r > l), 0.0};
                break;
            case Op_::SCRIPT_MIN:
                *state = {std::min(l, r), double(l <= r), double(r < l), 0.0};
                break;
            default:
                throw std::runtime_error("invalid selector opcode");
            }
        }

        static bool Finite(const State_& state) { return std::isfinite(state.value_) && std::isfinite(state.left_) && std::isfinite(state.right_); }

        static bool EvaluateGuard(const Node_& node, double l, double r, State_* state) {
            state->value_ = l - r;
            return std::isfinite(state->value_) && Region(state->value_, node.lower_, node.upper_) == node.region_;
        }

        bool Evaluate(size_t slot, const std::vector<double>& frame) {
            const auto& node = nodes_[slot];
            auto& state = states_[slot];
            const double l = Read(node.left_), r = Read(node.right_);
            state.left_ = state.right_ = 0.0;
            // Contiguous private opcode groups keep each evaluator small.
            if (node.op_ == Op_::LEAF)
                state.value_ = node.source_ == std::numeric_limits<size_t>::max() ? l : frame.at(node.source_);
            else if (node.op_ <= Op_::DIV)
                Arithmetic(node.op_, l, r, &state);
            else if (node.op_ <= Op_::LOG)
                Unary(node.op_, l, &state);
            else if (node.op_ <= Op_::SCRIPT_MIN)
                Selector(node.op_, l, r, &state);
            else
                return EvaluateGuard(node, l, r, &state);
            return Finite(state);
        }

        void AddAdjoint(const Operand_& operand, double value) {
            if (operand.slot_ != std::numeric_limits<size_t>::max())
                states_[operand.slot_].adjoint_ += value;
        }

    public:
        [[nodiscard]] static int Region(double value, double lower, double upper) { return value < lower ? -1 : value > upper ? 1 : 0; }
        void Reset() {
            nodes_.clear();
            states_.clear();
            prefix_ = 0;
            ready_ = false;
            ++generation_;
        }
        [[nodiscard]] uint64_t Generation() const { return generation_; }
        [[nodiscard]] uint64_t SuffixEpoch() const { return suffixEpoch_; }
        [[nodiscard]] size_t Size() const { return nodes_.size(); }
        [[nodiscard]] size_t Prefix() const { return prefix_; }
        [[nodiscard]] bool Ready() const { return ready_; }
        [[nodiscard]] size_t OccupiedBytes() const { return nodes_.size() * sizeof(Node_) + states_.size() * sizeof(State_); }
        [[nodiscard]] size_t CapacityBytes() const { return nodes_.capacity() * sizeof(Node_) + states_.capacity() * sizeof(State_); }
        [[nodiscard]] double Value(size_t slot) const { return states_.at(slot).value_; }
        [[nodiscard]] double Adjoint(size_t slot) const { return states_.at(slot).adjoint_; }
        void Mark() { prefix_ = nodes_.size(); }
        void Restore() {
            nodes_.resize(prefix_);
            states_.resize(prefix_);
            ready_ = false;
            ++suffixEpoch_;
        }
        size_t Append(const Node_& node, const std::vector<double>& frame) {
            ready_ = false;
            const size_t slot = nodes_.size();
            if ((node.left_.slot_ != std::numeric_limits<size_t>::max() && node.left_.slot_ >= slot) ||
                (node.right_.slot_ != std::numeric_limits<size_t>::max() && node.right_.slot_ >= slot))
                throw std::runtime_error("non-topological operand");
            nodes_.push_back(node);
            states_.emplace_back();
            if (!Evaluate(slot, frame))
                throw std::runtime_error("invalid recording value or derivative");
            return slot;
        }
        bool Replay(const std::vector<double>& frame, bool includePrefix = false) {
            ready_ = false;
            for (size_t i = includePrefix ? 0 : prefix_; i < nodes_.size(); ++i) {
                if (!Evaluate(i, frame))
                    return false;
            }
            ready_ = true;
            return true;
        }
        void Finish() { ready_ = true; }
        void Reverse(size_t root, bool suffixOnly = false) {
            if (!ready_ || root >= states_.size())
                throw std::runtime_error("reverse requires a valid completed graph");
            const size_t first = suffixOnly ? prefix_ : 0;
            for (size_t i = first; i < states_.size(); ++i)
                states_[i].adjoint_ = 0.0;
            states_[root].adjoint_ = 1.0;
            Propagate(first, nodes_.size());
        }
        void ReversePrefix() {
            if (!ready_)
                throw std::runtime_error("prefix reverse requires a valid graph");
            Propagate(0, prefix_);
        }

    private:
        void Propagate(size_t first, size_t end) {
            for (size_t i = end; i-- > first;) {
                const auto& state = states_[i];
                if (state.adjoint_ == 0.0)
                    continue;
                AddAdjoint(nodes_[i].left_, state.adjoint_ * state.left_);
                AddAdjoint(nodes_[i].right_, state.adjoint_ * state.right_);
            }
        }
    };

    struct Scalar_ {
        Graph_* graph_;
        const std::vector<double>* frame_;
        size_t slot_;
        uint64_t generation_;
        uint64_t suffixEpoch_;

        [[nodiscard]] Operand_ Operand() const {
            if (generation_ != graph_->Generation() || slot_ >= graph_->Size() ||
                (slot_ >= graph_->Prefix() && suffixEpoch_ != graph_->SuffixEpoch()))
                throw std::runtime_error("stale graph scalar");
            return {slot_, 0.0};
        }
        [[nodiscard]] double Value() const { return graph_->Value(Operand().slot_); }
    };

    inline Scalar_ Leaf(Graph_* graph, const std::vector<double>* frame, double value, size_t source = std::numeric_limits<size_t>::max()) {
        return {graph, frame, graph->Append({Op_::LEAF, {std::numeric_limits<size_t>::max(), value}, {}, 0.0, 0.0, source}, *frame),
                graph->Generation(), graph->SuffixEpoch()};
    }

    inline Scalar_ Apply(Op_ op, const Scalar_& left, Operand_ right = {}) {
        return {left.graph_, left.frame_, left.graph_->Append({op, left.Operand(), right}, *left.frame_), left.graph_->Generation(),
                left.graph_->SuffixEpoch()};
    }
    inline Scalar_ Apply(Op_ op, double left, const Scalar_& right) {
        return {right.graph_, right.frame_, right.graph_->Append({op, {std::numeric_limits<size_t>::max(), left}, right.Operand()}, *right.frame_),
                right.graph_->Generation(), right.graph_->SuffixEpoch()};
    }
    inline Operand_ CheckedOperand(const Scalar_& left, const Scalar_& right) {
        if (left.graph_ != right.graph_)
            throw std::runtime_error("foreign graph scalar");
        return right.Operand();
    }

    inline Scalar_ operator+(const Scalar_& l, const Scalar_& r) { return Apply(Op_::ADD, l, CheckedOperand(l, r)); }
    inline Scalar_ operator-(const Scalar_& l, const Scalar_& r) { return Apply(Op_::SUB, l, CheckedOperand(l, r)); }
    inline Scalar_ operator*(const Scalar_& l, const Scalar_& r) { return Apply(Op_::MUL, l, CheckedOperand(l, r)); }
    inline Scalar_ operator/(const Scalar_& l, const Scalar_& r) { return Apply(Op_::DIV, l, CheckedOperand(l, r)); }
    inline Scalar_ operator+(const Scalar_& l, double r) { return Apply(Op_::ADD, l, {std::numeric_limits<size_t>::max(), r}); }
    inline Scalar_ operator-(const Scalar_& l, double r) { return Apply(Op_::SUB, l, {std::numeric_limits<size_t>::max(), r}); }
    inline Scalar_ operator*(const Scalar_& l, double r) { return Apply(Op_::MUL, l, {std::numeric_limits<size_t>::max(), r}); }
    inline Scalar_ operator/(const Scalar_& l, double r) { return Apply(Op_::DIV, l, {std::numeric_limits<size_t>::max(), r}); }
    inline Scalar_ operator+(double l, const Scalar_& r) { return Apply(Op_::ADD, l, r); }
    inline Scalar_ operator-(double l, const Scalar_& r) { return Apply(Op_::SUB, l, r); }
    inline Scalar_ operator*(double l, const Scalar_& r) { return Apply(Op_::MUL, l, r); }
    inline Scalar_ exp(const Scalar_& x) { return Apply(Op_::EXP, x); }
    inline Scalar_ log(const Scalar_& x) { return Apply(Op_::LOG, x); }
    inline Scalar_ max(const Scalar_& l, const Scalar_& r) { return Apply(Op_::MAX, l, CheckedOperand(l, r)); }
    inline Scalar_ min(const Scalar_& l, const Scalar_& r) { return Apply(Op_::MIN, l, CheckedOperand(l, r)); }
    inline Scalar_ max(const Scalar_& l, double r) { return Apply(Op_::MAX, l, {std::numeric_limits<size_t>::max(), r}); }

    inline int Guard(const Scalar_& left, const Scalar_& right, double lower, double upper) {
        const int region = Graph_::Region(left.Value() - right.Value(), lower, upper);
        left.graph_->Append({Op_::GUARD, left.Operand(), CheckedOperand(left, right), lower, upper, std::numeric_limits<size_t>::max(), region},
                            *left.frame_);
        return region;
    }
} // namespace Dal::ReplayStudy
