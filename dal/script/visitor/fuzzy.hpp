//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor/evaluator.hpp>

namespace Dal::Script {

    //	The fuzzy evaluator

    template <class T> class FuzzyEvaluator_ : public EvaluatorBase_<T, FuzzyEvaluator_> {
        //	Default smoothing factor for conditions that don't override it
        double myDefEps;

        //	Stack for the fuzzy evaluation of conditions
        StaticStack_<T> myFuzzyStack;

        //	Temp storage for variables, preallocated for performance
        //	[i][j] = nested if level i variable j
        Vector_<Vector_<T>> myVarStore0;
        Vector_<Vector_<T>> myVarStore1;

        //	Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t myNestedIfLvl;

        //	Pop the Top 2 numbers of the fuzzy condition stack
        pair<T, T> Pop2f() {
            pair<T, T> res;
            res.first = myFuzzyStack.Top();
            myFuzzyStack.Pop();
            res.second = myFuzzyStack.Top();
            myFuzzyStack.Pop();
            return res;
        }

        //	Call Spread (-eps_/2,+eps_/2)
        static T CSpr(const T x, const double eps) {
            const double halfEps = 0.5 * eps;

            if (x < -halfEps)
                return T(0.0);
            else if (x > halfEps)
                return T(1.0);
            else
                return (x + halfEps) / eps;
        }

        //	Call Spread (lb_,rb_)
        static T CSpr(const T x, const double lb, const double rb) {
            if (x < lb)
                return T(0.0);
            else if (x > rb)
                return T(1.0);
            else
                return (x - lb) / (rb - lb);
        }

        //	Butterfly (-eps_/2,+eps_/2)
        static T BFly(const T x, const double eps) {
            const double halfEps = 0.5 * eps;

            if (x < -halfEps || x > halfEps)
                return T(0.0);
            else
                return (halfEps - fabs(x)) / halfEps;
        }

        //	Butterfly (lb_,0,rb_)
        static T BFly(const T x, const double lb, const double rb) {
            if (x < lb || x > rb)
                return T(0.0);
            else if (x < 0.0)
                return 1.0 - x / lb;
            else
                return 1.0 - x / rb;
        }

    public:
        using Base = EvaluatorBase_<T, FuzzyEvaluator_>;

        using Base::dStack_;
        using Base::variables_;
        using Base::Visit;
        using Base::VisitNode;

        FuzzyEvaluator_(const size_t nVar, const size_t maxNestedIfs, const double defEps = 0)
            : Base(nVar), myDefEps(defEps), myVarStore0(maxNestedIfs), myVarStore1(maxNestedIfs), myNestedIfLvl(0) {
            for (auto& varStore : myVarStore0)
                varStore.Resize(nVar);
            for (auto& varStore : myVarStore1)
                varStore.Resize(nVar);
        }

        //	Copy/Move

        FuzzyEvaluator_(const FuzzyEvaluator_& rhs)
            : Base(rhs), myDefEps(rhs.myDefEps), myVarStore0(rhs.myVarStore0.size()),
              myVarStore1(rhs.myVarStore1.size()), myNestedIfLvl(0) {
            for (auto& varStore : myVarStore0)
                varStore.Resize(variables_.size());
            for (auto& varStore : myVarStore1)
                varStore.Resize(variables_.size());
        }
        FuzzyEvaluator_& operator=(const FuzzyEvaluator_& rhs) {
            if (this == &rhs)
                return *this;
            Base::operator=(rhs);
            myDefEps = rhs.myDefEps;
            myVarStore0.Resize(rhs.myVarStore0.size());
            myVarStore1.Resize(rhs.myVarStore1.size());
            for (auto& varStore : myVarStore0)
                varStore.Resize(variables_.size());
            for (auto& varStore : myVarStore1)
                varStore.Resize(variables_.size());
            myNestedIfLvl = 0;
            return *this;
        }

        FuzzyEvaluator_(FuzzyEvaluator_&& rhs)
            : Base(move(rhs)), myDefEps(rhs.myDefEps), myVarStore0(move(rhs.myVarStore0)),
              myVarStore1(move(rhs.myVarStore1)), myNestedIfLvl(0) {}
        FuzzyEvaluator_& operator=(FuzzyEvaluator_&& rhs) {
            Base::operator=(move(rhs));
            myDefEps = rhs.myDefEps;
            myVarStore0 = move(rhs.myVarStore0);
            myVarStore1 = move(rhs.myVarStore1);
            myNestedIfLvl = 0;
            return *this;
        }

        //	(Re)set default smoothing factor
        void SetDefEps(double defEps) { myDefEps = defEps; }

        //	Overriden visitors

        //	If
        void Visit(const NodeIf& node) {
            //	Last "if true" statement index
            const size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

            //	Increase nested if level
            ++myNestedIfLvl;

            //	Visit the condition and compute its degree of truth dt
            VisitNode(*node.arguments_[0]);
            const T dt = myFuzzyStack.Top();
            myFuzzyStack.Pop();

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
                    myVarStore0[myNestedIfLvl - 1][idx] = variables_[idx];

                //	Eval "if true" statements
                for (size_t i = 1; i <= lastTrueStat; ++i)
                    VisitNode(*node.arguments_[i]);

                //	Record and Reset values of variables to be changed
                for (auto idx : node.affectedVars_) {
                    myVarStore1[myNestedIfLvl - 1][idx] = variables_[idx];
                    variables_[idx] = myVarStore0[myNestedIfLvl - 1][idx];
                }

                //	Eval "if false" statements if any
                if (node.firstElse_ != -1)
                    for (size_t i = node.firstElse_; i < node.arguments_.size(); ++i)
                        VisitNode(*node.arguments_[i]);

                //	Set values of variables to fuzzy values
                for (auto idx : node.affectedVars_)
                    variables_[idx] = dt * myVarStore1[myNestedIfLvl - 1][idx] + (1.0 - dt) * variables_[idx];
            }

            //	Decrease nested if level
            --myNestedIfLvl;
        }

        //	Conditions

        void Visit(const NodeTrue& node) { myFuzzyStack.Push(1.0); }
        void Visit(const NodeFalse& node) { myFuzzyStack.Push(0.0); }

        //	Equality
        void Visit(const NodeEqual& node) {
            //	Evaluate expression to be compared to 0
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.Top();
            dStack_.Pop();

            //	Discrete case: 0 is a singleton in expr's domain
            if (node.isDiscrete_) {
                myFuzzyStack.Push(BFly(expr, node.lb_, node.rb_));
            }
            //	Continuous case: 0 is part of expr's continuous domain
            else {
                //	Effective epsilon: take default unless overwritten on the node
                double eps = node.eps_ < 0 ? myDefEps : node.eps_;

                //	Butterfly
                myFuzzyStack.Push(BFly(expr, eps));
            }
        }

        //	Inequality

        //	For visiting superior and supEqual
        void VisitComp(const CompNode_& node) {
            //	Evaluate expression to be compared to 0
            VisitNode(*node.arguments_[0]);
            const T expr = dStack_.Top();
            dStack_.Pop();

            //	Discrete case:
            //	Either 0 is a singleton in expr's domain
            //	Or 0 is not part of expr's domain, but expr's domain has subdomains left and right of 0
            //		otherwise the condition would be always true/false
            if (node.isDiscrete_) {
                //	Call spread on the right
                myFuzzyStack.Push(CSpr(expr, node.lb_, node.rb_));
            }
            //	Continuous case: 0 is part of expr's continuous domain
            else {
                //	Effective epsilon: take default unless overwritten on the node
                const double eps = node.eps_ < 0 ? myDefEps : node.eps_;

                //	Call Spread
                myFuzzyStack.Push(CSpr(expr, eps));
            }
        }

        void Visit(const NodeSup& node) { VisitComp(node); }

        void Visit(const NodeSupEqual& node) { VisitComp(node); }

        //	Negation
        void visitNot(const NodeNot& node) {
            VisitNode(*node.arguments_[0]);
            myFuzzyStack.Top() = 1.0 - myFuzzyStack.Top();
        }

        //	Combinators
        //	Hard coded proba stlye and->dt(lhs)*dt(rhs), or->dt(lhs)+dt(rhs)-dt(lhs)*dt(rhs)
        void Visit(const NodeAnd& node) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            const auto args = Pop2f();
            myFuzzyStack.Push(args.first * args.second);
        }
        void Visit(const NodeOr& node) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            const auto args = Pop2f();
            myFuzzyStack.Push(args.first + args.second - args.first * args.second);
        }
    };
} // namespace Dal::Script
