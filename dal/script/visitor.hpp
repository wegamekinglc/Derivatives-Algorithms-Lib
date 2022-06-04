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

        virtual void VisitArguments(ScriptNode_* node) {
            for (auto& arg: node->arguments_)
                arg->AcceptVisitor(this);
        }

    public:
        void Visit(const std::unique_ptr<ScriptNode_>& tree) {
            tree->AcceptVisitor(this);
        }

        virtual void Visit(NodeCollect_* node) { VisitArguments(node); }
        virtual void Visit(NodeTrue_* node) { VisitArguments(node); }
        virtual void Visit(NodeFalse_* node) { VisitArguments(node); }
        virtual void Visit(NodeUPlus_* node) { VisitArguments(node); }
        virtual void Visit(NodeUMinus_* node) { VisitArguments(node); }
        virtual void Visit(NodePlus_* node) { VisitArguments(node); }
        virtual void Visit(NodeMinus_* node) { VisitArguments(node); }
        virtual void Visit(NodeMultiply_* node) { VisitArguments(node); }
        virtual void Visit(NodeDivide_* node) { VisitArguments(node); }
        virtual void Visit(NodePower_* node) { VisitArguments(node); }
        virtual void Visit(NodeLog_* node) { VisitArguments(node); }
        virtual void Visit(NodeSqrt_* node) { VisitArguments(node); }
        virtual void Visit(NodeMax_* node) { VisitArguments(node); }
        virtual void Visit(NodeMin_* node) { VisitArguments(node); }
        virtual void Visit(NodeConst_* node) { VisitArguments(node); }
        virtual void Visit(NodeVar_* node) { VisitArguments(node); }
        virtual void Visit(NodeAssign_* node) { VisitArguments(node); }
        virtual void Visit(NodeIf_* node) { VisitArguments(node); }
        virtual void Visit(NodeEqual_* node) { VisitArguments(node); }
        virtual void Visit(NodeSupEqual_* node) { VisitArguments(node); }
        virtual void Visit(NodeSuperior_* node) { VisitArguments(node); }
        virtual void Visit(NodeAnd_* node) { VisitArguments(node); }
        virtual void Visit(NodeNot_* node) { VisitArguments(node); }
        virtual void Visit(NodeOr_* node) { VisitArguments(node); }
        virtual void Visit(NodePays_* node) { VisitArguments(node); }
    };

    class ConstVisitor_ {
    public:
        virtual ~ConstVisitor_() = default;

    protected:
        ConstVisitor_() = default;

        virtual void VisitArguments(const ScriptNode_* node) {
            for (auto& arg: node->arguments_)
                arg->AcceptVisitor(this);
        }

    public:
        void Visit(const std::unique_ptr<ScriptNode_>& tree) {
            tree->AcceptVisitor(this);
        }

        virtual void Visit(const NodeCollect_* node) { VisitArguments(node);}
        virtual void Visit(const NodeTrue_* node) { VisitArguments(node); }
        virtual void Visit(const NodeFalse_* node) { VisitArguments(node); }
        virtual void Visit(const NodeUPlus_* node) { VisitArguments(node); }
        virtual void Visit(const NodeUMinus_* node) { VisitArguments(node); }
        virtual void Visit(const NodePlus_* node) { VisitArguments(node); }
        virtual void Visit(const NodeMinus_* node) { VisitArguments(node); }
        virtual void Visit(const NodeMultiply_* node) { VisitArguments(node); }
        virtual void Visit(const NodeDivide_* node) { VisitArguments(node); }
        virtual void Visit(const NodePower_* node) { VisitArguments(node); }
        virtual void Visit(const NodeLog_* node) { VisitArguments(node); }
        virtual void Visit(const NodeSqrt_* node) { VisitArguments(node); }
        virtual void Visit(const NodeMax_* node) { VisitArguments(node); }
        virtual void Visit(const NodeMin_* node) { VisitArguments(node); }
        virtual void Visit(const NodeConst_* node) { VisitArguments(node); }
        virtual void Visit(const NodeVar_* node) { VisitArguments(node); }
        virtual void Visit(const NodeAssign_* node) { VisitArguments(node); }
        virtual void Visit(const NodeIf_* node) { VisitArguments(node); }
        virtual void Visit(const NodeEqual_* node) { VisitArguments(node); }
        virtual void Visit(const NodeSupEqual_* node) { VisitArguments(node); }
        virtual void Visit(const NodeSuperior_* node) { VisitArguments(node); }
        virtual void Visit(const NodeAnd_* node) { VisitArguments(node); }
        virtual void Visit(const NodeNot_* node) { VisitArguments(node); }
        virtual void Visit(const NodeOr_* node) { VisitArguments(node); }
        virtual void Visit(const NodePays_* node) { VisitArguments(node); }
    };
}
