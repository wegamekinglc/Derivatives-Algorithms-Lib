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

    struct NodeAdd : public Visitable<exprNode, NodeAdd, VISITORS> {
        using Visitable<exprNode, NodeAdd, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeSub : public Visitable<exprNode, NodeSub, VISITORS> {
        using Visitable<exprNode, NodeSub, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeMult : public Visitable<exprNode, NodeMult, VISITORS> {
        using Visitable<exprNode, NodeMult, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeDiv : public Visitable<exprNode, NodeDiv, VISITORS> {
        using Visitable<exprNode, NodeDiv, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodePow : public Visitable<exprNode, NodePow, VISITORS> {
        using Visitable<exprNode, NodePow, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeMax : public Visitable<exprNode, NodeMax, VISITORS> {
        using Visitable<exprNode, NodeMax, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeMin : public Visitable<exprNode, NodeMin, VISITORS> {
        using Visitable<exprNode, NodeMin, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };

    //  Unary expressions

    struct NodeUplus : public Visitable<exprNode, NodeUplus, VISITORS> {
        using Visitable<exprNode, NodeUplus, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeUminus : public Visitable<exprNode, NodeUminus, VISITORS> {
        using Visitable<exprNode, NodeUminus, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };

    //	Math operators

    struct NodeLog : public Visitable<exprNode, NodeLog, VISITORS> {
        using Visitable<exprNode, NodeLog, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };
    struct NodeSqrt : public Visitable<exprNode, NodeSqrt, VISITORS> {
        using Visitable<exprNode, NodeSqrt, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };

    //  Multi expressions

    struct NodeSmooth : public Visitable<exprNode, NodeSmooth, VISITORS> {
        using Visitable<exprNode, NodeSmooth, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };

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

    struct NodeEqual : public Visitable<compNode, NodeEqual, VISITORS> {
        using Visitable<compNode, NodeEqual, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
        using compNode::discrete;
        using compNode::eps;
        using compNode::lb;
        using compNode::rb;
    };

    struct NodeSup : public Visitable<compNode, NodeSup, VISITORS> {
        using Visitable<compNode, NodeSup, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
        using compNode::discrete;
        using compNode::eps;
        using compNode::lb;
        using compNode::rb;
    };

    struct NodeSupEqual : public Visitable<compNode, NodeSupEqual, VISITORS> {
        using Visitable<compNode, NodeSupEqual, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
        using compNode::discrete;
        using compNode::eps;
        using compNode::lb;
        using compNode::rb;
    };

    //	And/or/not

    struct NodeAnd : public Visitable<boolNode, NodeAnd, VISITORS> {
        using Visitable<boolNode, NodeAnd, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
    };

    struct NodeOr : public Visitable<boolNode, NodeOr, VISITORS> {
        using Visitable<boolNode, NodeOr, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
    };

    struct NodeNot : public Visitable<boolNode, NodeNot, VISITORS> {
        using Visitable<boolNode, NodeNot, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;
    };

    //  Leaves

    //	Market access
    struct NodeSpot : public Visitable<exprNode, NodeSpot, VISITORS> {
        using Visitable<exprNode, NodeSpot, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;
    };

    //  Const
    struct NodeConst : public Visitable<exprNode, NodeConst, VISITORS> {
        using Visitable<exprNode, NodeConst, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;

        NodeConst(const double val) {
            isConst = true;
            constVal = val;
        }
    };

    struct NodeTrue : public Visitable<boolNode, NodeTrue, VISITORS> {
        using Visitable<boolNode, NodeTrue, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;

        NodeTrue() { alwaysTrue = true; }
    };

    struct NodeFalse : public Visitable<boolNode, NodeFalse, VISITORS> {
        using Visitable<boolNode, NodeFalse, VISITORS>::accept;
        using Node::arguments;
        using boolNode::alwaysTrue;
        using boolNode::alwaysFalse;

        NodeFalse() { alwaysFalse = true; }
    };

    //  Variable
    struct NodeVar : public Visitable<exprNode, NodeVar, VISITORS> {
        using Visitable<exprNode, NodeVar, VISITORS>::accept;
        using Node::arguments;
        using exprNode::isConst;
        using exprNode::constVal;

        NodeVar(const String_& n) : name(n), index(-1) {
            isConst = true;
            constVal = 0.0;
        }

        const String_ name;
        int index;
    };

    //	Assign, Pays

    struct NodeAssign : public Visitable<actNode, NodeAssign, VISITORS> {
        using Visitable<actNode, NodeAssign, VISITORS>::accept;
        using Node::arguments;
    };

    struct NodePays : public Visitable<actNode, NodePays, VISITORS> {
        using Visitable<actNode, NodePays, VISITORS>::accept;
        using Node::arguments;
    };

    //	If
    struct NodeIf : public Visitable<actNode, NodeIf, VISITORS> {
        using Visitable<actNode, NodeIf, VISITORS>::accept;
        using Node::arguments;

        int firstElse;
        //	For fuzzy eval: indices of variables affected in statements, including nested
        Vector_<size_t> affectedVars;
        //	Always true/false as per domain processor
        bool alwaysTrue;
        bool alwaysFalse;
    };

    //	Collection of statements
    struct NodeCollect : public Visitable<actNode, NodeCollect, VISITORS> {
        using Visitable<actNode, NodeCollect, VISITORS>::accept;
        using Node::arguments;
    };

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