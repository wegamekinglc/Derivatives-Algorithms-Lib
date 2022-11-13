//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/script/visitor.hpp>
#include <memory>

namespace Dal::Script {

    class ConstCondProcessor_ : public Visitor_<ConstCondProcessor_> {
        ScriptNode_* current_;
        template <class N_>
        void VisitArguments(N_& node) {
            for (auto &arg: node.arguments_) {
                current_ = &arg;
                this->Visit(arg);
            }
        }

    public:
        void ProcessFromTop(ScriptNode_& top);

        template<class NodeCond_>
        void VisitCondT(NodeCond_& node) {
            if (node->alwaysTrue_)
                *current_ = std::make_unique<NodeTrue_>();
            else if(node->alwaysFalse_)
                *current_ = std::make_unique<NodeFalse_>();
            else
                VisitArguments(*node);
        }

        using Visitor_<ConstCondProcessor_>::operator();
        
        void operator()(std::unique_ptr<NodeEqual_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeNot_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeSuperior_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeSupEqual_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeAnd_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeOr_>& node) {
            VisitCondT(node);
        }

        void operator()(std::unique_ptr<NodeIf_>& node);
    };

}
