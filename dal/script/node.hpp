//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <memory>
#include <variant>
#include <dal/script/nodebase.hpp>
#include <dal/string/strings.hpp>


namespace Dal::Script {
    //  Hierarchy

    //  Nodes that return a number
    struct ExprNode_ : public Node_ {
        bool isConst_ = false;
        double constVal_ = 0.0;
    };

    //  Action nodes
    struct ActNode_ : public Node_ {};

    //  Nodes that return a bool
    struct BoolNode_ : public Node_ {
        bool alwaysTrue_ = false;
        bool alwaysFalse_ = false;
    };

    //  All the concrete nodes

    //  VisitBinary expressions

    struct NodeAdd_ : public Visitable_<ExprNode_, NodeAdd_, VISITORS> {};
    struct NodeSub_ : public Visitable_<ExprNode_, NodeSub_, VISITORS> {};
    struct NodeMulti_ : public Visitable_<ExprNode_, NodeMulti_, VISITORS> {};
    struct NodeDiv_ : public Visitable_<ExprNode_, NodeDiv_, VISITORS> {};
    struct NodePow_ : public Visitable_<ExprNode_, NodePow_, VISITORS> {};
    struct NodeMax_ : public Visitable_<ExprNode_, NodeMax_, VISITORS> {};
    struct NodeMin_ : public Visitable_<ExprNode_, NodeMin_, VISITORS> {};

    //  Unary expressions
    struct NodeUPlus_ : public Visitable_<ExprNode_, NodeUPlus_, VISITORS> {};
    struct NodeUMinus_ : public Visitable_<ExprNode_, NodeUMinus_, VISITORS> {};

    //	Math operators
    struct NodeLog_ : public Visitable_<ExprNode_, NodeLog_, VISITORS> {};
    struct NodeSqrt_ : public Visitable_<ExprNode_, NodeSqrt_, VISITORS> {};
    struct NodeExp_ : public Visitable_<ExprNode_, NodeExp_, VISITORS> {};

    //  Comparisons

    struct CompNode_ : public BoolNode_ {
        // Fuzzying stuff
        bool isDiscrete_ = false; // Continuous or isDiscrete_
        double eps_ = 0.0; //	Continuous eps
        // Discrete butterfly bounds
        double lb_ = 0.0;
        double rb_ = 0.0;
    };

    struct NodeEqual_ : public Visitable_<CompNode_, NodeEqual_, VISITORS> {};

    struct NodeSup_ : public Visitable_<CompNode_, NodeSup_, VISITORS> {};

    struct NodeSupEqual_ : public Visitable_<CompNode_, NodeSupEqual_, VISITORS> {};

    //	And/or/not

    struct NodeAnd_ : public Visitable_<BoolNode_, NodeAnd_, VISITORS> {};

    struct NodeOr_ : public Visitable_<BoolNode_, NodeOr_, VISITORS> {};

    struct NodeNot_ : public Visitable_<BoolNode_, NodeNot_, VISITORS> {};

    //  Leaves

    //	Market access
    struct NodeSpot_ : public Visitable_<ExprNode_, NodeSpot_, VISITORS> {};

    //  Const
    struct NodeConst_ : public Visitable_<ExprNode_, NodeConst_, VISITORS> {
        explicit NodeConst_(double val) {
            ExprNode_::isConst_ = true;
            ExprNode_::constVal_ = val;
        }
    };

    struct NodeTrue_ : public Visitable_<BoolNode_, NodeTrue_, VISITORS> {
        NodeTrue_() { BoolNode_::alwaysTrue_ = true; }
    };

    struct NodeFalse_ : public Visitable_<BoolNode_, NodeFalse_, VISITORS> {
        NodeFalse_() { alwaysFalse_ = true; }
    };

    //  Variable
    struct NodeVar_ : public Visitable_<ExprNode_, NodeVar_, VISITORS> {
        explicit NodeVar_(String_ name) : name_(std::move(name)), index_(-1) {
            ExprNode_::isConst_ = true;
            ExprNode_::constVal_ = 0.0;
        }

        const String_ name_;
        int index_;
    };

    struct NodeConstVar_: public Visitable_<ExprNode_, NodeConstVar_, VISITORS> {
        explicit NodeConstVar_(String_ name, double val) : name_(std::move(name)), index_(-1) {
            ExprNode_::isConst_ = true;
            ExprNode_::constVal_ = val;
        }

        const String_ name_;
        int index_;
    };

    //	Assign, Pays

    struct NodeAssign_ : public Visitable_<ActNode_, NodeAssign_, VISITORS> {};

    struct NodePays_ : public Visitable_<ActNode_, NodePays_, VISITORS> {};

    //	If
    struct NodeIf_ : public Visitable_<ActNode_, NodeIf_, VISITORS> {
        int firstElse_ = 0;
        //	For fuzzy eval: indices of variables affected in statements, including nested
        Vector_<size_t> affectedVars_;
        //	Always true/false as per domain processor
        bool alwaysTrue_ = false;
        bool alwaysFalse_ = false;
    };

    //	Collection of statements
    struct NodeCollect_ : public Visitable_<ActNode_, NodeCollect_, VISITORS> {};

    //	Utilities

    //  Downcast Node_ to concrete type, crashes if used on another Node_ type

    template <class Concrete_> FORCE_INLINE const Concrete_* Downcast(const std::unique_ptr<Node_>& node) {
        return static_cast<const Concrete_*>(node.get());
    }

    template <class Concrete_> FORCE_INLINE Concrete_* Downcast(std::unique_ptr<Node_>& node) {
        return static_cast<Concrete_*>(node.get());
    }

    //  Factories

    //  Make concrete node
    template <typename ConcreteNode_, typename... Args> std::unique_ptr<ConcreteNode_> MakeNode(Args&&... args) {
        return std::unique_ptr<ConcreteNode_>(new ConcreteNode_(std::forward<Args>(args)...));
    }

    //  Same but return as pointer on base
    template <typename ConcreteNode_, typename... Args> std::unique_ptr<Node_> MakeBaseNode(Args&&... args) {
        return std::unique_ptr<Node_>(new ConcreteNode_(std::forward<Args>(args)...));
    }

    //	Build binary concrete, and set its arguments_ to lhs and rhs
    template <class NodeType_> std::unique_ptr<NodeType_> MakeBinary(ExprTree_& lhs, ExprTree_& rhs) {
        auto top = MakeNode<NodeType_>();
        top->arguments_.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        //	Return
        return top;
    }

    //  Same but return as pointer on base
    template <class ConcreteNode_> ExprTree_ MakeBaseBinary(ExprTree_& lhs, ExprTree_& rhs) {
        auto top = MakeBaseNode<ConcreteNode_>();
        top->arguments_.Resize(2);
        //	Take ownership of lhs and rhs
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        //	Return
        return top;
    }
} // namespace Dal::Script