//
// Created by wegam on 2022/7/23.
//

#include <dal/platform/strict.hpp>
#include <dal/script/visitor/constcondprocessor.hpp>

namespace Dal::Script {
    void ConstCondProcessor_::VisitArguments(ScriptNode_ *node) {
        for (auto &arg: node->arguments_) {
            current_ = &arg;
            arg->AcceptVisitor(this);
        }
    }

    void ConstCondProcessor_::ProcessFromTop(std::unique_ptr<ScriptNode_> &top) {
        current_ = &top;
        top->AcceptVisitor(this);
    }

    void ConstCondProcessor_::Visit(NodeIf_ *node) {
        if (node->alwaysTrue_) {
            size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
            Vector_<std::unique_ptr<ScriptNode_>> args = std::move(node->arguments_);
            *current_ = std::make_unique<NodeCollect_>();

            for (size_t i = 1; i <= lastTrueStat; ++i) {
                (*current_)->arguments_.push_back(std::move(args[i]));
            }
            VisitArguments(current_->get());
        } else if (node->alwaysFalse_) {
            int firstElseStatement = node->firstElse_;
            Vector_<std::unique_ptr<ScriptNode_>> args = std::move(node->arguments_);
            *current_ = std::make_unique<NodeCollect_>();

            if (firstElseStatement != -1) {
                for (size_t i = firstElseStatement; i < args.size(); ++i) {
                    (*current_)->arguments_.push_back(std::move(args[i]));
                }
            }
            VisitArguments(current_->get());
        } else
            VisitArguments(node);
    }
}