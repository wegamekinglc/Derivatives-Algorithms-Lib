//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/experimental/node.hpp>
#include <dal/script/experimental/visit.hpp>

namespace Dal::Script::Experimental {
    template <class D_>
    class Visitor_ {
    protected:
        Visitor_() = default;

        template <class N_> void VisitArguments(N_& node) {
            for (auto& arg : node.arguments_)
                this->Visit(arg);
        }

    public:
        void Visit(ScriptNode_& node) {
            static auto sub = static_cast<D_*>(this);
            std::visit(*sub, node);
        }

        void operator()(std::unique_ptr<NodeCollect_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeTrue_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeFalse_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeUPlus_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeUMinus_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodePlus_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeMinus_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeMultiply_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeDivide_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodePower_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeLog_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeSqrt_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeMax_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeMin_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeConst_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeVar_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeAssign_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeIf_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeEqual_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeSupEqual_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeSuperior_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeAnd_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeNot_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeOr_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodePays_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeSpot_> &node) { VisitArguments(*node); }

        void operator()(std::unique_ptr<NodeSmooth_> &node) { VisitArguments(*node); }
    };

    template <class D_>
    class ConstVisitor_ {
    protected:
        ConstVisitor_() = default;

        template <class N_> void VisitArguments(const N_& node) {
            for (auto& arg : node.arguments_)
                this->Visit(arg);
        }

    public:
        void Visit(const ScriptNode_& node) {
            static auto sub = static_cast<D_*>(this);
            std::visit(*sub, node);
        }

        void operator()(const std::unique_ptr<NodeCollect_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeTrue_> &node) {
            VisitArguments(*node);
        }

        void operator()(const std::unique_ptr<NodeFalse_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeUPlus_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeUMinus_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodePlus_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeMinus_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeMultiply_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeDivide_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodePower_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeLog_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeSqrt_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeMax_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeMin_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeConst_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeVar_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeAssign_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeIf_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeEqual_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeSupEqual_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeSuperior_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeAnd_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeNot_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeOr_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodePays_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeSpot_> &node) { VisitArguments(*node); }

        void operator()(const std::unique_ptr<NodeSmooth_> &node) { VisitArguments(*node); }
    };
} // namespace Dal::Script::Experimental
