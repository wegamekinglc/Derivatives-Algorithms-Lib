/*
 * Created by wegam on 2023/1/26.
Written by Antoine Savine in 2018

This code is the strict IP of Antoine Savine

License to use and alter this code for personal and commercial applications
is freely granted to any person or company who purchased a copy of the book

Modern Computational Finance: Scripting for Derivatives and XVA
Jesper Andreasen & Antoine Savine
Wiley, 2018

As long as this comment is preserved at the Top of the file
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
    template <class T_> struct EvalState_ {
        //	State
        Vector_<T_> variables_;

        //  Constructor
        EvalState_(size_t nVar) : variables_(nVar) {}

        //  Initializer
        void Init() {
            for (auto& var : variables_)
                var = 0.0;
        }

        const Vector_<T_>& VarVals() const { return variables_; }
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

    class Compiler_ : public ConstVisitor_<Compiler_> {
        //	State
        Vector_<int> nodeStream_;
        Vector_<double> constStream_;
        Vector_<const void*> dataStream_;

    public:
        using ConstVisitor_<Compiler_>::Visit;

        //	Accessors

        //	Access the streams after traversal
        const Vector_<int>& NodeStream() const { return nodeStream_; }
        const Vector_<double>& ConstStream() const { return constStream_; }
        const Vector_<const void*>& DataStream() const { return dataStream_; }

        //	Visitors

        //	Expressions

        //  Binaries

        template <NodeType IfBin, NodeType IfConstLeft, NodeType IfConstRight> void VisitBinary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.push_back(Const);
                nodeStream_.push_back(int(constStream_.size()));
                constStream_.push_back(node.constVal_);
            } else {
                const ExprNode_* lhs = Downcast<ExprNode_>(node.arguments_[0]);
                const ExprNode_* rhs = Downcast<ExprNode_>(node.arguments_[1]);

                if (lhs->isConst_) {
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.push_back(IfConstLeft);
                    nodeStream_.push_back(int(constStream_.size()));
                    constStream_.push_back(lhs->constVal_);
                } else if (rhs->isConst_) {
                    node.arguments_[0]->Accept(*this);
                    nodeStream_.push_back(IfConstRight);
                    nodeStream_.push_back(int(constStream_.size()));
                    constStream_.push_back(rhs->constVal_);
                } else {
                    node.arguments_[0]->Accept(*this);
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.push_back(IfBin);
                }
            }
        }

        void Visit(const NodeAdd& node) { VisitBinary<Add, AddConst, AddConst>(node); }
        void Visit(const NodeSub& node) { VisitBinary<Sub, ConstSub, SubConst>(node); }
        void Visit(const NodeMult& node) { VisitBinary<Mult, MultConst, MultConst>(node); }
        void Visit(const NodeDiv& node) { VisitBinary<Div, ConstDiv, DivConst>(node); }
        void Visit(const NodePow& node) { VisitBinary<Pow, ConstPow, PowConst>(node); }

        void Visit(const NodeMax& node) { VisitBinary<Max2, Max2Const, Max2Const>(node); }

        void Visit(const NodeMin& node) { VisitBinary<Min2, Min2Const, Min2Const>(node); }

        //	Unaries
        template <NodeType NT> void VisitUnary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.push_back(Const);
                nodeStream_.push_back(int(constStream_.size()));
                constStream_.push_back(node.constVal_);
            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.push_back(NT);
            }
        }

        void Visit(const NodeUplus& node) { node.arguments_[0]->Accept(*this); }

        void Visit(const NodeUminus& node) { VisitUnary<Uminus>(node); }
        void Visit(const NodeLog& node) { VisitUnary<Log>(node); }
        void Visit(const NodeSqrt& node) { VisitUnary<Sqrt>(node); }

        //  Multies

        void Visit(const NodeSmooth& node) {
            //  Const?
            if (node.isConst_) {
                nodeStream_.push_back(Const);
                nodeStream_.push_back(int(constStream_.size()));
                constStream_.push_back(node.constVal_);
            } else {
                //  Must come back to optimize that one
                VisitArguments(node);
                nodeStream_.push_back(Smooth);
            }
        }

        //	Conditions

        template <NodeType NT, typename OP> void VisitCondition(const BoolNode_& node, OP op) {
            const ExprNode_* arg = Downcast<ExprNode_>(node.arguments_[0]);

            if (arg->isConst_) {
                nodeStream_.push_back(op(arg->constVal_) ? True : False);

            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.push_back(NT);
            }
        }

        void Visit(const NodeEqual& node) {
            VisitCondition<Equal>(node, [](const double x) { return x == 0.0; });
        }

        void Visit(const NodeSup& node) {
            VisitCondition<Sup>(node, [](const double x) { return x > 0.0; });
        }
        void Visit(const NodeSupEqual& node) {
            VisitCondition<SupEqual>(node, [](const double x) { return x > -Dal::EPSILON; });
        }

        //  And/Or/Not

        void Visit(const NodeAnd& node) {
            node.arguments_[0]->Accept(*this);
            node.arguments_[1]->Accept(*this);
            nodeStream_.push_back(And);
        }

        void Visit(const NodeOr& node) {
            node.arguments_[0]->Accept(*this);
            node.arguments_[1]->Accept(*this);
            nodeStream_.push_back(Or);
        }

        void Visit(const NodeNot& node) {
            node.arguments_[0]->Accept(*this);
            nodeStream_.push_back(Not);
        }

        //  Assign, pays

        void Visit(const NodeAssign& node) {
            const NodeVar* var = Downcast<NodeVar>(node.arguments_[0]);
            const ExprNode_* rhs = Downcast<ExprNode_>(node.arguments_[1]);

            if (rhs->isConst_) {
                nodeStream_.push_back(AssignConst);
                nodeStream_.push_back(int(constStream_.size()));
                constStream_.push_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.push_back(Assign);
            }
            nodeStream_.push_back(int(var->index_));
        }

        void Visit(const NodePays& node) {
            const NodeVar* var = Downcast<NodeVar>(node.arguments_[0]);
            const ExprNode_* rhs = Downcast<ExprNode_>(node.arguments_[1]);

            if (rhs->isConst_) {
                nodeStream_.push_back(PaysConst);
                nodeStream_.push_back(int(constStream_.size()));
                constStream_.push_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.push_back(Pays);
            }
            nodeStream_.push_back(int(var->index_));
        }

        //  Leaves

        void Visit(const NodeVar& node) {
            nodeStream_.push_back(Var);
            nodeStream_.push_back(int(node.index_));
        }

        void Visit(const NodeConst& node) {
            nodeStream_.push_back(Const);
            nodeStream_.push_back(int(constStream_.size()));
            constStream_.push_back(node.constVal_);
        }

        void Visit(const NodeTrue& node) { nodeStream_.push_back(True); }

        void Visit(const NodeFalse& node) { nodeStream_.push_back(False); }

        //	Scenario related
        void Visit(const NodeSpot& node) { nodeStream_.push_back(Spot); }

        //	Instructions
        void Visit(const NodeIf& node) {
            //  Visit condition
            node.arguments_[0]->Accept(*this);

            //  Mark instruction
            nodeStream_.push_back(node.firstElse_ == -1 ? If : IfElse);
            //  Record space
            const size_t thisSpace = nodeStream_.size() - 1;
            //  Make 2 spaces for last if-true and last if-false
            nodeStream_.push_back(0);
            if (node.firstElse_ != -1)
                nodeStream_.push_back(0);

            //  Visit if-true statements
            const auto lastTrue = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;
            for (size_t i = 1; i <= lastTrue; ++i) {
                node.arguments_[i]->Accept(*this);
            }
            //  Record last if-true space
            nodeStream_[thisSpace + 1] = int(nodeStream_.size());

            //  Visit if-false statements
            const size_t n = node.arguments_.size();
            if (node.firstElse_ != -1) {
                for (size_t i = node.firstElse_; i < n; ++i) {
                    node.arguments_[i]->Accept(*this);
                }
                //  Record last if-false space
                nodeStream_[thisSpace + 2] = int(nodeStream_.size());
            }
        }
    };

    template <class T>
    inline void EvalCompiled(
        //  Stream to eval
        const Vector_<int>& nodeStream,
        const Vector_<double>& constStream,
        const Vector_<const void*>& dataStream,
        //  Scenario
        const AAD::Sample_<T>& scen,
        //  State
        EvalState_<T>& state,
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

                dStack[1] += dStack.Top();
                dStack.Pop();

                ++i;
                break;

            case AddConst:

                dStack.Top() += constStream[nodeStream[++i]];

                ++i;
                break;

            case Sub:

                dStack[1] -= dStack.Top();
                dStack.Pop();

                ++i;
                break;

            case SubConst:

                dStack.Top() -= constStream[nodeStream[++i]];

                ++i;
                break;

            case ConstSub:

                dStack.Top() = constStream[nodeStream[++i]] - dStack.Top();

                ++i;
                break;

            case Mult:

                dStack[1] *= dStack.Top();
                dStack.Pop();

                ++i;
                break;

            case MultConst:

                dStack.Top() *= constStream[nodeStream[++i]];

                ++i;
                break;

            case Div:

                dStack[1] /= dStack.Top();
                dStack.Pop();

                ++i;
                break;

            case DivConst:

                dStack.Top() /= constStream[nodeStream[++i]];

                ++i;
                break;

            case ConstDiv:

                dStack.Top() = constStream[nodeStream[++i]] / dStack.Top();

                ++i;
                break;

            case Pow:

                dStack[1] = pow(dStack[1], dStack.Top());
                dStack.Pop();

                ++i;
                break;

            case PowConst:

                dStack.Top() = pow(dStack.Top(), constStream[nodeStream[++i]]);

                ++i;
                break;

            case ConstPow:

                dStack.Top() = pow(constStream[nodeStream[++i]], dStack.Top());

                ++i;
                break;

            case Max2:

                y = dStack.Top();

                if (y > dStack[1])
                    dStack[1] = y;
                dStack.Pop();

                ++i;
                break;

            case Max2Const:

                y = constStream[nodeStream[++i]];
                if (y > dStack.Top())
                    dStack.Top() = y;

                ++i;
                break;

            case Min2:

                y = dStack.Top();

                if (y < dStack[1])
                    dStack[1] = y;
                dStack.Pop();

                ++i;
                break;

            case Min2Const:

                y = constStream[nodeStream[++i]];
                if (y < dStack.Top())
                    dStack.Top() = y;

                ++i;
                break;

            case Spot:

                dStack.Push(scen.spot_);

                ++i;
                break;

            case Var:

                dStack.Push(state.variables_[nodeStream[++i]]);

                ++i;
                break;

            case Const:

                dStack.Push(constStream[nodeStream[++i]]);

                ++i;
                break;

            case Assign:

                idx = nodeStream[++i];
                state.variables_[idx] = dStack.Top();
                dStack.Pop();

                ++i;
                break;

            case AssignConst:

                x = constStream[nodeStream[++i]];
                idx = nodeStream[++i];
                state.variables_[idx] = x;

                ++i;
                break;

            case Pays:

                ++i;
                idx = nodeStream[i];
                state.variables_[idx] += dStack.Top() / scen.numeraire_;
                dStack.Pop();

                ++i;
                break;

            case PaysConst:

                x = constStream[nodeStream[++i]];
                idx = nodeStream[++i];
                state.variables_[idx] += x / scen.numeraire_;

                ++i;
                break;

            case If:

                if (bStack.Top()) {
                    i += 2;
                } else {
                    i = nodeStream[++i];
                }

                bStack.Pop();

                break;

            case IfElse:

                if (!bStack.Top()) {
                    i = nodeStream[++i];
                } else {
                    //  Cannot avoid nested call here
                    EvalCompiled(nodeStream, constStream, dataStream, scen, state, i + 3, nodeStream[i + 1]);
                    i = nodeStream[i + 2];
                }

                bStack.Pop();

                break;

            case Equal:

                bStack.Push(dStack.Top() == 0);
                dStack.Pop();

                ++i;
                break;

            case Sup:

                bStack.Push(dStack.Top() > 0);
                dStack.Pop();

                ++i;
                break;

            case SupEqual:

                bStack.Push(dStack.Top() >= 0);
                dStack.Pop();

                ++i;
                break;

            case And:

                if (bStack[1]) {
                    bStack[1] = bStack.Top();
                }
                bStack.Pop();

                ++i;
                break;

            case Or:

                if (!bStack[1]) {
                    bStack[1] = bStack.Top();
                }
                bStack.Pop();

                ++i;
                break;

            case Smooth:

                //	Eval the condition
                x = dStack[3];
                y = 0.5 * dStack.Top();
                z = dStack[2];
                t = dStack[1];

                dStack.Pop(3);

                //	Left
                if (x < -y)
                    dStack.Top() = t;

                //	Right
                if (x < -y)
                    dStack.Top() = z;

                //	Fuzzy
                else {
                    dStack.Top() = t + 0.5 * (z - t) / y * (x + y);
                }

                ++i;
                break;

            case Sqrt:

                dStack.Top() = sqrt(dStack.Top());

                ++i;
                break;

            case Log:

                dStack.Top() = log(dStack.Top());

                ++i;
                break;

            case Not:

                bStack.Top() = !bStack.Top();

                ++i;
                break;

            case Uminus:

                dStack.Top() = -dStack.Top();

                ++i;
                break;

            case True:

                bStack.Push(true);

                ++i;
                break;

            case False:

                bStack.Push(false);

                ++i;
                break;
            }
        }
    }
} // namespace Dal::Script