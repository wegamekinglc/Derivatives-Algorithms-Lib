//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor/evaluator.hpp>
#include <dal/script/visitor/smoothing.hpp>


namespace Dal::Script {

    template <class T> class FuzzyEvaluator_ : public EvaluatorBase_<T, FuzzyEvaluator_> {
        // Default smoothing factor for conditions that don't override it
        double defEps_;
        StaticStack_<T> fuzzyStack_;

        // Preallocated for performance. [i][j] = nested if level i, variable j.
        Vector_<Vector_<T>> varStore0_;
        Vector_<Vector_<T>> varStore1_;

        // 0: not in an if, 1: outermost if, 2: nested once, etc.
        size_t nestedIfLvl_;

        FORCE_INLINE pair<T, T> Pop2f() {
            pair<T, T> res;
            res.first = fuzzyStack_.TopAndPop();
            res.second = fuzzyStack_.TopAndPop();
            return res;
        }

    public:
        using Base = EvaluatorBase_<T, FuzzyEvaluator_>;

        using Base::dStack_;
        using Base::variables_;
        using Base::Visit;
        using Base::VisitNode;

        FuzzyEvaluator_(const Vector_<>& variables,  const Vector_<T>& constVariables, const size_t maxNestedIfs, const double defEps = 0)
            : Base(variables, constVariables), defEps_(defEps), varStore0_(maxNestedIfs), varStore1_(maxNestedIfs), nestedIfLvl_(0) {
            for (auto& varStore : varStore0_)
                varStore.Resize(variables.size());
            for (auto& varStore : varStore1_)
                varStore.Resize(variables.size());
        }

        FuzzyEvaluator_(const FuzzyEvaluator_& rhs)
            : Base(rhs), defEps_(rhs.defEps_), varStore0_(rhs.varStore0_.size()), varStore1_(rhs.varStore1_.size()),
              nestedIfLvl_(0) {
            for (auto& varStore : varStore0_)
                varStore.Resize(variables_.size());
            for (auto& varStore : varStore1_)
                varStore.Resize(variables_.size());
        }

        FuzzyEvaluator_& operator=(const FuzzyEvaluator_& rhs) {
            if (this == &rhs)
                return *this;
            Base::operator=(rhs);
            defEps_ = rhs.defEps_;
            varStore0_.Resize(rhs.varStore0_.size());
            varStore1_.Resize(rhs.varStore1_.size());
            for (auto& varStore : varStore0_)
                varStore.Resize(variables_.size());
            for (auto& varStore : varStore1_)
                varStore.Resize(variables_.size());
            nestedIfLvl_ = 0;
            return *this;
        }

        FuzzyEvaluator_(FuzzyEvaluator_&& rhs) noexcept
            : Base(move(rhs)), defEps_(rhs.defEps_), varStore0_(move(rhs.varStore0_)), varStore1_(move(rhs.varStore1_)),
              nestedIfLvl_(0) {}
        FuzzyEvaluator_& operator = (FuzzyEvaluator_&& rhs) noexcept {
            Base::operator=(move(rhs));
            defEps_ = rhs.defEps_;
            varStore0_ = move(rhs.varStore0_);
            varStore1_ = move(rhs.varStore1_);
            nestedIfLvl_ = 0;
            return *this;
        }

        FORCE_INLINE void SetDefEps(double defEps) { defEps_ = defEps; }

        void Visit(const NodeIf_& node) {
            const size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            ++nestedIfLvl_;

            VisitNode(*node.arguments_[0]);
            const T dt = fuzzyStack_.TopAndPop();

            // Absolutely true
            if (dt > 1.0 - EPSILON) {
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    VisitNode(*node.arguments_[i]);
            }
            // Absolutely false
            else if (dt < EPSILON) {
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        VisitNode(*node.arguments_[i]);
            }
            // Fuzzy: split evaluation between true and false branches, weight by dt
            else {
                for (auto idx : node.affectedVars_)
                    varStore0_[nestedIfLvl_ - 1][idx] = variables_[idx];

                for (size_t i = 1; i <= lastTrueStat; ++i)
                    VisitNode(*node.arguments_[i]);

                for (auto idx : node.affectedVars_) {
                    varStore1_[nestedIfLvl_ - 1][idx] = variables_[idx];
                    variables_[idx] = varStore0_[nestedIfLvl_ - 1][idx];
                }

                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        VisitNode(*node.arguments_[i]);

                for (auto idx : node.affectedVars_)
                    variables_[idx] = dt * varStore1_[nestedIfLvl_ - 1][idx] + (1.0 - dt) * variables_[idx];
            }

            --nestedIfLvl_;
        }

        // Conditions
        FORCE_INLINE void Visit(const NodeTrue_& node) { fuzzyStack_.Push(1.0); }
        FORCE_INLINE void Visit(const NodeFalse_& node) { fuzzyStack_.Push(0.0); }

        // Equality
        FORCE_INLINE void Visit(const NodeEqual_& node) {
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            // Discrete case: 0 is a singleton in expr's domain
            if (node.isDiscrete_)
                fuzzyStack_.Push(BFly(expr, node.lb_, node.rb_));
            // Continuous case: 0 lies inside expr's continuous domain
            else {
                // Use node's epsilon if set, otherwise fall back to default
                double eps = node.eps_ < 0 ? defEps_ : node.eps_;
                fuzzyStack_.Push(BFly(expr, eps));
            }
        }

        // Inequality (sup/supEqual share the logic)
        void VisitComp(const CompNode_& node) {
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            // Discrete case: 0 is either a singleton or outside expr's domain entirely,
            // but with subdomains on both sides (otherwise the condition is always true/false)
            if (node.isDiscrete_) {
                fuzzyStack_.Push(CSpr(expr, node.lb_, node.rb_));
            }
            // Continuous case: 0 lies inside expr's continuous domain
            else {
                const double eps = node.eps_ < 0 ? defEps_ : node.eps_;
                fuzzyStack_.Push(CSpr(expr, eps));
            }
        }

        FORCE_INLINE void Visit(const NodeSup_& node) { VisitComp(node); }
        FORCE_INLINE void Visit(const NodeSupEqual_& node) { VisitComp(node); }

        // Negation: probability complement on the fuzzy stack
        FORCE_INLINE void Visit(const NodeNot_& node) {
            VisitNode(*node.arguments_[0]);
            fuzzyStack_.Top() = 1.0 - fuzzyStack_.Top();
        }

        // Combinators: probability-style fuzzy logic
        FORCE_INLINE void Visit(const NodeAnd_& node) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            const auto args = Pop2f();
            fuzzyStack_.Push(args.first * args.second);
        }

        FORCE_INLINE void Visit(const NodeOr_& node) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            const auto args = Pop2f();
            fuzzyStack_.Push(args.first + args.second - args.first * args.second);
        }
    };
} // namespace Dal::Script
