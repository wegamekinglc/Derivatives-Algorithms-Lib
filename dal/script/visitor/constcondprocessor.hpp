//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/script/visitor.hpp>
#include <memory>

namespace Dal::Script {

    class ConstCondProcessor_ : public Visitor_ {
        std::unique_ptr<ScriptNode_>* current_;
        void VisitArguments(ScriptNode_* node) override;

    public:
        void ProcessFromTop(std::unique_ptr<ScriptNode_>& top);

        template<class NodeCond_>
        void VisitCondT(NodeCond_* node) {
            if (node->alwaysTrue_)
                *current_ = std::make_unique<NodeTrue_>();
            else if(node->alwaysFalse_)
                *current_ = std::make_unique<NodeFalse_>();
            else
                VisitArguments(node);
        }

        void Visit(NodeEqual_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeNot_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeSuperior_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeSupEqual_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeAnd_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeOr_* node) override {
            VisitCondT(node);
        }

        void Visit(NodeIf_* node) override;
    };

}
