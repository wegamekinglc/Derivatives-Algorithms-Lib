//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <stack>
#include <dal/string/strings.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {
    class Debugger_: public ConstVisitor_ {
        String_ prefix_;
        std::stack<String_> stack_;

        void Debug(const Node_& node, const String_& nodeId) {
            prefix_ += '\t';
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                (*it)->AcceptVisitor(this);

            prefix_.pop_back();

            String_ str(prefix_ + nodeId);
            if (!node.arguments_.empty()) {
                str += "(\n";
                str += stack_.top();
                stack_.pop();
                if (node.arguments_.size() > 1)
                    str += prefix_ + ",\n";
                for (size_t i = 1; i < node.arguments_.size() - 1; ++i) {
                    str += stack_.top() + prefix_ + ",\n";
                    stack_.pop();
                }
                if (node.arguments_.size() > 1) {
                    str += stack_.top();
                    stack_.pop();
                }

                str += prefix_ + ')';
            }
            str += '\n';
            stack_.push(std::move(str));
        }

    public:
        String_ String() const {
            return stack_.top();
        }

        void VisitCollect(const NodeCollect_* node) override {
            Debug(*node, "COLLECT");
        }

        void VisitTrue(const NodeTrue_* node) override {
            Debug(*node, "TRUE");
        }

        void VisitFalse(const NodeFalse_* node) override {
            Debug(*node, "FALSE");
        }
    };
}