//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <stack>
#include <dal/string/strings.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <dal/math/stacks.hpp>

namespace Dal::Script {
    class Debugger_: public ConstVisitor_ {
        String_ prefix_;
        Stack_<String_> stack_;

        void Debug(const ScriptNode_& node, const String_& nodeId);

    public:
        String_ String() const;

        void Visit(const NodeCollect_* node) override;
        void Visit(const NodeTrue_* node) override;
        void Visit(const NodeFalse_* node) override;
        void Visit(const NodeUPlus_* node) override;
        void Visit(const NodeUMinus_* node) override;
        void Visit(const NodePlus_* node) override;
        void Visit(const NodeMinus_* node) override;
        void Visit(const NodeMultiply_* node) override;
        void Visit(const NodeDivide_* node) override;
        void Visit(const NodePower_* node) override;
        void Visit(const NodeLog_* node) override;
        void Visit(const NodeSqrt_* node) override;
        void Visit(const NodeMax_* node) override;
        void Visit(const NodeMin_* node) override;
        void Visit(const NodeConst_* node) override;
        void Visit(const NodeVar_* node) override;
        void Visit(const NodeAssign_* node) override;
        void Visit(const NodeIf_* node) override;
        void Visit(const NodeEqual_* node) override;
        void Visit(const NodeNot_* node) override;
        void Visit(const NodeSuperior_* node) override;
        void Visit(const NodeSupEqual_* node) override;
        void Visit(const NodeAnd_* node) override;
        void Visit(const NodeOr_* node) override;
        void Visit(const NodePays_* node) override;
        void Visit(const NodeSpot_* node) override;
    };
}