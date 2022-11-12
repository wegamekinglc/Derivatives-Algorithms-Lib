//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/experimental/node.hpp>

namespace Dal::Script::Experimental {
    template <class D_>
    class Visitor_ {

    protected:
        Visitor_() = default;

        template<class T_>
        void VisitArguments(T_ &node) {
            for (auto &arg: node.arguments_)
                this->Visit(arg);
        }

    public:

        void Visit(std::unique_ptr<ScriptNode_>& node) {
            if (auto item = std::get_if<NodeCollect_>(&*node)) {
                static_cast<D_*>(this)->VisitCollect(*item);
            } else if (auto item = std::get_if<NodeTrue_>(&*node)) {
                static_cast<D_*>(this)->VisitTrue(*item);
            } else if (auto item = std::get_if<NodeFalse_>(&*node)) {
                static_cast<D_*>(this)->VisitFalse(*item);
            } else if (auto item = std::get_if<NodeUPlus_>(&*node)) {
                static_cast<D_*>(this)->VisitUPlus(*item);
            } else if (auto item = std::get_if<NodeUMinus_>(&*node)) {
                static_cast<D_*>(this)->VisitUMinus(*item);
            } else if (auto item = std::get_if<NodePlus_>(&*node)) {
                static_cast<D_*>(this)->VisitPlus(*item);
            } else if (auto item = std::get_if<NodeMinus_>(&*node)) {
                static_cast<D_*>(this)->VisitMinus(*item);
            } else if (auto item = std::get_if<NodeMultiply_>(&*node)) {
                static_cast<D_*>(this)->VisitMultiply(*item);
            } else if (auto item = std::get_if<NodeDivide_>(&*node)) {
                static_cast<D_*>(this)->VisitDivide(*item);
            } else if (auto item = std::get_if<NodePower_>(&*node)) {
                static_cast<D_*>(this)->VisitPower(*item);
            } else if (auto item = std::get_if<NodeLog_>(&*node)) {
                static_cast<D_*>(this)->VisitLog(*item);
            } else if (auto item = std::get_if<NodeSqrt_>(&*node)) {
                static_cast<D_*>(this)->VisitSqrt(*item);
            } else if (auto item = std::get_if<NodeMax_>(&*node)) {
                static_cast<D_*>(this)->VisitMax(*item);
            } else if (auto item = std::get_if<NodeMin_>(&*node)) {
                static_cast<D_*>(this)->VisitMin(*item);
            } else if (auto item = std::get_if<NodeConst_>(&*node)) {
                static_cast<D_*>(this)->VisitConst(*item);
            } else if (auto item = std::get_if<NodeVar_>(&*node)) {
                static_cast<D_*>(this)->VisitVar(*item);
            } else if (auto item = std::get_if<NodeAssign_>(&*node)) {
                static_cast<D_*>(this)->VisitAssign(*item);
            } else if (auto item = std::get_if<NodeIf_>(&*node)) {
                static_cast<D_*>(this)->VisitIf(*item);
            } else if (auto item = std::get_if<NodeEqual_>(&*node)) {
                static_cast<D_*>(this)->VisitEqual(*item);
            } else if (auto item = std::get_if<NodeNot_>(&*node)) {
                static_cast<D_*>(this)->VisitNot(*item);
            } else if (auto item = std::get_if<NodeSuperior_>(&*node)) {
                static_cast<D_*>(this)->VisitSuperior(*item);
            } else if (auto item = std::get_if<NodeSupEqual_>(&*node)) {
                static_cast<D_*>(this)->VisitSupEqual(*item);
            } else if (auto item = std::get_if<NodeAnd_>(&*node)) {
                static_cast<D_*>(this)->VisitAnd(*item);
            } else if (auto item = std::get_if<NodeOr_>(&*node)) {
                static_cast<D_*>(this)->VisitOr(*item);
            } else if (auto item = std::get_if<NodePays_>(&*node)) {
                static_cast<D_*>(this)->VisitPays(*item);
            } else if (auto item = std::get_if<NodeSpot_>(&*node)) {
                static_cast<D_*>(this)->VisitSpot(*item);
            } else if (auto item = std::get_if<NodeSmooth_>(&*node)) {
                static_cast<D_*>(this)->VisitSmooth(*item);
            }
        }

        inline void VisitCollect(NodeCollect_ &node) { VisitArguments(node); }

        inline void VisitTrue(NodeTrue_ &node) { VisitArguments(node); }

        inline void VisitFalse(NodeFalse_ &node) { VisitArguments(node); }

        inline void VisitUPlus(NodeUPlus_ &node) { VisitArguments(node); }

        inline void VisitUMinus(NodeUMinus_ &node) { VisitArguments(node); }

        inline void VisitPlus(NodePlus_ &node) { VisitArguments(node); }

        inline void VisitMinus(NodeMinus_ &node) { VisitArguments(node); }

        inline void VisitMultiply(NodeMultiply_ &node) { VisitArguments(node); }

        inline void VisitDivide(NodeDivide_ &node) { VisitArguments(node); }

        inline void VisitPower(NodePower_ &node) { VisitArguments(node); }

        inline void VisitLog(NodeLog_ &node) { VisitArguments(node); }

        inline void VisitSqrt(NodeSqrt_ &node) { VisitArguments(node); }

        inline void VisitMax(NodeMax_ &node) { VisitArguments(node); }

        inline void VisitMin(NodeMin_ &node) { VisitArguments(node); }

        inline void VisitConst(NodeConst_ &node) { VisitArguments(node); }

        inline void VisitVar(NodeVar_ &node) { VisitArguments(node); }

        inline void VisitAssign(NodeAssign_ &node) { VisitArguments(node); }

        inline void VisitIf(NodeIf_ &node) { VisitArguments(node); }

        inline void VisitEqual(NodeEqual_ &node) { VisitArguments(node); }

        inline void VisitSupEqual(NodeSupEqual_ &node) { VisitArguments(node); }

        inline void VisitSuperior(NodeSuperior_ &node) { VisitArguments(node); }

        inline void VisitAnd(NodeAnd_ &node) { VisitArguments(node); }

        inline void VisitNot(NodeNot_ &node) { VisitArguments(node); }

        inline void VisitOr(NodeOr_ &node) { VisitArguments(node); }

        inline void VisitPays(NodePays_ &node) { VisitArguments(node); }

        inline void VisitSpot(NodeSpot_ &node) { VisitArguments(node); }

        inline void VisitSmooth(NodeSmooth_ &node) { VisitArguments(node); }
    };


    template <class D_>
    class ConstVisitor_ {
    protected:
        ConstVisitor_() = default;

        template<class T_>
        void VisitArguments(const T_ &node) {
            for (auto &arg: node.arguments_)
                this->Visit(arg);
        }

    public:
        void Visit(const std::unique_ptr<ScriptNode_>& node) {
            if (auto item = std::get_if<NodeCollect_>(&*node)) {
                static_cast<D_*>(this)->VisitCollect(*item);
            } else if (auto item = std::get_if<NodeTrue_>(&*node)) {
                static_cast<D_*>(this)->VisitTrue(*item);
            } else if (auto item = std::get_if<NodeFalse_>(&*node)) {
                static_cast<D_*>(this)->VisitFalse(*item);
            } else if (auto item = std::get_if<NodeUPlus_>(&*node)) {
                static_cast<D_*>(this)->VisitUPlus(*item);
            } else if (auto item = std::get_if<NodeUMinus_>(&*node)) {
                static_cast<D_*>(this)->VisitUMinus(*item);
            } else if (auto item = std::get_if<NodePlus_>(&*node)) {
                static_cast<D_*>(this)->VisitPlus(*item);
            } else if (auto item = std::get_if<NodeMinus_>(&*node)) {
                static_cast<D_*>(this)->VisitMinus(*item);
            } else if (auto item = std::get_if<NodeMultiply_>(&*node)) {
                static_cast<D_*>(this)->VisitMultiply(*item);
            } else if (auto item = std::get_if<NodeDivide_>(&*node)) {
                static_cast<D_*>(this)->VisitDivide(*item);
            } else if (auto item = std::get_if<NodePower_>(&*node)) {
                static_cast<D_*>(this)->VisitPower(*item);
            } else if (auto item = std::get_if<NodeLog_>(&*node)) {
                static_cast<D_*>(this)->VisitLog(*item);
            } else if (auto item = std::get_if<NodeSqrt_>(&*node)) {
                static_cast<D_*>(this)->VisitSqrt(*item);
            } else if (auto item = std::get_if<NodeMax_>(&*node)) {
                static_cast<D_*>(this)->VisitMax(*item);
            } else if (auto item = std::get_if<NodeMin_>(&*node)) {
                static_cast<D_*>(this)->VisitMin(*item);
            } else if (auto item = std::get_if<NodeConst_>(&*node)) {
                static_cast<D_*>(this)->VisitConst(*item);
            } else if (auto item = std::get_if<NodeVar_>(&*node)) {
                static_cast<D_*>(this)->VisitVar(*item);
            } else if (auto item = std::get_if<NodeAssign_>(&*node)) {
                static_cast<D_*>(this)->VisitAssign(*item);
            } else if (auto item = std::get_if<NodeIf_>(&*node)) {
                static_cast<D_*>(this)->VisitIf(*item);
            } else if (auto item = std::get_if<NodeEqual_>(&*node)) {
                static_cast<D_*>(this)->VisitEqual(*item);
            } else if (auto item = std::get_if<NodeNot_>(&*node)) {
                static_cast<D_*>(this)->VisitNot(*item);
            } else if (auto item = std::get_if<NodeSuperior_>(&*node)) {
                static_cast<D_*>(this)->VisitSuperior(*item);
            } else if (auto item = std::get_if<NodeSupEqual_>(&*node)) {
                static_cast<D_*>(this)->VisitSupEqual(*item);
            } else if (auto item = std::get_if<NodeAnd_>(&*node)) {
                static_cast<D_*>(this)->VisitAnd(*item);
            } else if (auto item = std::get_if<NodeOr_>(&*node)) {
                static_cast<D_*>(this)->VisitOr(*item);
            } else if (auto item = std::get_if<NodePays_>(&*node)) {
                static_cast<D_*>(this)->VisitPays(*item);
            } else if (auto item = std::get_if<NodeSpot_>(&*node)) {
                static_cast<D_*>(this)->VisitSpot(*item);
            } else if (auto item = std::get_if<NodeSmooth_>(&*node)) {
                static_cast<D_*>(this)->VisitSmooth(*item);
            }
        }

        inline void VisitCollect(const NodeCollect_ &node) { VisitArguments(node); }

        inline void VisitTrue(const NodeTrue_ &node) { VisitArguments(node); }

        inline void VisitFalse(const NodeFalse_ &node) { VisitArguments(node); }

        inline void VisitUPlus(const NodeUPlus_ &node) { VisitArguments(node); }

        inline void VisitUMinus(const NodeUMinus_ &node) { VisitArguments(node); }

        inline void VisitPlus(const NodePlus_ &node) { VisitArguments(node); }

        inline void VisitMinus(const NodeMinus_ &node) { VisitArguments(node); }

        inline void VisitMultiply(const NodeMultiply_ &node) { VisitArguments(node); }

        inline void VisitDivide(const NodeDivide_ &node) { VisitArguments(node); }

        inline void VisitPower(const NodePower_ &node) { VisitArguments(node); }

        inline void VisitLog(const NodeLog_ &node) { VisitArguments(node); }

        inline void VisitSqrt(const NodeSqrt_ &node) { VisitArguments(node); }

        inline void VisitMax(const NodeMax_ &node) { VisitArguments(node); }

        inline void VisitMin(const NodeMin_ &node) { VisitArguments(node); }

        inline void VisitConst(const NodeConst_ &node) { VisitArguments(node); }

        inline void VisitVar(const NodeVar_ &node) { VisitArguments(node); }

        inline void VisitAssign(const NodeAssign_ &node) { VisitArguments(node); }

        inline void VisitIf(const NodeIf_ &node) { VisitArguments(node); }

        inline void VisitEqual(const NodeEqual_ &node) { VisitArguments(node); }

        inline void VisitSupEqual(const NodeSupEqual_ &node) { VisitArguments(node); }

        inline void VisitSuperior(const NodeSuperior_ &node) { VisitArguments(node); }

        inline void VisitAnd(const NodeAnd_ &node) { VisitArguments(node); }

        inline void VisitNot(const NodeNot_ &node) { VisitArguments(node); }

        inline void VisitOr(const NodeOr_ &node) { VisitArguments(node); }

        inline void VisitPays(const NodePays_ &node) { VisitArguments(node); }

        inline void VisitSpot(const NodeSpot_ &node) { VisitArguments(node); }

        inline void VisitSmooth(const NodeSmooth_ &node) { VisitArguments(node); }
    };
}
