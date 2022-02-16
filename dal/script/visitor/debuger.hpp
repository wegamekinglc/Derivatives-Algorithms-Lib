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

        void Visit(const NodeCollect_* node) override {
            Debug(*node, "COLLECT");
        }

        void Visit(const NodeTrue_* node) override {
            Debug(*node, "TRUE");
        }

        void Visit(const NodeFalse_* node) override {
            Debug(*node, "FALSE");
        }

        void Visit(const NodeUPlus_* node) override {
            Debug(*node, "U-PLUS");
        }

        void Visit(const NodeUMinus_* node) override {
            Debug(*node, "U-Minus");
        }

        void Visit(const NodePlus_* node) override {
            Debug(*node, "PLUS");
        }

        void Visit(const NodeMinus_* node) override {
            Debug(*node, "MINUS");
        }

        void Visit(const NodeMultiply_* node) override {
            Debug(*node, "MULTIPLY");
        }

        void Visit(const NodeDivide_* node) override {
            Debug(*node, "DIV");
        }

        void Visit(const NodePower_* node) override {
            Debug(*node, "POW");
        }

        void Visit(const NodeLog_* node) override {
            Debug(*node, "LOG");
        }

        void Visit(const NodeSqrt_* node) override {
            Debug(*node, "SQRT");
        }

        void Visit(const NodeMax_* node) override {
            Debug(*node, "MAX");
        }

        void Visit(const NodeMin_* node) override {
            Debug(*node, "MIN");
        }

    };
}