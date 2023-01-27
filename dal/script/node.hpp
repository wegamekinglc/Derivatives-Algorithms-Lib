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
    struct exprNode : public Node {
        bool isConst = false;
        double constVal;
    };

    //  Action nodes
    struct actNode : public Node {};

    //  Nodes that return a bool
    struct boolNode : public Node {
        bool alwaysTrue;
        bool alwaysFalse;
    };

    //  All the concrete nodes

    //  Binary expressions

    struct NodeAdd : public Visitable<exprNode, NodeAdd, VISITORS> {};
    struct NodeSub : public Visitable<exprNode, NodeSub, VISITORS> {};
    struct NodeMult : public Visitable<exprNode, NodeMult, VISITORS> {};
    struct NodeDiv : public Visitable<exprNode, NodeDiv, VISITORS> {};
    struct NodePow : public Visitable<exprNode, NodePow, VISITORS> {};
    struct NodeMax : public Visitable<exprNode, NodeMax, VISITORS> {};
    struct NodeMin : public Visitable<exprNode, NodeMin, VISITORS> {};

    //  Unary expressions

    struct NodeUplus : public Visitable<exprNode, NodeUplus, VISITORS> {};
    struct NodeUminus : public Visitable<exprNode, NodeUminus, VISITORS> {};

    //	Math operators

    struct NodeLog : public Visitable<exprNode, NodeLog, VISITORS> {};
    struct NodeSqrt : public Visitable<exprNode, NodeSqrt, VISITORS> {};

    //  Multi expressions

    struct NodeSmooth : public Visitable<exprNode, NodeSmooth, VISITORS> {};

    //  Comparisons

    struct compNode : public boolNode {
        //	Fuzzying stuff
        bool discrete; //	Continuous or discrete
                       //	Continuous eps
        double eps;
        //	Discrete butterfly bounds
        double lb;
        double rb;
        //	End of fuzzying stuff
    };

    struct NodeEqual : public Visitable<compNode, NodeEqual, VISITORS> {};

    struct NodeSup : public Visitable<compNode, NodeSup, VISITORS> {};

    struct NodeSupEqual : public Visitable<compNode, NodeSupEqual, VISITORS> {};

    //	And/or/not

    struct NodeAnd : public Visitable<boolNode, NodeAnd, VISITORS> {};

    struct NodeOr : public Visitable<boolNode, NodeOr, VISITORS> {};

    struct NodeNot : public Visitable<boolNode, NodeNot, VISITORS> {};

    //  Leaves

    //	Market access
    struct NodeSpot : public Visitable<exprNode, NodeSpot, VISITORS> {};

    //  Const
    struct NodeConst : public Visitable<exprNode, NodeConst, VISITORS> {
        NodeConst(const double val) {
            exprNode::isConst = true;
            exprNode::constVal = val;
        }
    };

    struct NodeTrue : public Visitable<boolNode, NodeTrue, VISITORS> {
        NodeTrue() { boolNode::alwaysTrue = true; }
    };

    struct NodeFalse : public Visitable<boolNode, NodeFalse, VISITORS> {
        NodeFalse() { alwaysFalse = true; }
    };

    //  Variable
    struct NodeVar : public Visitable<exprNode, NodeVar, VISITORS> {
        NodeVar(const String_& n) : name(n), index(-1) {
            exprNode::isConst = true;
            exprNode::constVal = 0.0;
        }

        const String_ name;
        int index;
    };

    //	Assign, Pays

    struct NodeAssign : public Visitable<actNode, NodeAssign, VISITORS> {};

    struct NodePays : public Visitable<actNode, NodePays, VISITORS> {};

    //	If
    struct NodeIf : public Visitable<actNode, NodeIf, VISITORS> {
        int firstElse;
        //	For fuzzy eval: indices of variables affected in statements, including nested
        Vector_<size_t> affectedVars;
        //	Always true/false as per domain processor
        bool alwaysTrue;
        bool alwaysFalse;
    };

    //	Collection of statements
    struct NodeCollect : public Visitable<actNode, NodeCollect, VISITORS> {};

    //	Utilities

    //  Downcast Node to concrete type, crashes if used on another Node type

    template <class Concrete> const Concrete* downcast(const std::unique_ptr<Node>& node) {
        return static_cast<const Concrete*>(node.get());
    }

    template <class Concrete> Concrete* downcast(std::unique_ptr<Node>& node) { return static_cast<Concrete*>(node.get()); }

    //  Factories

    //  Make concrete node
    template <typename ConcreteNode, typename... Args> std::unique_ptr<ConcreteNode> MakeNode(Args&&... args) {
        return std::unique_ptr<ConcreteNode>(new ConcreteNode(std::forward<Args>(args)...));
    }

    //  Same but return as pointer on base
    template <typename ConcreteNode, typename... Args> std::unique_ptr<Node> MakeBaseNode(Args&&... args) {
        return std::unique_ptr<Node>(new ConcreteNode(std::forward<Args>(args)...));
    }

    //	Build binary concrete, and set its arguments to lhs and rhs
    template <class NodeType> std::unique_ptr<NodeType> MakeBinary(ExprTree& lhs, ExprTree& rhs) {
        auto top = MakeNode<NodeType>();
        top->arguments.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments[0] = std::move(lhs);
        top->arguments[1] = std::move(rhs);
        //	Return
        return top;
    }

    //  Same but return as pointer on base
    template <class ConcreteNode> ExprTree MakeBaseBinary(ExprTree& lhs, ExprTree& rhs) {
        auto top = MakeBaseNode<ConcreteNode>();
        top->arguments.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments[0] = std::move(lhs);
        top->arguments[1] = std::move(rhs);
        //	Return
        return top;
    }
} // namespace Dal::Script