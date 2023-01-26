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

    template <class T, template <typename> class EVAL> class EvaluatorBase : public constVisitor<EVAL<T>> {
    protected:
        //	State
        Vector_<T> myVariables;

        //	Stacks
        StaticStack_<T> myDstack;
        StaticStack_<char> myBstack;

        //	Reference to current scenario
        const AAD::Scenario_<T>* myScenario;

        //	Index of current event
        size_t myCurEvt;

    public:
        using constVisitor<EVAL<T>>::Visit;
        using constVisitor<EVAL<T>>::visitNode;

        //	Constructor, nVar = number of variables, from Product after parsing and variable indexation
        EvaluatorBase(const size_t nVar) : myVariables(nVar) {}

        //	Copy/Move

        EvaluatorBase(const EvaluatorBase& rhs) : myVariables(rhs.myVariables) {}
        EvaluatorBase& operator=(const EvaluatorBase& rhs) {
            if (this == &rhs)
                return *this;
            myVariables = rhs.myVariables;
            return *this;
        }

        EvaluatorBase(EvaluatorBase&& rhs) : myVariables(move(rhs.myVariables)) {}
        EvaluatorBase& operator=(EvaluatorBase&& rhs) {
            myVariables = move(rhs.myVariables);
            return *this;
        }

        //	(Re-)initialize before evaluation in each scenario
        void Init() {
            for (auto& varIt : myVariables)
                varIt = 0.0;
            //	Stacks should be empty, if this is not the case the empty them
            //		without affecting capacity for added performance
            myDstack.reset();
            myBstack.reset();
        }

        //	Accessors

        //	Access to variable values after evaluation
        const Vector_<T>& VarVals() const { return myVariables; }

        //	Set generated scenarios and current event

        //	Set reference to current scenario
        void SetScenario(const AAD::Scenario_<T>* scen) { myScenario = scen; }

        //	Set index of current event
        void SetCurEvt(size_t curEvt) { myCurEvt = curEvt; }

        //	Visitors

        //	Expressions

        //	Binaries

        template <class OP> void visitBinary(const exprNode& node, OP op) {
            visitNode(*node.arguments[0]);
            visitNode(*node.arguments[1]);
            op(myDstack[1], myDstack.top());
            myDstack.pop();
        }

        void Visit(const NodeAdd& node) {
            visitBinary(node, [](T& x, const T y) { x += y; });
        }
        void Visit(const NodeSub& node) {
            visitBinary(node, [](T& x, const T y) { x -= y; });
        }
        void Visit(const NodeMult& node) {
            visitBinary(node, [](T& x, const T y) { x *= y; });
        }
        void Visit(const NodeDiv& node) {
            visitBinary(node, [](T& x, const T y) { x /= y; });
        }
        void Visit(const NodePow& node) {
            visitBinary(node, [](T& x, const T y) { x = pow(x, y); });
        }
        void Visit(const NodeMax& node) {
            visitBinary(node, [](T& x, const T y) {
                if (x < y)
                    x = y;
            });
        }
        void Visit(const NodeMin& node) {
            visitBinary(node, [](T& x, const T y) {
                if (x > y)
                    x = y;
            });
        }

        //	Unaries
        template <class OP> inline void visitUnary(const exprNode& node, OP op) {
            visitNode(*node.arguments[0]);
            op(myDstack.top());
        }

        void Visit(const NodeUplus& node) {
            visitUnary(node, [](T& x) {});
        }
        void Visit(const NodeUminus& node) {
            visitUnary(node, [](T& x) { x = -x; });
        }

        //	Functions
        void Visit(const NodeLog& node) {
            visitUnary(node, [](T& x) { x = log(x); });
        }
        void Visit(const NodeSqrt& node) {
            visitUnary(node, [](T& x) { x = sqrt(x); });
        }

        //  Multies
        void Visit(const NodeSmooth& node) {
            //	Eval the condition
            visitNode(*node.arguments[0]);
            const T x = myDstack.top();
            myDstack.pop();

            //	Eval the epsilon
            visitNode(*node.arguments[3]);
            const T halfEps = 0.5 * myDstack.top();
            myDstack.pop();

            //	Left
            if (x < -halfEps)
                visitNode(*node.arguments[2]);

            //	Right
            else if (x > halfEps)
                visitNode(*node.arguments[1]);

            //	Fuzzy
            else {
                visitNode(*node.arguments[1]);
                const T vPos = myDstack.top();
                myDstack.pop();

                visitNode(*node.arguments[2]);
                const T vNeg = myDstack.top();
                myDstack.pop();

                myDstack.push(vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps));
            }
        }

        //	Conditions
        template <class OP> inline void visitCondition(const boolNode& node, OP op) {
            visitNode(*node.arguments[0]);
            myBstack.push(op(myDstack.top()));
            myDstack.pop();
        }

        void Visit(const NodeEqual& node) {
            visitCondition(node, [](const T x) { return x == 0; });
        }
        void Visit(const NodeSup& node) {
            visitCondition(node, [](const T x) { return x > 0; });
        }
        void Visit(const NodeSupEqual& node) {
            visitCondition(node, [](const T x) { return x >= 0; });
        }

        void Visit(const NodeAnd& node) {
            visitNode(*node.arguments[0]);
            if (myBstack.top()) {
                myBstack.pop();
                visitNode(*node.arguments[1]);
            }
        }
        void Visit(const NodeOr& node) {
            visitNode(*node.arguments[0]);
            if (!myBstack.top()) {
                myBstack.pop();
                visitNode(*node.arguments[1]);
            }
        }
        void Visit(const NodeNot& node) {
            visitNode(*node.arguments[0]);
            auto& b = myBstack.top();
            b = !b;
        }

        //	Instructions
        void Visit(const NodeIf& node) {
            //	Eval the condition
            visitNode(*node.arguments[0]);

            //	Pick the result
            const auto isTrue = myBstack.top();
            myBstack.pop();

            //	Evaluate the relevant statements
            if (isTrue) {
                const auto lastTrue = node.firstElse == -1 ? node.arguments.size() - 1 : node.firstElse - 1;
                for (unsigned i = 1; i <= lastTrue; ++i) {
                    visitNode(*node.arguments[i]);
                }
            } else if (node.firstElse != -1) {
                const size_t n = node.arguments.size();
                for (unsigned i = node.firstElse; i < n; ++i) {
                    visitNode(*node.arguments[i]);
                }
            }
        }

        void Visit(const NodeAssign& node) {
            const auto varIdx = downcast<NodeVar>(node.arguments[0])->index;

            //	Visit the RHS expression
            visitNode(*node.arguments[1]);

            //	Write result into variable
            myVariables[varIdx] = myDstack.top();
            myDstack.pop();
        }

        void Visit(const NodePays& node) {
            const auto varIdx = downcast<NodeVar>(node.arguments[0])->index;

            //	Visit the RHS expression
            visitNode(*node.arguments[1]);

            //	Write result into variable
            myVariables[varIdx] += myDstack.top() / (*myScenario)[myCurEvt].numeraire_;
            myDstack.pop();
        }

        //	Variables and constants
        void Visit(const NodeVar& node) {
            //	Push value onto the stack
            myDstack.push(myVariables[node.index]);
        }

        void Visit(const NodeConst& node) { myDstack.push(node.constVal); }

        void Visit(const NodeTrue& node) { myBstack.push(true); }
        void Visit(const NodeFalse& node) { myBstack.push(false); }

        //	Scenario related
        void Visit(const NodeSpot& node) { myDstack.push((*myScenario)[myCurEvt].spot_); }
    };

    //  Concrete Evaluator_
    template <class T> class Evaluator_ : public EvaluatorBase<T, Evaluator_> {

    public:
        using Base = EvaluatorBase<T, Evaluator_>;

        Evaluator_(const size_t nVar) : Base(nVar) {}
        Evaluator_(const Evaluator_& rhs) : Base(rhs) {}
        Evaluator_(Evaluator_&& rhs) : Base(move(rhs)) {}
        Evaluator_& operator=(const Evaluator_& rhs) { Base::operator=(rhs); }
        Evaluator_& operator=(const Evaluator_&& rhs) { Base::operator=(move(rhs)); }
    };

} // namespace Dal::Script
