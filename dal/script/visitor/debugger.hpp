//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <dal/string/strings.hpp>


namespace Dal::Script {
    class Debugger_ : public ConstVisitor_<Debugger_> {
        String_ prefix_;
        Stack_<String_> stack_;

        template <class T_>
        void Debug(const T_& node, const String_& nodeId) {
            prefix_ += " ";
            bool isEmpty = node->arguments_.empty();
            if (!isEmpty)
                for (auto it = node->arguments_.rbegin(); it != node->arguments_.rend(); ++it)
                    this->Visit(*it);

            prefix_.pop_back();

            String_ str(prefix_ + nodeId);
            if (!isEmpty) {
                str += "(\n";
                str += stack_.Top();
                stack_.Pop();
                if (node->arguments_.size() > 1)
                    str += prefix_ + ",\n";
                for (size_t i = 1; i < node->arguments_.size() - 1; ++i) {
                    str += stack_.Top() + prefix_ + ",\n";
                    stack_.Pop();
                }
                if (node->arguments_.size() > 1) {
                    str += stack_.Top();
                    stack_.Pop();
                }

                str += prefix_ + ')';
            }
            str += '\n';
            stack_.Push(std::move(str));
        }

    public:
        [[nodiscard]] String_ String() const;

        void operator()(const std::unique_ptr<NodeCollect_>& node);
        void operator()(const std::unique_ptr<NodeTrue_>& node);
        void operator()(const std::unique_ptr<NodeFalse_>& node);
        void operator()(const std::unique_ptr<NodeUPlus_>& node);
        void operator()(const std::unique_ptr<NodeUMinus_>& node);
        void operator()(const std::unique_ptr<NodePlus_>& node);
        void operator()(const std::unique_ptr<NodeMinus_>& node);
        void operator()(const std::unique_ptr<NodeMultiply_>& node);
        void operator()(const std::unique_ptr<NodeDivide_>& node);
        void operator()(const std::unique_ptr<NodePower_>& node);
        void operator()(const std::unique_ptr<NodeLog_>& node);
        void operator()(const std::unique_ptr<NodeSqrt_>& node);
        void operator()(const std::unique_ptr<NodeMax_>& node);
        void operator()(const std::unique_ptr<NodeMin_>& node);
        void operator()(const std::unique_ptr<NodeConst_>& node);
        void operator()(const std::unique_ptr<NodeVar_>& node);
        void operator()(const std::unique_ptr<NodeAssign_>& node);
        void operator()(const std::unique_ptr<NodeIf_>& node);
        void operator()(const std::unique_ptr<NodeEqual_>& node);
        void operator()(const std::unique_ptr<NodeNot_>& node);
        void operator()(const std::unique_ptr<NodeSuperior_>& node);
        void operator()(const std::unique_ptr<NodeSupEqual_>& node);
        void operator()(const std::unique_ptr<NodeAnd_>& node);
        void operator()(const std::unique_ptr<NodeOr_>& node);
        void operator()(const std::unique_ptr<NodePays_>& node);
        void operator()(const std::unique_ptr<NodeSpot_>& node);
        void operator()(const std::unique_ptr<NodeSmooth_>& node);
    };
} // namespace Dal::Script