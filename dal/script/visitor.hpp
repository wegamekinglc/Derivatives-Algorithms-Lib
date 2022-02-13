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
    };

    class ConstVisitor_ {
    public:
        virtual ~ConstVisitor_() = default;

    protected:
        ConstVisitor_() = default;

        virtual void VisitArguments(Node_* node) {
            for (auto& arg: node->arguments_)
                arg->AcceptVisitor(this);
        }

    public:
        void Visit(const std::unique_ptr<Node_>& tree) {
            tree->AcceptVisitor(this);
        }
    };
}
