//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <memory>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    // Collapses always-true and always-false condition/if nodes into concrete true/false nodes
    // or statement collections. DomainProcessor_ must run first to set the flags.
    class ConstCondProcessor_ : public Visitor_<ConstCondProcessor_> {
        ExprTree_* current_;

        void VisitArgsSetCurrent(Node_& node) {
            for (auto& arg : node.arguments_) {
                current_ = &arg;
                arg->Accept(*this);
            }
        }

    public:
        ConstCondProcessor_() = default;

        template <class N_>
        std::enable_if_t<std::is_same<N_, std::remove_const_t<N_>>::value && !HasConstVisit_<ConstCondProcessor_>::ForNodeType<N_>()>
        Visit(N_& node) {
            VisitArgsSetCurrent(node);
        }

        // Must be called from the root — this visitor mutates the tree structure.
        void ProcessFromTop(std::unique_ptr<Node_>& top) {
            current_ = &top;
            top->Accept(*this);
        }

        // Conditions — one handler for all boolean nodes
        void VisitBool(BoolNode_& node) {
            if (node.alwaysTrue_)
                *current_ = std::unique_ptr<Node_>(new NodeTrue_);
            else if (node.alwaysFalse_)
                *current_ = std::unique_ptr<Node_>(new NodeFalse_);
            else
                VisitArgsSetCurrent(node);
        }

        void Visit(NodeEqual_& node) { VisitBool(node); }
        void Visit(NodeSup_& node) { VisitBool(node); }
        void Visit(NodeSupEqual_& node) { VisitBool(node); }
        void Visit(NodeNot_& node) { VisitBool(node); }
        void Visit(NodeAnd_& node) { VisitBool(node); }
        void Visit(NodeOr_& node) { VisitBool(node); }

        // If
        void Visit(NodeIf_& node) {
            if (node.alwaysTrue_) {
                size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

                Vector_<ExprTree_> args = std::move(node.arguments_);
                *current_ = std::unique_ptr<Node_>(new NodeCollect_);

                for (size_t i = 1; i <= lastTrueStat; ++i)
                    (*current_)->arguments_.push_back(std::move(args[i]));
                VisitArgsSetCurrent(**current_);
            }
            else if (node.alwaysFalse_) {
                int firstElseStatement = node.firstElse_;

                Vector_<ExprTree_> args = std::move(node.arguments_);
                *current_ = std::unique_ptr<Node_>(new NodeCollect_);

                if (firstElseStatement != -1)
                    for (size_t i = firstElseStatement; i < args.size(); ++i)
                        (*current_)->arguments_.push_back(std::move(args[i]));
                VisitArgsSetCurrent(**current_);
            }
            else
                VisitArgsSetCurrent(node);
        }
    };
} // namespace Dal::Script
