//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <utility>

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

        using Base::curEvt_;
        using Base::dStack_;
        using Base::scenario_;
        using Base::variables_;
        using Base::Visit;
        using Base::VisitNode;

        FuzzyEvaluator_(const Vector_<>& variables,
                        const Vector_<T>& constVariables,
                        const size_t maxNestedIfs,
                        const double defEps = 0,
                        const Vector_<size_t>& vectorCapacities = {})
            : Base(variables, constVariables, vectorCapacities), defEps_(defEps), varStore0_(maxNestedIfs), varStore1_(maxNestedIfs),
              nestedIfLvl_(0) {
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
            : Base(std::move(rhs)), defEps_(rhs.defEps_), varStore0_(std::move(rhs.varStore0_)), varStore1_(std::move(rhs.varStore1_)),
              nestedIfLvl_(0) {}
        FuzzyEvaluator_& operator = (FuzzyEvaluator_&& rhs) noexcept {
            Base::operator=(std::move(rhs));
            defEps_ = rhs.defEps_;
            varStore0_ = std::move(rhs.varStore0_);
            varStore1_ = std::move(rhs.varStore1_);
            nestedIfLvl_ = 0;
            return *this;
        }

        FORCE_INLINE void SetDefEps(double defEps) { defEps_ = defEps; }

        //  Engaged only while the LSMC driver replays a recording path (fuzzy AAD)
        LsmcFuzzySinks_<T>* lsmcFuzzySinks_ = nullptr;

        void EvalTrueBranch(const NodeIf_& node, size_t lastTrueStat) {
            for (size_t i = 1; i <= lastTrueStat; ++i)
                VisitNode(*node.arguments_[i]);
        }

        void EvalFalseBranch(const NodeIf_& node) {
            if (node.HasElse())
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
            REQUIRE(nestedIfLvl_ > 0 && nestedIfLvl_ <= varStore0_.size(), "fuzzy If nesting exceeds allocated var stores");
            const size_t lvl = nestedIfLvl_ - 1;
            StoreAffectedVars(node, lvl);
            if (lsmcFuzzySinks_)
                lsmcFuzzySinks_->SnapshotBranchPayment(lvl);
            EvalTrueBranch(node, lastTrueStat);
            CaptureTrueBranchVars(node, lvl);
            if (lsmcFuzzySinks_)
                lsmcFuzzySinks_->CaptureBranchPayment(lvl);
            EvalFalseBranch(node);
            BlendAffectedVars(node, lvl, dt);
            if (lsmcFuzzySinks_)
                lsmcFuzzySinks_->BlendBranchPayment(lvl, dt);
        }

        void Visit(const NodeIf_& node) {
            const size_t lastTrueStat = node.LastTrueIndex();

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

        //  LSMC recording: with no sinks installed the plain fuzzy arithmetic runs; with
        //  them the raw payment lands in the driver's row while the payoff variable keeps
        //  the exact accumulated arithmetic of the base evaluator. Payments inside a
        //  fuzzy branch are blended by the branch degree via the sinks' snapshots.
        FORCE_INLINE void Visit(const NodePays_& node) {
            if (!lsmcFuzzySinks_) {
                Base::Visit(node);
                return;
            }
            const auto varIdx = Downcast<NodeVar_>(node.arguments_[0])->index_;
            VisitNode(*node.arguments_[1]);
            const T payment = dStack_.TopAndPop();
            if (static_cast<size_t>(varIdx) == lsmcFuzzySinks_->payoffIdx_)
                (*lsmcFuzzySinks_->pays_)[lsmcFuzzySinks_->eventOrdinal_] += payment;
            variables_[varIdx] += payment / (*scenario_)[curEvt_].numeraire_;
        }

        //  EXERCISE leaves the script state untouched; the driver's rows receive the live
        //  exercise value and the fuzzy condition degree (1 when unconditional)
        void Visit(const NodeExercise_& node) {
            REQUIRE2(lsmcFuzzySinks_,
                     "UnsupportedExecutionMode: EXERCISE statements require the LSMC simulation driver; " + node.source_.Describe(), ScriptError_);
            VisitNode(*node.arguments_[0]);
            const T value = dStack_.TopAndPop();
            T cond(1.0);
            if (node.arguments_.size() > 1) {
                VisitNode(*node.arguments_[1]);
                cond = fuzzyStack_.TopAndPop();
            }
            const size_t slot = (*lsmcFuzzySinks_->eventToExercise_)[lsmcFuzzySinks_->eventOrdinal_];
            (*lsmcFuzzySinks_->h_)[slot] = value;
            (*lsmcFuzzySinks_->cond_)[slot] = cond;
        }
    };
} // namespace Dal::Script
