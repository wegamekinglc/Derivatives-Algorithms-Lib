/*
 * Created by wegam on 2023/1/26.
Written by Antoine Savine in 2018

This code is the strict IP of Antoine Savine

License to use and alter this code for personal and commercial applications
is freely granted to any person or company who purchased a copy of the book

Modern Computational Finance: Scripting for Derivatives and XVA
Jesper Andreasen & Antoine Savine
Wiley, 2018

As long as this comment is preserved at the top of the file
*/

#pragma once

#include <iostream>

#include <algorithm>
#include <dal/math/aad/sample.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <functional>

namespace Dal::Script {
    template <class T> struct EvalState {
        //	State
        Vector_<T> variables;

        //  Constructor
        EvalState(const size_t nVar) : variables(nVar) {}

        //  Initializer
        void init() {
            for (auto& var : variables)
                var = 0.0;
        }
    };

    enum NodeType {
        Add,
        AddConst,
        Sub,
        SubConst,
        ConstSub,
        Mult,
        MultConst,
        Div,
        DivConst,
        ConstDiv,
        Pow,
        PowConst,
        ConstPow,
        Max2,
        Max2Const,
        Min2,
        Min2Const,
        Spot,
        Var,
        Const,
        Assign,
        AssignConst,
        Pays,
        PaysConst,
        If,
        IfElse,
        Equal,
        Sup,
        SupEqual,
        And,
        Or,
        Smooth,
        Sqrt,
        Log,
        Not,
        Uminus,
        True,
        False
    };

#define EPS 1.0e-12

    class Compiler_ : public constVisitor<Compiler_> {
        //	State
        Vector_<int> myNodeStream;
        Vector_<double> myConstStream;
        Vector_<const void*> myDataStream;

    public:
        using constVisitor<Compiler_>::Visit;

        //	Accessors

        //	Access the streams after traversal
        const Vector_<int>& nodeStream() const { return myNodeStream; }
        const Vector_<double>& constStream() const { return myConstStream; }
        const Vector_<const void*>& dataStream() const { return myDataStream; }

        //	Visitors

        //	Expressions

        //  Binaries

        template <NodeType IfBin, NodeType IfConstLeft, NodeType IfConstRight> void visitBinary(const exprNode& node) {
            if (node.isConst) {
                myNodeStream.push_back(Const);
                myNodeStream.push_back(int(myConstStream.size()));
                myConstStream.push_back(node.constVal);
            } else {
                const exprNode* lhs = downcast<exprNode>(node.arguments[0]);
                const exprNode* rhs = downcast<exprNode>(node.arguments[1]);

                if (lhs->isConst) {
                    node.arguments[1]->accept(*this);
                    myNodeStream.push_back(IfConstLeft);
                    myNodeStream.push_back(int(myConstStream.size()));
                    myConstStream.push_back(lhs->constVal);
                } else if (rhs->isConst) {
                    node.arguments[0]->accept(*this);
                    myNodeStream.push_back(IfConstRight);
                    myNodeStream.push_back(int(myConstStream.size()));
                    myConstStream.push_back(rhs->constVal);
                } else {
                    node.arguments[0]->accept(*this);
                    node.arguments[1]->accept(*this);
                    myNodeStream.push_back(IfBin);
                }
            }
        }

        void Visit(const NodeAdd& node) { visitBinary<Add, AddConst, AddConst>(node); }
        void Visit(const NodeSub& node) { visitBinary<Sub, ConstSub, SubConst>(node); }
        void Visit(const NodeMult& node) { visitBinary<Mult, MultConst, MultConst>(node); }
        void Visit(const NodeDiv& node) { visitBinary<Div, ConstDiv, DivConst>(node); }
        void Visit(const NodePow& node) { visitBinary<Pow, ConstPow, PowConst>(node); }

        void Visit(const NodeMax& node) { visitBinary<Max2, Max2Const, Max2Const>(node); }

        void Visit(const NodeMin& node) { visitBinary<Min2, Min2Const, Min2Const>(node); }

        //	Unaries
        template <NodeType NT> void visitUnary(const exprNode& node) {
            if (node.isConst) {
                myNodeStream.push_back(Const);
                myNodeStream.push_back(int(myConstStream.size()));
                myConstStream.push_back(node.constVal);
            } else {
                node.arguments[0]->accept(*this);
                myNodeStream.push_back(NT);
            }
        }

        void Visit(const NodeUplus& node) { node.arguments[0]->accept(*this); }

        void Visit(const NodeUminus& node) { visitUnary<Uminus>(node); }
        void Visit(const NodeLog& node) { visitUnary<Log>(node); }
        void Visit(const NodeSqrt& node) { visitUnary<Sqrt>(node); }

        //  Multies

        void Visit(const NodeSmooth& node) {
            //  Const?
            if (node.isConst) {
                myNodeStream.push_back(Const);
                myNodeStream.push_back(int(myConstStream.size()));
                myConstStream.push_back(node.constVal);
            } else {
                //  Must come back to optimize that one
                visitArguments(node);
                myNodeStream.push_back(Smooth);
            }
        }

        //	Conditions

        template <NodeType NT, typename OP> void visitCondition(const boolNode& node, OP op) {
            const exprNode* arg = downcast<exprNode>(node.arguments[0]);

            if (arg->isConst) {
                myNodeStream.push_back(op(arg->constVal) ? True : False);

            } else {
                node.arguments[0]->accept(*this);
                myNodeStream.push_back(NT);
            }
        }

        void Visit(const NodeEqual& node) {
            visitCondition<Equal>(node, [](const double x) { return x == 0.0; });
        }

        void Visit(const NodeSup& node) {
            visitCondition<Sup>(node, [](const double x) { return x > 0.0; });
        }
        void Visit(const NodeSupEqual& node) {
            visitCondition<SupEqual>(node, [](const double x) { return x > -EPS; });
        }

        //  And/Or/Not

        void Visit(const NodeAnd& node) {
            node.arguments[0]->accept(*this);
            node.arguments[1]->accept(*this);
            myNodeStream.push_back(And);
        }

        void Visit(const NodeOr& node) {
            node.arguments[0]->accept(*this);
            node.arguments[1]->accept(*this);
            myNodeStream.push_back(Or);
        }

        void Visit(const NodeNot& node) {
            node.arguments[0]->accept(*this);
            myNodeStream.push_back(Not);
        }

        //  Assign, pays

        void Visit(const NodeAssign& node) {
            const NodeVar* var = downcast<NodeVar>(node.arguments[0]);
            const exprNode* rhs = downcast<exprNode>(node.arguments[1]);

            if (rhs->isConst) {
                myNodeStream.push_back(AssignConst);
                myNodeStream.push_back(int(myConstStream.size()));
                myConstStream.push_back(rhs->constVal);
            } else {
                node.arguments[1]->accept(*this);
                myNodeStream.push_back(Assign);
            }
            myNodeStream.push_back(int(var->index));
        }

        void Visit(const NodePays& node) {
            const NodeVar* var = downcast<NodeVar>(node.arguments[0]);
            const exprNode* rhs = downcast<exprNode>(node.arguments[1]);

            if (rhs->isConst) {
                myNodeStream.push_back(PaysConst);
                myNodeStream.push_back(int(myConstStream.size()));
                myConstStream.push_back(rhs->constVal);
            } else {
                node.arguments[1]->accept(*this);
                myNodeStream.push_back(Pays);
            }
            myNodeStream.push_back(int(var->index));
        }

        //  Leaves

        void Visit(const NodeVar& node) {
            myNodeStream.push_back(Var);
            myNodeStream.push_back(int(node.index));
        }

        void Visit(const NodeConst& node) {
            myNodeStream.push_back(Const);
            myNodeStream.push_back(int(myConstStream.size()));
            myConstStream.push_back(node.constVal);
        }

        void Visit(const NodeTrue& node) { myNodeStream.push_back(True); }

        void Visit(const NodeFalse& node) { myNodeStream.push_back(False); }

        //	Scenario related
        void Visit(const NodeSpot& node) { myNodeStream.push_back(Spot); }

        //	Instructions
        void Visit(const NodeIf& node) {
            //  Visit condition
            node.arguments[0]->accept(*this);

            //  Mark instruction
            myNodeStream.push_back(node.firstElse == -1 ? If : IfElse);
            //  Record space
            const size_t thisSpace = myNodeStream.size() - 1;
            //  Make 2 spaces for last if-true and last if-false
            myNodeStream.push_back(0);
            if (node.firstElse != -1)
                myNodeStream.push_back(0);

            //  Visit if-true statements
            const auto lastTrue = node.firstElse == -1 ? node.arguments.size() - 1 : node.firstElse - 1;
            for (size_t i = 1; i <= lastTrue; ++i) {
                node.arguments[i]->accept(*this);
            }
            //  Record last if-true space
            myNodeStream[thisSpace + 1] = int(myNodeStream.size());

            //  Visit if-false statements
            const size_t n = node.arguments.size();
            if (node.firstElse != -1) {
                for (size_t i = node.firstElse; i < n; ++i) {
                    {
                        node.arguments[i]->accept(*this);
                    }
                }
                //  Record last if-false space
                myNodeStream[thisSpace + 2] = int(myNodeStream.size());
            }
        }
    };

    template <class T>
    inline void evalCompiled(
        //  Stream to eval
        const Vector_<int>& nodeStream,
        const Vector_<double>& constStream,
        const Vector_<const void*>& dataStream,
        //  Scenario
        const AAD::Sample_<T>& scen,
        //  State
        EvalState<T>& state,
        //  First (included), last (excluded)
        size_t first = 0,
        size_t last = 0) {
        const size_t n = last ? last : nodeStream.size();
        size_t i = first;

        //  Work space
        T x, y, z, t;
        size_t idx;

        //  Stacks
        StaticStack_<T> dStack;
        StaticStack_<char> bStack;

        //  Loop on instructions
        while (i < n) {
            //  Big switch
            switch (nodeStream[i]) {

            case Add:

                dStack[1] += dStack.top();
                dStack.pop();

                ++i;
                break;

            case AddConst:

                dStack.top() += constStream[nodeStream[++i]];

                ++i;
                break;

            case Sub:

                dStack[1] -= dStack.top();
                dStack.pop();

                ++i;
                break;

            case SubConst:

                dStack.top() -= constStream[nodeStream[++i]];

                ++i;
                break;

            case ConstSub:

                dStack.top() = constStream[nodeStream[++i]] - dStack.top();

                ++i;
                break;

            case Mult:

                dStack[1] *= dStack.top();
                dStack.pop();

                ++i;
                break;

            case MultConst:

                dStack.top() *= constStream[nodeStream[++i]];

                ++i;
                break;

            case Div:

                dStack[1] /= dStack.top();
                dStack.pop();

                ++i;
                break;

            case DivConst:

                dStack.top() /= constStream[nodeStream[++i]];

                ++i;
                break;

            case ConstDiv:

                dStack.top() = constStream[nodeStream[++i]] / dStack.top();

                ++i;
                break;

            case Pow:

                dStack[1] = pow(dStack[1], dStack.top());
                dStack.pop();

                ++i;
                break;

            case PowConst:

                dStack.top() = pow(dStack.top(), constStream[nodeStream[++i]]);

                ++i;
                break;

            case ConstPow:

                dStack.top() = pow(constStream[nodeStream[++i]], dStack.top());

                ++i;
                break;

            case Max2:

                y = dStack.top();

                if (y > dStack[1])
                    dStack[1] = y;
                dStack.pop();

                ++i;
                break;

            case Max2Const:

                y = constStream[nodeStream[++i]];
                if (y > dStack.top())
                    dStack.top() = y;

                ++i;
                break;

            case Min2:

                y = dStack.top();

                if (y < dStack[1])
                    dStack[1] = y;
                dStack.pop();

                ++i;
                break;

            case Min2Const:

                y = constStream[nodeStream[++i]];
                if (y < dStack.top())
                    dStack.top() = y;

                ++i;
                break;

            case Spot:

                dStack.push(scen.spot);

                ++i;
                break;

            case Var:

                dStack.push(state.variables[nodeStream[++i]]);

                ++i;
                break;

            case Const:

                dStack.push(constStream[nodeStream[++i]]);

                ++i;
                break;

            case Assign:

                idx = nodeStream[++i];
                state.variables[idx] = dStack.top();
                dStack.pop();

                ++i;
                break;

            case AssignConst:

                x = constStream[nodeStream[++i]];
                idx = nodeStream[++i];
                state.variables[idx] = x;

                ++i;
                break;

            case Pays:

                ++i;
                idx = nodeStream[i];
                state.variables[idx] += dStack.top() / scen.numeraire;
                dStack.pop();

                ++i;
                break;

            case PaysConst:

                x = constStream[nodeStream[++i]];
                idx = nodeStream[++i];
                state.variables[idx] += x / scen.numeraire;

                ++i;
                break;

            case If:

                if (bStack.top()) {
                    i += 2;
                } else {
                    i = nodeStream[++i];
                }

                bStack.pop();

                break;

            case IfElse:

                if (!bStack.top()) {
                    i = nodeStream[++i];
                } else {
                    //  Cannot avoid nested call here
                    evalCompiled(nodeStream, constStream, dataStream, scen, state, i + 3, nodeStream[i + 1]);
                    i = nodeStream[i + 2];
                }

                bStack.pop();

                break;

            case Equal:

                bStack.push(dStack.top() == 0);
                dStack.pop();

                ++i;
                break;

            case Sup:

                bStack.push(dStack.top() > 0);
                dStack.pop();

                ++i;
                break;

            case SupEqual:

                bStack.push(dStack.top() >= 0);
                dStack.pop();

                ++i;
                break;

            case And:

                if (bStack[1]) {
                    bStack[1] = bStack.top();
                }
                bStack.pop();

                ++i;
                break;

            case Or:

                if (!bStack[1]) {
                    bStack[1] = bStack.top();
                }
                bStack.pop();

                ++i;
                break;

            case Smooth:

                //	Eval the condition
                x = dStack[3];
                y = 0.5 * dStack.top();
                z = dStack[2];
                t = dStack[1];

                dStack.pop(3);

                //	Left
                if (x < -y)
                    dStack.top() = t;

                //	Right
                if (x < -y)
                    dStack.top() = z;

                //	Fuzzy
                else {
                    dStack.top() = t + 0.5 * (z - t) / y * (x + y);
                }

                ++i;
                break;

            case Sqrt:

                dStack.top() = sqrt(dStack.top());

                ++i;
                break;

            case Log:

                dStack.top() = log(dStack.top());

                ++i;
                break;

            case Not:

                bStack.top() = !bStack.top();

                ++i;
                break;

            case Uminus:

                dStack.top() = -dStack.top();

                ++i;
                break;

            case True:

                bStack.push(true);

                ++i;
                break;

            case False:

                bStack.push(false);

                ++i;
                break;
            }
        }
    }
} // namespace Dal::Script