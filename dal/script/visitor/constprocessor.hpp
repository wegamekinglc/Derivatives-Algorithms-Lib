/*
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

#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {
    class ConstProcessor_ : public Visitor_<ConstProcessor_> {
    protected:
        // State

        // Const's status of variables
        Vector_<char> varConst_;
        Vector_<double> varConstVal_;

        // Inside an if?
        bool isInConditional_;

        // Is this node a IsConstant?
        // Note the argument must be of ExprNode_ type
        static bool ConstArg(const ExprTree_& node) { return Downcast<const ExprNode_>(node)->isConst_; }

        // Are all the arguments_ to this node IsConstant?
        // Note the arguments_ must be of ExprNode_ type
        static bool ConstArgs(const Node_& node, const size_t first = 0) {
            for (size_t i = first; i < node.arguments_.size(); ++i) {
                if (!ConstArg(node.arguments_[i]))
                    return false;
            }
            return true;
        }

    public:
        using Visitor_<ConstProcessor_>::Visit;

        // Constructor, nVar = number of variables, from Product after parsing and variable indexation
        // All variables start as constants with value 0
        explicit ConstProcessor_(const size_t n_var)
            : varConst_(n_var, true), varConstVal_(n_var, 0.0), isInConditional_(false) {}

        // Visitors
        // Expressions
        // Binaries
        template <class OP_> void VisitBinary(ExprNode_& node, const OP_& op) {
            VisitArguments(node);
            if (ConstArgs(node)) {
                node.isConst_ = true;
                const double lhs = Downcast<ExprNode_>(node.arguments_[0])->constVal_;
                const double rhs = Downcast<ExprNode_>(node.arguments_[1])->constVal_;
                node.constVal_ = op(lhs, rhs);
            }
        }

        void Visit(NodeAdd_& node) {
            VisitBinary(node, [](double x, double y) { return x + y; });
        }
        void Visit(NodeSub_& node) {
            VisitBinary(node, [](double x, double y) { return x - y; });
        }
        void Visit(NodeMulti_& node) {
            VisitBinary(node, [](double x, double y) { return x * y; });
        }
        void Visit(NodeDiv_& node) {
            VisitBinary(node, [](double x, double y) { return x / y; });
        }
        void Visit(NodePow_& node) {
            VisitBinary(node, [](double x, double y) { return pow(x, y); });
        }
        void Visit(NodeMax_& node) {
            VisitBinary(node, [](double x, double y) { return max(x, y); });
        }
        void Visit(NodeMin_& node) {
            VisitBinary(node, [](double x, double y) { return min(x, y); });
        }

        // Unary
        template <class OP_> void VisitUnary(ExprNode_& node, const OP_& op) {
            VisitArguments(node);
            if (ConstArgs(node)) {
                node.isConst_ = true;
                const auto arg = Downcast<ExprNode_>(node.arguments_[0])->constVal_;
                node.constVal_ = op(arg);
            }
        }

        void Visit(NodeUPlus_& node) {
            VisitUnary(node, [](double x) { return x; });
        }
        void Visit(NodeUMinus_& node) {
            VisitUnary(node, [](double x) { return -x; });
        }

        // Functions
        void Visit(NodeLog_& node) {
            VisitUnary(node, [](double x) { return log(x); });
        }

        void Visit(NodeSqrt_& node) {
            VisitUnary(node, [](double x) { return sqrt(x); });
        }

        void Visit(NodeExp_& node) {
            VisitUnary(node, [](double x) { return exp(x); });
        }

        // If
        void Visit(NodeIf_& node) {
            // Mark conditional
            // Identify nested
            bool nested = isInConditional_;

            // Mark
            if (!nested)
                isInConditional_ = true;

            // Visit arguments_
            VisitArguments(node);

            // Reset (unless nested)
            if (!nested)
                isInConditional_ = false;
        }

        void Visit(NodeAssign_& node) {
            // Get index from LHS
            const size_t varIndex = Downcast<const NodeVar_>(node.arguments_[0])->index_;

            // Visit RHS
            node.arguments_[1]->Accept(*this);

            // All conditional assignments result in non const vars
            if (!isInConditional_) {
                //  RHS IsConstant?
                if (ConstArg(node.arguments_[1])) {
                    varConst_[varIndex] = true;
                    varConstVal_[varIndex] = Downcast<const ExprNode_>(node.arguments_[1])->constVal_;
                } else
                    varConst_[varIndex] = false;
            } else
                varConst_[varIndex] = false;
        }

        void Visit(NodePays_& node) {
            // A payment is always non IsConstant because it is normalized by a possibly stochastic numeraire
            const size_t varIndex = Downcast<const NodeVar_>(node.arguments_[0])->index_;
            varConst_[varIndex] = false;

            // Visit RHS
            node.arguments_[1]->Accept(*this);
        }

        // Variables, RHS only, we don't Visit LHS vars
        void Visit(NodeVar_& node) {
            if (varConst_[node.index_]) {
                node.isConst_ = true;
                node.constVal_ = varConstVal_[node.index_];
            } else
                node.isConst_ = false;
        }

        // We don't Visit IsBoolean nodes, that is best left to fuzzy logic
        // We don't Visit constants (which are always const) or spots (which are never const)
    };
}

