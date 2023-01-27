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
        Vector_<char> varConst_;
        Vector_<double> varConstVal_;

        //	Inside an if?
        bool isInConditional_;

        //  Is this node a constant?
        //  Note the argument must be of ExprNode_ type
        static bool ConstArg(const ExprTree_& node) { return Downcast<const ExprNode_>(node)->isConst_; }

        //  Are all the arguments_ to this node constant?
        //  Note the arguments_ must be of ExprNode_ type
        static bool ConstArgs(const Node_& node, const size_t first = 0) {
            for (size_t i = first; i < node.arguments_.size(); ++i) {
                if (!ConstArg(node.arguments_[i]))
                    return false;
            }
            return true;
        }

    public:
        using Visitor_<ConstProcessor_>::Visit;

        //	Constructor, nVar = number of variables, from Product after parsing and variable indexation
        //  All variables start as constants with value 0
        ConstProcessor_(const size_t nVar) : varConst_(nVar, true), varConstVal_(nVar, 0.0), isInConditional_(false) {}

        //	Visitors

        //	Expressions

        //	Binaries

        template <class OP> void VisitBinary(ExprNode_& node, const OP op) {
            VisitArguments(node);
            if (ConstArgs(node)) {
                node.isConst_ = true;

                const double lhs = Downcast<ExprNode_>(node.arguments_[0])->constVal_;
                const double rhs = Downcast<ExprNode_>(node.arguments_[1])->constVal_;
                node.constVal_ = op(lhs, rhs);
            }
        }

        void Visit(NodeAdd& node) {
            VisitBinary(node, [](const double& x, const double& y) { return x + y; });
        }
        void Visit(NodeSub& node) {
            VisitBinary(node, [](const double& x, const double& y) { return x - y; });
        }
        void Visit(NodeMult& node) {
            VisitBinary(node, [](const double& x, const double& y) { return x * y; });
        }
        void Visit(NodeDiv& node) {
            VisitBinary(node, [](const double& x, const double& y) { return x / y; });
        }
        void Visit(NodePow& node) {
            VisitBinary(node, [](const double& x, const double& y) { return pow(x, y); });
        }
        void Visit(NodeMax& node) {
            VisitBinary(node, [](const double& x, const double& y) { return max(x, y); });
        }
        void Visit(NodeMin& node) {
            VisitBinary(node, [](const double& x, const double& y) { return min(x, y); });
        }

        //	Unaries
        template <class OP> void VisitUnary(ExprNode_& node, const OP op) {
            VisitArguments(node);
            if (ConstArgs(node)) {
                node.isConst_ = true;

                const double arg = Downcast<ExprNode_>(node.arguments_[0])->constVal_;
                node.constVal_ = op(arg);
            }
        }

        void Visit(NodeUplus& node) {
            VisitUnary(node, [](const double& x) { return x; });
        }
        void Visit(NodeUminus& node) {
            VisitUnary(node, [](const double& x) { return -x; });
        }

        //	Functions
        void Visit(NodeLog& node) {
            VisitUnary(node, [](const double& x) { return log(x); });
        }
        void Visit(NodeSqrt& node) {
            VisitUnary(node, [](const double& x) { return sqrt(x); });
        }

        //  Multies

        void Visit(NodeSmooth& node) {
            VisitArguments(node);
            if (ConstArgs(node)) {
                node.isConst_ = true;

                const double x = reinterpret_cast<ExprNode_*>(node.arguments_[0].get())->constVal_;
                const double vPos = reinterpret_cast<ExprNode_*>(node.arguments_[1].get())->constVal_;
                const double vNeg = reinterpret_cast<ExprNode_*>(node.arguments_[2].get())->constVal_;
                const double halfEps = 0.5 * reinterpret_cast<ExprNode_*>(node.arguments_[3].get())->constVal_;

                if (x < -halfEps)
                    node.constVal_ = vNeg;
                else if (x > halfEps)
                    node.constVal_ = vPos;
                else {
                    node.constVal_ = vNeg + 0.5 * (vPos - vNeg) / halfEps * (x + halfEps);
                }
            }
        }

        //	If
        void Visit(NodeIf& node) {
            //  Mark conditional

            //  Identify nested
            bool nested = isInConditional_;

            //  Mark
            if (!nested)
                isInConditional_ = true;

            //  Visit arguments_
            VisitArguments(node);

            //  Reset (unless nested)
            if (!nested)
                isInConditional_ = false;
        }

        void Visit(NodeAssign& node) {
            //  Get index from LHS
            const size_t varIndex = Downcast<const NodeVar>(node.arguments_[0])->index_;

            //  Visit RHS
            node.arguments_[1]->Accept(*this);

            //  All conditional assignments result in non const vars
            if (!isInConditional_) {
                //  RHS constant?
                if (ConstArg(node.arguments_[1])) {
                    varConst_[varIndex] = true;
                    varConstVal_[varIndex] = Downcast<const ExprNode_>(node.arguments_[1])->constVal_;
                } else {
                    varConst_[varIndex] = false;
                }
            } else {
                varConst_[varIndex] = false;
            }
        }

        void Visit(NodePays& node) {
            //  A payment is always non constant because it is normalized by a possibly stochastic numeraire
            const size_t varIndex = Downcast<const NodeVar>(node.arguments_[0])->index_;
            varConst_[varIndex] = false;

            //  Visit RHS
            node.arguments_[1]->Accept(*this);
        }

        //	Variables, RHS only, we don't Visit LHS vars
        void Visit(NodeVar& node) {
            if (varConst_[node.index_]) {
                node.isConst_ = true;
                node.constVal_ = varConstVal_[node.index_];
            } else {
                node.isConst_ = false;
            }
        }

        //  We don't Visit boolean nodes, that is best left to fuzzy logic

        //  We don't Visit constants (which are always const) or spots (which are never const)
    };
}

