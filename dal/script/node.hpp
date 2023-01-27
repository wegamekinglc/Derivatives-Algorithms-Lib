//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/script/visitor.hpp>
#include <dal/string/strings.hpp>
#include <memory>
#include <variant>

namespace Dal::Script {
    struct Node;
    using ExprTree = std::unique_ptr<Node>;
    using Expression = ExprTree;
    using Statement = ExprTree;
    using Event_ = Vector_<Statement>;

    struct Node : VisitableBase<VISITORS> {
        using VisitableBase<VISITORS>::accept;
        Vector_<ExprTree> arguments;
        virtual ~Node() {}
    };

    //  Hierarchy

    //  Nodes that return a number
    struct exprNode : Node {
        bool isConst = false;
        double constVal;
    };

    //  Action nodes
    struct actNode : Node {};

    //  Nodes that return a bool
    struct boolNode : Node {
        bool alwaysTrue;
        bool alwaysFalse;
    };

    //  All the concrete nodes

    //  Binary expressions

    struct NodeAdd : Visitable<exprNode, NodeAdd, VISITORS> {};
    struct NodeSub : Visitable<exprNode, NodeSub, VISITORS> {};
    struct NodeMult : Visitable<exprNode, NodeMult, VISITORS> {};
    struct NodeDiv : Visitable<exprNode, NodeDiv, VISITORS> {};
    struct NodePow : Visitable<exprNode, NodePow, VISITORS> {};
    struct NodeMax : Visitable<exprNode, NodeMax, VISITORS> {};
    struct NodeMin : Visitable<exprNode, NodeMin, VISITORS> {};

    //  Unary expressions

    struct NodeUplus : Visitable<exprNode, NodeUplus, VISITORS> {};
    struct NodeUminus : Visitable<exprNode, NodeUminus, VISITORS> {};

    //	Math operators

    struct NodeLog : Visitable<exprNode, NodeLog, VISITORS> {};
    struct NodeSqrt : Visitable<exprNode, NodeSqrt, VISITORS> {};

    //  Multi expressions

    struct NodeSmooth : Visitable<exprNode, NodeSmooth, VISITORS> {};

    //  Comparisons

    struct compNode : boolNode {
        //	Fuzzying stuff
        bool discrete; //	Continuous or discrete
                       //	Continuous eps
        double eps;
        //	Discrete butterfly bounds
        double lb;
        double rb;
        //	End of fuzzying stuff
    };

    struct NodeEqual : Visitable<compNode, NodeEqual, VISITORS> {};

    struct NodeSup : Visitable<compNode, NodeSup, VISITORS> {};

    struct NodeSupEqual : Visitable<compNode, NodeSupEqual, VISITORS> {};

    //	And/or/not

    struct NodeAnd : Visitable<boolNode, NodeAnd, VISITORS> {};

    struct NodeOr : Visitable<boolNode, NodeOr, VISITORS> {};

    struct NodeNot : Visitable<boolNode, NodeNot, VISITORS> {};

    //  Leaves

    //	Market access
    struct NodeSpot : Visitable<exprNode, NodeSpot, VISITORS> {};

    //  Const
    struct NodeConst : Visitable<exprNode, NodeConst, VISITORS> {
        NodeConst(const double val) {
            isConst = true;
            constVal = val;
        }
    };

    struct NodeTrue : Visitable<boolNode, NodeTrue, VISITORS> {
        NodeTrue() { alwaysTrue = true; }
    };

    struct NodeFalse : Visitable<boolNode, NodeFalse, VISITORS> {
        NodeFalse() { alwaysFalse = true; }
    };

    //  Variable
    struct NodeVar : Visitable<exprNode, NodeVar, VISITORS> {
        NodeVar(const String_& n) : name(n), index(-1) {
            isConst = true;
            constVal = 0.0;
        }

        const String_ name;
        int index;
    };

    //	Assign, Pays

    struct NodeAssign : Visitable<actNode, NodeAssign, VISITORS> {};

    struct NodePays : Visitable<actNode, NodePays, VISITORS> {};

    //	If
    struct NodeIf : Visitable<actNode, NodeIf, VISITORS> {
        int firstElse;
        //	For fuzzy eval: indices of variables affected in statements, including nested
        Vector_<size_t> affectedVars;
        //	Always true/false as per domain processor
        bool alwaysTrue;
        bool alwaysFalse;
    };

    //	Collection of statements
    struct NodeCollect : Visitable<actNode, NodeCollect, VISITORS> {};

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