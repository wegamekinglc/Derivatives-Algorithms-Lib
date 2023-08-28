//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor/evaluator.hpp>


namespace Dal::Script {

    template <class T_>
    FORCE_INLINE T_ CSpr(const T_& x, double eps) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps)
            return T_(0.0);
        else if (x > halfEps)
            return T_(1.0);
        else
            return (x + halfEps) / eps;
    }

    template <class T_>
    FORCE_INLINE T_ CSpr(const T_& x, double lb, double rb) {
        if (x < lb)
            return T_(0.0);
        else if (x > rb)
            return T_(1.0);
        else
            return (x - lb) / (rb - lb);
    }

    template <class T_>
    FORCE_INLINE T_ BFly(const T_& x, double eps) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps || x > halfEps)
            return T_(0.0);
        else
            return (halfEps - fabs(x)) / halfEps;
    }

    template <class T_>
    FORCE_INLINE T_ BFly(const T_& x, double lb, double rb) {
        if (x < lb || x > rb)
            return T_(0.0);
        else if (x < 0.0)
            return 1.0 - x / lb;
        else
            return 1.0 - x / rb;
    }

    // The fuzzy evaluator
    template <class T> class FuzzyEvaluator_ : public EvaluatorBase_<T, FuzzyEvaluator_> {
        // Default smoothing factor for conditions that don't override it
        double defEps_;

        // Stack for the fuzzy evaluation of conditions
        StaticStack_<T> fuzzyStack_;

        // Temp storage for variables, preallocated for performance
        // [i][j] = nested if level i variable j
        Vector_<Vector_<T>> varStore0_;
        Vector_<Vector_<T>> varStore1_;

        // Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t nestedIfLvl_;

        // Pop the Top 2 numbers of the fuzzy condition stack
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

        FuzzyEvaluator_(const Vector_<>& variables,  const Vector_<T>& const_variables, const size_t maxNestedIfs, const double defEps = 0)
            : Base(variables, const_variables), defEps_(defEps), varStore0_(maxNestedIfs), varStore1_(maxNestedIfs), nestedIfLvl_(0) {
            for (auto& varStore : varStore0_)
                varStore.Resize(variables.size());
            for (auto& varStore : varStore1_)
                varStore.Resize(variables.size());
        }

        // Copy/Move
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

        // (Re)set default smoothing factor
        FORCE_INLINE void SetDefEps(double defEps) { defEps_ = defEps; }

        // Overriden visitors
        // If
        void Visit(const NodeIf_& node) {
            //	Last "if true" statement index
            const size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            //	Increase nested if level
            ++nestedIfLvl_;

            //	Visit the condition and compute its degree of truth dt
            VisitNode(*node.arguments_[0]);
            const T dt = fuzzyStack_.TopAndPop();

            //	Absolutely true
            if (dt > 1.0 - EPSILON) {
                //	Eval "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    VisitNode(*node.arguments_[i]);

            }
            //	Absolutely false
            else if (dt < EPSILON) {
                //	Eval "if false" statements if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        VisitNode(*node.arguments_[i]);
            }
            //	Fuzzy
            else {
                //	Record values of variables to be changed
                for (auto idx : node.affectedVars_)
                    varStore0_[nestedIfLvl_ - 1][idx] = variables_[idx];

                //	Eval "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    VisitNode(*node.arguments_[i]);

                //	Record and Reset values of variables to be changed
                for (auto idx : node.affectedVars_) {
                    varStore1_[nestedIfLvl_ - 1][idx] = variables_[idx];
                    variables_[idx] = varStore0_[nestedIfLvl_ - 1][idx];
                }

                //	Eval "if false" statements if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        VisitNode(*node.arguments_[i]);

                //	Set values of variables to fuzzy values
                for (auto idx : node.affectedVars_)
                    variables_[idx] = dt * varStore1_[nestedIfLvl_ - 1][idx] + (1.0 - dt) * variables_[idx];
            }

            //	Decrease nested if level
            --nestedIfLvl_;
        }

        // Conditions
        FORCE_INLINE void Visit(const NodeTrue_& node) { fuzzyStack_.Push(1.0); }
        FORCE_INLINE void Visit(const NodeFalse_& node) { fuzzyStack_.Push(0.0); }

        // Equality
        FORCE_INLINE void Visit(const NodeEqual_& node) {
            //	Evaluate expression to be compared to 0
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            //	Discrete case: 0 is a IsSingleton in expr's domain
            if (node.isDiscrete_)
                fuzzyStack_.Push(BFly(expr, node.lb_, node.rb_));
            //	Continuous case: 0 is part of expr's IsContinuous domain
            else {
                //	Effective epsilon: take default unless overwritten on the node
                double eps = node.eps_ < 0 ? defEps_ : node.eps_;

                //	Butterfly
                fuzzyStack_.Push(BFly(expr, eps));
            }
        }

        // Inequality
        // For visiting superior and supEqual
        void VisitComp(const CompNode_& node) {
            //	Evaluate expression to be compared to 0
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.TopAndPop();

            //	Discrete case:
            //	Either 0 is a IsSingleton in expr's domain
            //	Or 0 is not part of expr's domain, but expr's domain has subdomains left and right of 0
            //		otherwise the condition would be always true/false
            if (node.isDiscrete_) {
                //	Call spread on the right
                fuzzyStack_.Push(CSpr(expr, node.lb_, node.rb_));
            }
            //	Continuous case: 0 is part of expr's IsContinuous domain
            else {
                //	Effective epsilon: take default unless overwritten on the node
                const double eps = node.eps_ < 0 ? defEps_ : node.eps_;

                //	Call Spread
                fuzzyStack_.Push(CSpr(expr, eps));
            }
        }

        FORCE_INLINE void Visit(const NodeSup_& node) { VisitComp(node); }
        FORCE_INLINE void Visit(const NodeSupEqual_& node) { VisitComp(node); }

        // Negation
        FORCE_INLINE void visitNot(const NodeNot_& node) {
            VisitNode(*node.arguments_[0]);
            fuzzyStack_.Top() = 1.0 - fuzzyStack_.Top();
        }

        // Combinators
        // Hard coded proba stlye and->dt(lhs)*dt(rhs), or->dt(lhs)+dt(rhs)-dt(lhs)*dt(rhs)
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
