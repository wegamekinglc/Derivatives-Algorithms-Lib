//
// Created by wegam on 2022/7/10.
//

#include <dal/script/experimental/visitor/ifprocessor.hpp>

namespace Dal::Script::Experimental {
    void IFProcessor_::operator()(std::unique_ptr<NodeIf_> &node) {
        ++nestedIFLv_;
        if (nestedIFLv_ > maxNestedIFs_)
            maxNestedIFs_ = nestedIFLv_;

        varStack_.Push(std::set<size_t>());
        for (size_t i = 1; i < node->arguments_.size(); ++i)
            Visit(node->arguments_[i]);

        node->affectedVars_.clear();
        std::copy(varStack_.Top().begin(),
                  varStack_.Top().end(),
                  std::back_inserter(node->affectedVars_));

        varStack_.Pop();
        --nestedIFLv_;
        if (nestedIFLv_ > 0)
            std::copy(node->affectedVars_.begin(),
                      node->affectedVars_.end(),
                      std::inserter(varStack_.Top(), varStack_.Top().end()));
    }

    void IFProcessor_::operator()(std::unique_ptr<NodeAssign_> &node) {
        if (nestedIFLv_ > 0)
            Visit(node->arguments_[0]);
    }

    void IFProcessor_::operator()(std::unique_ptr<NodePays_> &node) {
        if (nestedIFLv_ > 0)
            Visit(node->arguments_[0]);
    }

    void IFProcessor_::operator()(std::unique_ptr<NodeVar_> &node) {
        if (nestedIFLv_ > 0)
            varStack_.Top().insert(node->index_);
    }
}