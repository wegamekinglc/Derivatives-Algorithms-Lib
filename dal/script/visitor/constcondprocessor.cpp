//
// Created by wegam on 2022/7/23.
//

#include <dal/platform/strict.hpp>
#include <dal/script/visitor/constcondprocessor.hpp>

namespace Dal::Script {

    void ConstCondProcessor_::ProcessFromTop(ScriptNode_ &top) {
        current_ = &top;
        Visit(top);
    }

    void ConstCondProcessor_::operator()(std::unique_ptr<NodeIf_>& node) {
        if (node->alwaysTrue_) {
            size_t lastTrueStat = node->firstElse_ == -1 ? node->arguments_.size() - 1 : node->firstElse_ - 1;
            Vector_<ScriptNode_> args = std::move(node->arguments_);
            *current_ = std::make_unique<NodeCollect_>();

            auto& ptr = std::get<std::unique_ptr<NodeCollect_>>(*current_);
            for (size_t i = 1; i <= lastTrueStat; ++i) {
                ptr->arguments_.push_back(std::move(args[i]));
            }
            VisitArguments(*ptr);
        } else if (node->alwaysFalse_) {
            int firstElseStatement = node->firstElse_;
            Vector_<ScriptNode_> args = std::move(node->arguments_);
            *current_ = std::make_unique<NodeCollect_>();

            auto& ptr = std::get<std::unique_ptr<NodeCollect_>>(*current_);
            if (firstElseStatement != -1) {
                for (size_t i = firstElseStatement; i < args.size(); ++i) {
                    ptr->arguments_.push_back(std::move(args[i]));
                }
            }
            VisitArguments(*ptr);
        } else
            VisitArguments(*node);
    }
}