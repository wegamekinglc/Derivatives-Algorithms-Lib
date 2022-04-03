//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <memory>
#include <dal/string/strings.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::Script {

    class Visitor_;
    class ConstVisitor_;

    struct Node_;

    struct Node_ {
        Vector_<std::unique_ptr<Node_>> arguments_;
        virtual ~Node_() = default;
        virtual void AcceptVisitor(Visitor_* visitor) = 0;
        virtual void AcceptVisitor(ConstVisitor_* visitor) const = 0;
    };

    template <class ConcreteNode_, typename... Args_>
    std::unique_ptr<ConcreteNode_> MakeNode(Args_&&... args) {
        return std::unique_ptr<ConcreteNode_>(new ConcreteNode_(std::forward<Args_>(args)...));
    }

    template <class ConcreteNode_, typename... Args_>
    std::unique_ptr<Node_> MakeBaseNode(Args_&&... args) {
        return std::unique_ptr<Node_>(new ConcreteNode_(std::forward<Args_>(args)...));
    }

    template <class NodeType_>
    std::unique_ptr<Node_> BuildBinary(std::unique_ptr<Node_>& lhs,
                                       std::unique_ptr<Node_>& rhs) {
        std::unique_ptr<Node_> top = MakeBaseNode<NodeType_>();
        top->arguments_.Resize(2);
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
        return top;
    }

    template <class NodeType_>
    std::unique_ptr<NodeType_> BuildConcreteBinary(std::unique_ptr<Node_>& lhs, std::unique_ptr<Node_>& rhs) {
        std::unique_ptr<NodeType_> top = MakeNode<NodeType_>();
        top->arguments_.Resize(2);
        top->arguments_[0] = std::move(lhs);
        top->arguments_[1] = std::move(rhs);
    }

    struct NodeCollect_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeTrue_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeFalse_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeUPlus_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeUMinus_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodePlus_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeMinus_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeMultiply_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeDivide_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodePower_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeLog_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeSqrt_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeMax_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeMin_: public Node_ {
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeConst_: public Node_ {
        double val_;
        explicit NodeConst_(double val): val_(val) {}
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };

    struct NodeVar_: public Node_ {
        String_ name_;
        int index_;
        explicit NodeVar_(const String_& name): name_(name), index_(-1) {}
        void AcceptVisitor(Visitor_* visitor) override;
        void AcceptVisitor(ConstVisitor_* visitor) const override;
    };
}
