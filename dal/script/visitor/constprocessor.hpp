/*
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

#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {
    class ConstProcessor_ : public Visitor_<ConstProcessor_> {
    protected:
        //	State

        //  Const status of variables
        Vector_<char> myVarConst;
        Vector_<double> myVarConstVal;

        //	Inside an if?
        bool myInConditional;

        //  Is this node a constant?
        //  Note the argument must be of ExprNode_ type
        static bool constArg(const ExprTree_& node) { return Downcast<const ExprNode_>(node)->isConst; }

        //  Are all the arguments to this node constant?
        //  Note the arguments must be of ExprNode_ type
        static bool constArgs(const Node_& node, const size_t first = 0) {
            for (size_t i = first; i < node.arguments.size(); ++i) {
                if (!constArg(node.arguments[i]))
                    return false;
            }
            return true;
        }

    public:
        using Visitor_<ConstProcessor_>::Visit;

        //	Constructor, nVar = number of variables, from Product after parsing and variable indexation
        //  All variables start as constants with value 0
        ConstProcessor_(const size_t nVar) : myVarConst(nVar, true), myVarConstVal(nVar, 0.0), myInConditional(false) {}

        //	Visitors

        //	Expressions

        //	Binaries

        template <class OP> void visitBinary(ExprNode_& node, const OP op) {
            visitArguments(node);
            if (constArgs(node)) {
                node.isConst = true;

                const double lhs = Downcast<ExprNode_>(node.arguments[0])->constVal;
                const double rhs = Downcast<ExprNode_>(node.arguments[1])->constVal;
                node.constVal = op(lhs, rhs);
            }
        }

        void Visit(NodeAdd& node) {
            visitBinary(node, [](const double& x, const double& y) { return x + y; });
        }
        void Visit(NodeSub& node) {
            visitBinary(node, [](const double& x, const double& y) { return x - y; });
        }
        void Visit(NodeMult& node) {
            visitBinary(node, [](const double& x, const double& y) { return x * y; });
        }
        void Visit(NodeDiv& node) {
            visitBinary(node, [](const double& x, const double& y) { return x / y; });
        }
        void Visit(NodePow& node) {
            visitBinary(node, [](const double& x, const double& y) { return pow(x, y); });
        }
        void Visit(NodeMax& node) {
            visitBinary(node, [](const double& x, const double& y) { return max(x, y); });
        }
        void Visit(NodeMin& node) {
            visitBinary(node, [](const double& x, const double& y) { return min(x, y); });
        }

        //	Unaries
        template <class OP> void visitUnary(ExprNode_& node, const OP op) {
            visitArguments(node);
            if (constArgs(node)) {
                node.isConst = true;

                const double arg = Downcast<ExprNode_>(node.arguments[0])->constVal;
                node.constVal = op(arg);
            }
        }

        void Visit(NodeUplus& node) {
            visitUnary(node, [](const double& x) { return x; });
        }
        void Visit(NodeUminus& node) {
            visitUnary(node, [](const double& x) { return -x; });
        }

        //	Functions
        void Visit(NodeLog& node) {
            visitUnary(node, [](const double& x) { return log(x); });
        }
        void Visit(NodeSqrt& node) {
            visitUnary(node, [](const double& x) { return sqrt(x); });
        }

        //  Multies

        void Visit(NodeSmooth& node) {
            visitArguments(node);
            if (constArgs(node)) {
                node.isConst = true;

                const double x = reinterpret_cast<ExprNode_*>(node.arguments[0].get())->constVal;
                const double vPos = reinterpret_cast<ExprNode_*>(node.arguments[1].get())->constVal;
                const double vNeg = reinterpret_cast<ExprNode_*>(node.arguments[2].get())->constVal;
                const double halfEps = 0.5 * reinterpret_cast<ExprNode_*>(node.arguments[3].get())->constVal;

                if (x < -halfEps)
                    node.constVal = vNeg;
                else if (x > halfEps)
                    node.constVal = vPos;
                else {
                    node.constVal = vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps);
                }
            }
        }

        //	If
        void Visit(NodeIf& node) {
            //  Mark conditional

            //  Identify nested
            bool nested = myInConditional;

            //  Mark
            if (!nested)
                myInConditional = true;

            //  Visit arguments
            visitArguments(node);

            //  Reset (unless nested)
            if (!nested)
                myInConditional = false;
        }

        void Visit(NodeAssign& node) {
            //  Get index from LHS
            const size_t varIndex = Downcast<const NodeVar>(node.arguments[0])->index;

            //  Visit RHS
            node.arguments[1]->Accept(*this);

            //  All conditional assignments result in non const vars
            if (!myInConditional) {
                //  RHS constant?
                if (constArg(node.arguments[1])) {
                    myVarConst[varIndex] = true;
                    myVarConstVal[varIndex] = Downcast<const ExprNode_>(node.arguments[1])->constVal;
                } else {
                    myVarConst[varIndex] = false;
                }
            } else {
                myVarConst[varIndex] = false;
            }
        }

        void Visit(NodePays& node) {
            //  A payment is always non constant because it is normalized by a possibly stochastic numeraire
            const size_t varIndex = Downcast<const NodeVar>(node.arguments[0])->index;
            myVarConst[varIndex] = false;

            //  Visit RHS
            node.arguments[1]->Accept(*this);
        }

        //	Variables, RHS only, we don't Visit LHS vars
        void Visit(NodeVar& node) {
            if (myVarConst[node.index]) {
                node.isConst = true;
                node.constVal = myVarConstVal[node.index];
            } else {
                node.isConst = false;
            }
        }

        //  We don't Visit boolean nodes, that is best left to fuzzy logic

        //  We don't Visit constants (which are always const) or spots (which are never const)
    };
}

