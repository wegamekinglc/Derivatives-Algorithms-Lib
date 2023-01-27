//
// Created by wegam on 2022/6/3.
//

#pragma once

#include <dal/math/aad/sample.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    template <class T, template <typename> class EVAL_> class EvaluatorBase_ : public ConstVisitor_<EVAL_<T>> {
    protected:
        //	State
        Vector_<T> variables_;

        //	Stacks
        StaticStack_<T> dStack_;
        StaticStack_<char> bStack_;

        //	Reference to current scenario
        const AAD::Scenario_<T>* scenario_;

        //	Index of current event
        size_t curEvt_;

    public:
        using ConstVisitor_<EVAL_<T>>::Visit;
        using ConstVisitor_<EVAL_<T>>::VisitNode;

        //	Constructor, nVar = number of variables, from Product after parsing and variable indexation
        EvaluatorBase_(const size_t nVar) : variables_(nVar) {}

        //	Copy/Move

        EvaluatorBase_(const EvaluatorBase_& rhs) : variables_(rhs.variables_) {}
        EvaluatorBase_& operator=(const EvaluatorBase_& rhs) {
            if (this == &rhs)
                return *this;
            variables_ = rhs.variables_;
            return *this;
        }

        EvaluatorBase_(EvaluatorBase_&& rhs) : variables_(move(rhs.variables_)) {}
        EvaluatorBase_& operator=(EvaluatorBase_&& rhs) {
            variables_ = move(rhs.variables_);
            return *this;
        }

        //	(Re-)initialize before evaluation in each scenario
        void Init() {
            for (auto& varIt : variables_)
                varIt = 0.0;
            //	Stacks should be empty, if this is not the case the empty them
            //		without affecting capacity for added performance
            dStack_.Reset();
            bStack_.Reset();
        }

        //	Accessors

        //	Access to variable values after evaluation
        const Vector_<T>& VarVals() const { return variables_; }

        //	Set generated scenarios and current event

        //	Set reference to current scenario
        void SetScenario(const AAD::Scenario_<T>* scen) { scenario_ = scen; }

        //	Set index of current event
        void SetCurEvt(size_t curEvt) { curEvt_ = curEvt; }

        //	Visitors

        //	Expressions

        //	Binaries

        template <class OP> void VisitBinary(const ExprNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            VisitNode(*node.arguments_[1]);
            op(dStack_[1], dStack_.Top());
            dStack_.Pop();
        }

        void Visit(const NodeAdd& node) {
            VisitBinary(node, [](T& x, const T y) { x += y; });
        }
        void Visit(const NodeSub& node) {
            VisitBinary(node, [](T& x, const T y) { x -= y; });
        }
        void Visit(const NodeMult& node) {
            VisitBinary(node, [](T& x, const T y) { x *= y; });
        }
        void Visit(const NodeDiv& node) {
            VisitBinary(node, [](T& x, const T y) { x /= y; });
        }
        void Visit(const NodePow& node) {
            VisitBinary(node, [](T& x, const T y) { x = pow(x, y); });
        }
        void Visit(const NodeMax& node) {
            VisitBinary(node, [](T& x, const T y) {
                if (x < y)
                    x = y;
            });
        }
        void Visit(const NodeMin& node) {
            VisitBinary(node, [](T& x, const T y) {
                if (x > y)
                    x = y;
            });
        }

        //	Unaries
        template <class OP> inline void VisitUnary(const ExprNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            op(dStack_.Top());
        }

        void Visit(const NodeUplus& node) {
            VisitUnary(node, [](T& x) {});
        }
        void Visit(const NodeUminus& node) {
            VisitUnary(node, [](T& x) { x = -x; });
        }

        //	Functions
        void Visit(const NodeLog& node) {
            VisitUnary(node, [](T& x) { x = log(x); });
        }
        void Visit(const NodeSqrt& node) {
            VisitUnary(node, [](T& x) { x = sqrt(x); });
        }

        //  Multies
        void Visit(const NodeSmooth& node) {
            //	Eval the condition
            VisitNode(*node.arguments_[0]);
            const T x = dStack_.Top();
            dStack_.Pop();

            //	Eval the epsilon
            VisitNode(*node.arguments_[3]);
            const T halfEps = 0.5 * dStack_.Top();
            dStack_.Pop();

            //	Left
            if (x < -halfEps)
                VisitNode(*node.arguments_[2]);

            //	Right
            else if (x > halfEps)
                VisitNode(*node.arguments_[1]);

            //	Fuzzy
            else {
                VisitNode(*node.arguments_[1]);
                const T vPos = dStack_.Top();
                dStack_.Pop();

                VisitNode(*node.arguments_[2]);
                const T vNeg = dStack_.Top();
                dStack_.Pop();

                dStack_.Push(vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps));
            }
        }

        //	Conditions
        template <class OP> inline void VisitCondition(const BoolNode_& node, OP op) {
            VisitNode(*node.arguments_[0]);
            bStack_.Push(op(dStack_.Top()));
            dStack_.Pop();
        }

        void Visit(const NodeEqual& node) {
            VisitCondition(node, [](const T x) { return x == 0; });
        }
        void Visit(const NodeSup& node) {
            VisitCondition(node, [](const T x) { return x > 0; });
        }
        void Visit(const NodeSupEqual& node) {
            VisitCondition(node, [](const T x) { return x >= 0; });
        }

        void Visit(const NodeAnd& node) {
            VisitNode(*node.arguments_[0]);
            if (bStack_.Top()) {
                bStack_.Pop();
                VisitNode(*node.arguments_[1]);
            }
        }
        void Visit(const NodeOr& node) {
            VisitNode(*node.arguments_[0]);
            if (!bStack_.Top()) {
                bStack_.Pop();
                VisitNode(*node.arguments_[1]);
            }
        }
        void Visit(const NodeNot& node) {
            VisitNode(*node.arguments_[0]);
            auto& b = bStack_.Top();
            b = !b;
        }

        //	Instructions
        void Visit(const NodeIf& node) {
            //	Eval the condition
            VisitNode(*node.arguments_[0]);

            //	Pick the result
            const auto isTrue = bStack_.Top();
            bStack_.Pop();

            //	Evaluate the relevant statements
            if (isTrue) {
                const auto lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
                for (unsigned i = 1; i <= lastTrue; ++i) {
                    VisitNode(*node.arguments_[i]);
                }
            } else if (node.firstElse_ != -1) {
                const size_t n = node.arguments_.size();
                for (unsigned i = node.firstElse_; i < n; ++i) {
                    VisitNode(*node.arguments_[i]);
                }
            }
        }

        void Visit(const NodeAssign& node) {
            const auto varIdx = Downcast<NodeVar>(node.arguments_[0])->index_;

            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            //	Write result into variable
            variables_[varIdx] = dStack_.Top();
            dStack_.Pop();
        }

        void Visit(const NodePays& node) {
            const auto varIdx = Downcast<NodeVar>(node.arguments_[0])->index_;

            //	Visit the RHS expression
            VisitNode(*node.arguments_[1]);

            //	Write result into variable
            variables_[varIdx] += dStack_.Top() / (*scenario_)[curEvt_].numeraire_;
            dStack_.Pop();
        }

        //	Variables and constants
        void Visit(const NodeVar& node) {
            //	Push value onto the stack
            dStack_.Push(variables_[node.index_]);
        }

        void Visit(const NodeConst& node) { dStack_.Push(node.constVal_); }

        void Visit(const NodeTrue& node) { bStack_.Push(true); }
        void Visit(const NodeFalse& node) { bStack_.Push(false); }

        //	Scenario related
        void Visit(const NodeSpot& node) { dStack_.Push((*scenario_)[curEvt_].spot_); }
    };

    //  Concrete Evaluator_
    template <class T> class Evaluator_ : public EvaluatorBase_<T, Evaluator_> {

    public:
        using Base = EvaluatorBase_<T, Evaluator_>;

        Evaluator_(const size_t nVar) : Base(nVar) {}
        Evaluator_(const Evaluator_& rhs) : Base(rhs) {}
        Evaluator_(Evaluator_&& rhs) : Base(move(rhs)) {}
        Evaluator_& operator=(const Evaluator_& rhs) { Base::operator=(rhs); }
        Evaluator_& operator=(const Evaluator_&& rhs) { Base::operator=(move(rhs)); }
    };

} // namespace Dal::Script
