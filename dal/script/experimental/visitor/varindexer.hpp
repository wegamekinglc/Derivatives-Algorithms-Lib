//
// Created by wegam on 2022/5/21.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/script/experimental/node.hpp>
#include <dal/script/experimental/visitor.hpp>


namespace Dal::Script::Experimental {

    class VarIndexer_ {
        std::map<String_, int> varMap_;

        template<class N_>
        void VisitArguments(N_ &node) {
            for (auto &arg: node.arguments_)
                this->Visit(arg);
        }

    public:
        [[nodiscard]] Vector_<String_> GetVarNames() const;

        void Visit(ScriptNode_& node) {
            if (auto item = std::get_if<std::unique_ptr<NodeConst_>>(&node)) {
                VisitConst(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodePlus_>>(&node)) {
                VisitPlus(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeMinus_>>(&node)) {
                VisitMinus(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeTrue_>>(&node)) {
                VisitTrue(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeFalse_>>(&node)) {
                VisitFalse(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeUPlus_>>(&node)) {
                VisitUPlus(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeUMinus_>>(&node)) {
                VisitUMinus(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeMultiply_>>(&node)) {
                VisitMultiply(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeDivide_>>(&node)) {
                VisitDivide(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodePower_>>(&node)) {
                VisitPower(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeLog_>>(&node)) {
                VisitLog(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeSqrt_>>(&node)) {
                VisitSqrt(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeMax_>>(&node)) {
                VisitMax(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeMin_>>(&node)) {
                VisitMin(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeCollect_>>(&node)) {
                VisitCollect(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeVar_>>(&node)) {
                VisitVar(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeAssign_>>(&node)) {
                VisitAssign(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeIf_>>(&node)) {
                VisitIf(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeEqual_>>(&node)) {
                VisitEqual(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeNot_>>(&node)) {
                VisitNot(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeSuperior_>>(&node)) {
                VisitSuperior(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeSupEqual_>>(&node)) {
                VisitSupEqual(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeAnd_>>(&node)) {
                VisitAnd(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeOr_>>(&node)) {
                VisitOr(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodePays_>>(&node)) {
                VisitPays(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeSpot_>>(&node)) {
                VisitSpot(**item);
            } else if (auto item = std::get_if<std::unique_ptr<NodeSmooth_>>(&node)) {
                VisitSmooth(**item);
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

        inline void VisitVar(NodeVar_ &node) {
            auto it = varMap_.find(node.name_);
            if (it == varMap_.end())
                node.index_ = varMap_[node.name_] = varMap_.size();
            else
                node.index_ = it->second;
        }

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

}
