//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <dal/script/nodebase.hpp>
#include <dal/string/strings.hpp>
#include <memory>
#include <variant>

namespace Dal::Script {
    //  Hierarchy

    //  Nodes that return a number
    struct ExprNode_ : public Node_ {
        bool isConst_ = false;
        double constVal_;
    };

    //  Action nodes
    struct ActNode_ : public Node_ {};

    //  Nodes that return a bool
    struct BoolNode_ : public Node_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    //  All the concrete nodes

    //  VisitBinary expressions

    struct NodeAdd : public Visitable_<ExprNode_, NodeAdd, VISITORS> {};
    struct NodeSub : public Visitable_<ExprNode_, NodeSub, VISITORS> {};
    struct NodeMult : public Visitable_<ExprNode_, NodeMult, VISITORS> {};
    struct NodeDiv : public Visitable_<ExprNode_, NodeDiv, VISITORS> {};
    struct NodePow : public Visitable_<ExprNode_, NodePow, VISITORS> {};
    struct NodeMax : public Visitable_<ExprNode_, NodeMax, VISITORS> {};
    struct NodeMin : public Visitable_<ExprNode_, NodeMin, VISITORS> {};

    //  Unary expressions

    struct NodeUplus : public Visitable_<ExprNode_, NodeUplus, VISITORS> {};
    struct NodeUminus : public Visitable_<ExprNode_, NodeUminus, VISITORS> {};

    //	Math operators

    struct NodeLog : public Visitable_<ExprNode_, NodeLog, VISITORS> {};
    struct NodeSqrt : public Visitable_<ExprNode_, NodeSqrt, VISITORS> {};

    //  Multi expressions

    struct NodeSmooth : public Visitable_<ExprNode_, NodeSmooth, VISITORS> {};

    //  Comparisons

    struct CompNode_ : public BoolNode_ {
        //	Fuzzying stuff
        bool isDiscrete_; //	Continuous or isDiscrete_
                       //	Continuous eps_
        double eps_;
        //	Discrete butterfly bounds
        double lb_;
        double rb_;
        //	End of fuzzying stuff
    };

    struct NodeEqual : public Visitable_<CompNode_, NodeEqual, VISITORS> {};

    struct NodeSup : public Visitable_<CompNode_, NodeSup, VISITORS> {};

    struct NodeSupEqual : public Visitable_<CompNode_, NodeSupEqual, VISITORS> {};

    //	And/or/not

    struct NodeAnd : public Visitable_<BoolNode_, NodeAnd, VISITORS> {};

    struct NodeOr : public Visitable_<BoolNode_, NodeOr, VISITORS> {};

    struct NodeNot : public Visitable_<BoolNode_, NodeNot, VISITORS> {};

    //  Leaves

    //	Market access
    struct NodeSpot : public Visitable_<ExprNode_, NodeSpot, VISITORS> {};

    //  Const
    struct NodeConst : public Visitable_<ExprNode_, NodeConst, VISITORS> {
        NodeConst(const double val) {
            ExprNode_::isConst_ = true;
            ExprNode_::constVal_ = val;
        }
    };

    struct NodeTrue : public Visitable_<BoolNode_, NodeTrue, VISITORS> {
        NodeTrue() { BoolNode_::alwaysTrue_ = true; }
    };

    struct NodeFalse : public Visitable_<BoolNode_, NodeFalse, VISITORS> {
        NodeFalse() { alwaysFalse_ = true; }
    };

    //  Variable
    struct NodeVar : public Visitable_<ExprNode_, NodeVar, VISITORS> {
        NodeVar(const String_& n) : name_(n), index_(-1) {
            ExprNode_::isConst_ = true;
            ExprNode_::constVal_ = 0.0;
        }

        const String_ name_;
        int index_;
    };

    //	Assign, Pays

    struct NodeAssign : public Visitable_<ActNode_, NodeAssign, VISITORS> {};

    struct NodePays : public Visitable_<ActNode_, NodePays, VISITORS> {};

    //	If
    struct NodeIf : public Visitable_<ActNode_, NodeIf, VISITORS> {
        int firstElse_;
        //	For fuzzy eval: indices of variables affected in statements, including nested
        Vector_<size_t> affectedVars_;
        //	Always true/false as per domain processor
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    //	Collection of statements
    struct NodeCollect : public Visitable_<ActNode_, NodeCollect, VISITORS> {};

    //	Utilities

    //  Downcast Node_ to concrete type, crashes if used on another Node_ type

    template <class Concrete> const Concrete* Downcast(const std::unique_ptr<Node_>& node) {
        return static_cast<const Concrete*>(node.get());
    }

    template <class Concrete> Concrete* Downcast(std::unique_ptr<Node_>& node) { return static_cast<Concrete*>(node.get()); }

    //  Factories

    //  Make concrete node
    template <typename ConcreteNode, typename... Args> std::unique_ptr<ConcreteNode> MakeNode(Args&&... args) {
        return std::unique_ptr<ConcreteNode>(new ConcreteNode(std::forward<Args>(args)...));
    }

    //  Same but return as pointer on base
    template <typename ConcreteNode, typename... Args> std::unique_ptr<Node_> MakeBaseNode(Args&&... args) {
        return std::unique_ptr<Node_>(new ConcreteNode(std::forward<Args>(args)...));
    }

    //	Build binary concrete, and set its arguments_ to lhs and rhs
    template <class NodeType> std::unique_ptr<NodeType> MakeBinary(ExprTree_& lhs, ExprTree_& rhs) {
        auto top = MakeNode<NodeType>();
        top->arguments_.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        //	Return
        return top;
    }

    //  Same but return as pointer on base
    template <class ConcreteNode> ExprTree_ MakeBaseBinary(ExprTree_& lhs, ExprTree_& rhs) {
        auto top = MakeBaseNode<ConcreteNode>();
        top->arguments_.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        //	Return
        return top;
    }
} // namespace Dal::Script