//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/operators.hpp>
#include <dal/script/visitor/evaluator.hpp>

namespace Dal::Script {
    template <class T_>
    class FuzzyEvaluator_: public Evaluator_<T_> {
        double eps_;
        Stack_<T_> fuzzyStack_;

        // Temp storage for variables, pre-allocated for performance [i][j] = nested if level i variable j
        Vector_<Vector_<T_>> varStore0_;
        Vector_<Vector_<T_>> varStore1_;

        size_t nestedIFLv_;

        std::pair<T_, T_> Pop2f() {
            std::pair<T_, T_> res;
            res.first = fuzzyStack_.TopAndPop();
            res.second = fuzzyStack_.TopAndPop();
            return res;
        }
        // call spread (-eps/2, +eps/2)
        static T_ CSpr(const T_& x, double eps) {
            const double halfEps = 0.5 * eps;
            if (x < - halfEps)
                return 0.0;
            else if (x > halfEps)
                return 1.0;
            else
                return (x + halfEps) / eps;
        }

        // call spread (lb, rb)
        static T_ CSpr(const T_& x, double lb, double rb) {
            if (x < lb)
                return 0.0;
            else if (x > rb)
                return 1.0;
            else
                return (x - lb) / (rb - lb);
        }

        // butterfly (-eps/2, +eps/2)
        static T_ BFly( const T_& x, double eps)
        {
            const double halfEps = 0.5 * eps;
            if (x < - halfEps || x > halfEps)
                return 0.0;
            else
                return (halfEps - Fabs(x)) / halfEps;
        }

        // butterfly (lb, 0, rb)
        static T_ BFly( const T_& x, double lb, double rb)
        {
            if( x < lb || x > rb)
                return 0.0;
            else if (x < 0.0)
                return 1.0 - x / lb;
            else
                return 1.0 - x / rb;
        }

        template<class NodeSup_>
        void VisitImpl(const NodeSup_* node)
        {
            // evaluate expression to be compared to 0
            node->arguments_[0]->AcceptVisitor(this);
            const T_ expr = Evaluator_<T_>::dStack_.TopAndPop();

            // discrete case:
            // either 0 is a singleton in expr's domain
            // or 0 is not part of expr's domain, but expr's domain has subdomains left and right of 0
            // otherwise the condition would be always true/false
            if( node->discrete_)
                fuzzyStack_.Push( CSpr(expr, node->lb_, node->ub_));
            //	Continuous case: 0 is part of expr's continuous domain
            else {
                // effective epsilon: take default unless overwritten on the node
                const double eps = node->eps_ < 0 ? eps_ : node->eps_;
                // call Spread
                fuzzyStack_.Push( CSpr(expr, eps));
            }
        }

    public:
        FuzzyEvaluator_(size_t nVar, size_t maxNestedIFs, double eps)
        : Evaluator_<T_>(nVar), eps_(eps), varStore0_(maxNestedIFs), varStore1_(maxNestedIFs), nestedIFLv_(0) {
            for (auto& store: varStore0_)
                store.Resize(nVar);
            for (auto& store: varStore1_)
                store.Resize(nVar);
        }

        FuzzyEvaluator_(const FuzzyEvaluator_& rhs)
        : Evaluator_<T_>(rhs), eps_( rhs.eps_), varStore0_( rhs.varStore0_.size()), varStore1_( rhs.varStore1_.size()), nestedIFLv_( 0) {
            for( auto& store : varStore0_) store.resize(Evaluator_<T_>::variables_.size());
            for( auto& store : varStore1_) store.resize(Evaluator_<T_>::variables_.size());
        }

        FuzzyEvaluator_& operator=(const FuzzyEvaluator_& rhs) {
            if (this == &rhs)
                return *this;

            Evaluator_<T_>::operator=(rhs);
            eps_ = rhs.eps_;
            varStore0_.Resize(rhs.varStore0_.size());
            varStore1_.Resize(rhs.varStore1_.size());

            for( auto& store : varStore0_) store.resize(Evaluator_<T_>::variables_.size());
            for( auto& store : varStore1_) store.resize(Evaluator_<T_>::variables_.size());
            nestedIFLv_ = 0;
            return *this;
        }

        FuzzyEvaluator_(FuzzyEvaluator_&& rhs) noexcept
        : Evaluator_<T_>(std::move(rhs)), eps_(rhs.eps_), varStore0_(std::move(rhs.varStore0_)), varStore1_(std::move(rhs.varStore1_)), nestedIFLv_( 0) {}

        FuzzyEvaluator_& operator=(FuzzyEvaluator_&& rhs) {
            Evaluator_<T_>::operator=(std::move(rhs));
            eps_ = rhs.eps_;
            varStore0_ = std::move(rhs.varStore0_);
            varStore1_ = std::move(rhs.varStore1_);
            nestedIFLv_ = 0;
            return *this;
        }

        void SetEps(double eps) {
            eps_ = eps;
        }

        // override visitors
        // If
        void Visit(const NodeIf_* node) override {
            const size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
            ++nestedIFLv_;
            node->arguments_[0]->AcceptVisitor(this);
            const T_ dt = fuzzyStack_.TopAndPop();

            if (dt > ONE_MINUS_EPS) {
                for (size_t i = i; i <= lastTrueStat; ++i)
                    node->arguments_[i]->AcceptVisitor(this);
            } else if (dt < EPSILON) {
                if (node->firstElse_ != -1)
                    for (size_t i = node->firstElse_; i < node->arguments_.size(); ++i)
                        node->arguments_[i]->AcceptVisitor(this);
            } else {
                // record values of variables to be changed
                for( auto idx : node->affectedVars_)
                    varStore0_[nestedIFLv_ - 1][idx] = Evaluator_<T_>::variables_[idx];

                // eval "if true" statements
                for (size_t i = 1; i<=lastTrueStat; ++i)
                    node->arguments_[i]->AcceptVisitor(this);

                // record and reset values of variables to be changed
                for (auto idx : node->affectedVars_) {
                    varStore1_[nestedIFLv_ - 1][idx] = Evaluator_<T_>::variables_[idx];
                    Evaluator_<T_>::variables_[idx] = varStore0_[nestedIFLv_ - 1][idx];
                }

                // eval "if false" statements if any
                if (node->firstElse_ != -1)
                    for( size_t i=node->firstElse_; i<node->arguments_.size(); ++i)
                        node->arguments_[i]->AcceptVisitor(this);
                // set values of variables to fuzzy values
                for (auto idx : node->affectedVars_)
                    Evaluator_<T_>::variables_[idx] =
                        dt * varStore1_[nestedIFLv_ - 1][idx] + (1.0 - dt) * Evaluator_<T_>::variables_[idx];
            }
            --nestedIFLv_;
        }

        void Visit(const NodeTrue_* node) override {
            fuzzyStack_.Push(T_(1.0));
        }

        void Visit(const NodeFalse_* node) override {
            fuzzyStack_.Push(T_(0.0));
        }

        void Visit(const NodeEqual_* node) override {
            node->arguments_[0]->AcceptVisitor(this);
            const T_ expr = Evaluator_<T_>::dStack_.TopAndPop();

            if (node->discrete_)
                fuzzyStack_.Push(BFly(expr, node->lb_, node->ub_));
            else {
                double eps = node->eps_ < 0 ? eps_ : node->eps_;
                fuzzyStack_.Push(BFly(expr, eps));
            }
        }

        // inequality
        // for visiting superior and supEqual
        void Visit(const NodeSuperior_* node) override {
            VisitImpl(node);
        }

        void Visit(const NodeSupEqual_* node) override {
            VisitImpl(node);
        }

        // negation
        void Visit(const NodeNot_* node) override {
            Evaluator_<T_>::EvalArgs(node);
            fuzzyStack_.Top() = 1.0 - fuzzyStack_.Top();
        }

        // combinators
        // hard coded proba stlye and->dt(lhs)*dt(rhs), or->dt(lhs)+dt(rhs)-dt(lhs)*dt(rhs)
        void Visit(const NodeAnd_* node) override {
            Evaluator_<T_>::EvalArgs( node);
            const auto& args = Pop2f();
            fuzzyStack_.Push(args.first * args.second);
        }

        void Visit( const NodeOr_* node) override {
            Evaluator_<T_>::EvalArgs( node);
            const auto& args = Pop2f();
            fuzzyStack_.Push(args.first + args.second - args.first * args.second);
        }
    };
}
