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
            ResizeVarStores(&varStore0_, &varStore1_, variables.size());
        }

        FuzzyEvaluator_(const FuzzyEvaluator_& rhs)
            : Base(rhs), defEps_(rhs.defEps_), varStore0_(rhs.varStore0_.size()), varStore1_(rhs.varStore1_.size()),
              nestedIfLvl_(0) {
            ResizeVarStores(&varStore0_, &varStore1_, variables_.size());
        }

        FuzzyEvaluator_& operator=(const FuzzyEvaluator_& rhs) {
            if (this == &rhs)
                return *this;
            Base::operator=(rhs);
            defEps_ = rhs.defEps_;
            varStore0_.Resize(rhs.varStore0_.size());
            varStore1_.Resize(rhs.varStore1_.size());
            ResizeVarStores(&varStore0_, &varStore1_, variables_.size());
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

        void EvalTrueBranch(const NodeIf_& node, size_t lastTrueStat) {
            for (size_t i = 1; i <= lastTrueStat; ++i)
                VisitNode(*node.arguments_[i]);
        }

        void EvalFalseBranch(const NodeIf_& node) {
            if (node.firstElse_ != -1)
                for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                    VisitNode(*node.arguments_[i]);
        }

        void StoreAffectedVars(const NodeIf_& node, size_t lvl) {
            for (auto idx : node.affectedVars_)
                varStore0_[lvl][idx] = variables_[idx];
        }

        void CaptureTrueBranchVars(const NodeIf_& node, size_t lvl) {
            for (auto idx : node.affectedVars_) {
                varStore1_[lvl][idx] = variables_[idx];
                variables_[idx] = varStore0_[lvl][idx];
            }
        }

        void BlendAffectedVars(const NodeIf_& node, size_t lvl, const T& dt) {
            for (auto idx : node.affectedVars_)
                variables_[idx] = dt * varStore1_[lvl][idx] + (1.0 - dt) * variables_[idx];
        }

        void EvalFuzzyBranches(const NodeIf_& node, size_t lastTrueStat, const T& dt) {
            const size_t lvl = nestedIfLvl_ - 1;
            StoreAffectedVars(node, lvl);
            EvalTrueBranch(node, lastTrueStat);
            CaptureTrueBranchVars(node, lvl);
            EvalFalseBranch(node);
            BlendAffectedVars(node, lvl, dt);
        }

        void Visit(const NodeIf_& node) {
            const size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            ++nestedIfLvl_;

            VisitNode(*node.arguments_[0]);
            const T dt = fuzzyStack_.TopAndPop();

            if (dt > 1.0 - EPSILON)
                EvalTrueBranch(node, lastTrueStat);
            else if (dt < EPSILON)
                EvalFalseBranch(node);
            else
                EvalFuzzyBranches(node, lastTrueStat, dt);

            --nestedIfLvl_;
        }

        FORCE_INLINE void Visit(const NodeTrue_& node) { fuzzyStack_.Push(1.0); }
        FORCE_INLINE void Visit(const NodeFalse_& node) { fuzzyStack_.Push(0.0); }

        FORCE_INLINE void Visit(const NodeEqual_& node) {
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            if (node.isDiscrete_)
                fuzzyStack_.Push(BFly(expr, node.lb_, node.rb_));
            else {
                double eps = node.eps_ < 0 ? defEps_ : node.eps_;
                fuzzyStack_.Push(BFly(expr, eps));
            }
        }

        void VisitComp(const CompNode_& node) {
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            if (node.isDiscrete_) {
                fuzzyStack_.Push(CSpr(expr, node.lb_, node.rb_));
            } else {
                const double eps = node.eps_ < 0 ? defEps_ : node.eps_;
                fuzzyStack_.Push(CSpr(expr, eps));
            }
        }

        FORCE_INLINE void Visit(const NodeSup_& node) { VisitComp(node); }
        FORCE_INLINE void Visit(const NodeSupEqual_& node) { VisitComp(node); }

        FORCE_INLINE void Visit(const NodeNot_& node) {
            VisitNode(*node.arguments_[0]);
            fuzzyStack_.Top() = 1.0 - fuzzyStack_.Top();
        }

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
