//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <memory>
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
        return new ConcreteNode_(std::forward<Args_>(args)...);
    }

    template <class ConcreteNode_, typename... Args_>
    std::unique_ptr<Node_> MakeBaseNode(Args_&&... args) {
        return new ConcreteNode_(std::forward<Args_>(args)...);
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

}