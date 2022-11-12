//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <variant>
#include <memory>
#include <dal/string/strings.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::Script::Experimental {

    struct NodeCollect_;
    struct NodeTrue_;
    struct NodeFalse_;
    struct NodeUPlus_;
    struct NodeUMinus_;
    struct NodePlus_;
    struct NodeMinus_;
    struct NodeMultiply_;
    struct NodeDivide_;
    struct NodePower_;
    struct NodeLog_;
    struct NodeSqrt_;
    struct NodeMax_;
    struct NodeMin_;
    struct NodeConst_;
    struct NodeVar_;
    struct NodeAssign_;
    struct NodeIf_;
    struct NodeEqual_;
    struct NodeNot_;
    struct NodeSuperior_;
    struct NodeSupEqual_;
    struct NodeAnd_;
    struct NodeOr_;
    struct NodePays_;
    struct NodeSpot_;
    struct NodeSmooth_;

    using ScriptNode_ = std::variant<NodeCollect_,
            NodeTrue_,
            NodeFalse_,
            NodeUPlus_,
            NodeUMinus_,
            NodePlus_,
            NodeMinus_,
            NodeMultiply_,
            NodeDivide_,
            NodePower_,
            NodeLog_,
            NodeSqrt_,
            NodeMax_,
            NodeMin_,
            NodeConst_,
            NodeVar_,
            NodeAssign_,
            NodeIf_,
            NodeEqual_,
            NodeNot_,
            NodeSuperior_,
            NodeSupEqual_,
            NodeAnd_,
            NodeOr_,
            NodePays_,
            NodeSpot_,
            NodeSmooth_>;

    struct BaseNode_ {
        Vector_<std::unique_ptr<ScriptNode_>> arguments_;
    };

    struct NodeCollect_ : public BaseNode_ {
    };

    struct NodeTrue_ : public BaseNode_ {
    };

    struct NodeFalse_ : public BaseNode_ {
    };

    struct NodeUPlus_ : public BaseNode_ {
    };

    struct NodeUMinus_ : public BaseNode_ {
    };

    struct NodePlus_ : public BaseNode_ {
    };

    struct NodeMinus_ : public BaseNode_ {
    };

    struct NodeMultiply_ : public BaseNode_ {
    };

    struct NodeDivide_ : public BaseNode_ {
    };

    struct NodePower_ : public BaseNode_ {
    };

    struct NodeLog_ : public BaseNode_ {
    };

    struct NodeSqrt_ : public BaseNode_ {

    };

    struct NodeMax_ : public BaseNode_ {
    };

    struct NodeMin_ : public BaseNode_ {
    };

    struct NodeConst_ : public BaseNode_ {
        double val_;
        explicit NodeConst_(double val) : val_(val) {}

    };

    struct NodeVar_ : public BaseNode_ {
        String_ name_;
        int index_;
        explicit NodeVar_(String_ &&name) : name_(std::move(name)), index_(-1) {}
    };

    struct NodeAssign_ : public BaseNode_ {

    };

    struct NodeIf_ : public BaseNode_ {
        int firstElse_;
        Vector_<int> affectedVars_;
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    struct NodeEqual_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
        bool discrete_;
        double eps_;
        double lb_;
        double ub_;
    };

    struct NodeNot_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    struct NodeSuperior_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
        bool discrete_;
        double eps_;
        double lb_;
        double ub_;
    };

    struct NodeSupEqual_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
        bool discrete_;
        double eps_;
        double lb_;
        double ub_;
    };

    struct NodeAnd_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    struct NodeOr_ : public BaseNode_ {
        bool alwaysTrue_;
        bool alwaysFalse_;
    };

    struct NodePays_ : public BaseNode_ {
    };

    struct NodeSpot_ : public BaseNode_ {
    };

    struct NodeSmooth_ : public BaseNode_ {
    };

    template<class ConcreteNode_, typename... Args_>
    std::unique_ptr<ScriptNode_> MakeBaseNode(Args_ &&... args) {
        return std::make_unique<ScriptNode_>(ConcreteNode_(std::forward<Args_>(args)...));
    }

    template<class NodeType_>
    std::unique_ptr<ScriptNode_> BuildBinary(std::unique_ptr<ScriptNode_>& lhs, std::unique_ptr<ScriptNode_>& rhs) {
        auto top = std::make_unique<ScriptNode_>(NodeType_());
        std::get<NodeType_>(*top).arguments_.Resize(2);
        std::get<NodeType_>(*top).arguments_[0] = std::move(lhs);
        std::get<NodeType_>(*top).arguments_[1] = std::move(rhs);
        return top;
    }


}