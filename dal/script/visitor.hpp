//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <dal/script/node.hpp>

namespace Dal::Script {
    class Visitor_ {
    public:
        virtual ~Visitor_() = default;

    protected:
        Visitor_() = default;

        virtual void VisitArguments(Node_* node) {
            for (auto& arg: node->arguments_)
                arg->AcceptVisitor(this);
        }

    public:
        void Visit(const std::unique_ptr<Node_>& tree) {
            tree->AcceptVisitor(this);
        }

        virtual void VisitCollect(NodeCollect_* node) { VisitArguments(node); }
        virtual void VisitTrue(NodeTrue_* node) { VisitArguments(node); }
        virtual void VisitFalse(NodeFalse_* node) { VisitArguments(node); }
    };

    class ConstVisitor_ {
    public:
        virtual ~ConstVisitor_() = default;

    protected:
        ConstVisitor_() = default;

        virtual void VisitArguments(const Node_* node) {
            for (auto& arg: node->arguments_)
                arg->AcceptVisitor(this);
        }

    public:
        void Visit(const std::unique_ptr<Node_>& tree) {
            tree->AcceptVisitor(this);
        }

        virtual void VisitCollect(const NodeCollect_* node) { VisitArguments(node);}
        virtual void VisitTrue(const NodeTrue_* node) { VisitArguments(node); }
        virtual void VisitFalse(const NodeFalse_* node) { VisitArguments(node); }
    };
}
