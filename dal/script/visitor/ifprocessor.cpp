//
// Created by wegam on 2022/7/10.
//

#include <dal/script/visitor/ifprocessor.hpp>

namespace Dal::Script {
    void IFProcessor_::Visit(NodeIf_* node) {
        ++nestedIFLv_;
        if (nestedIFLv_ > maxNestedIFs_)
            maxNestedIFs_ = nestedIFLv_;

        varStack_.Push(std::set<size_t>());
        for (size_t i = 1; i < node->arguments_.size(); ++i)
            node->arguments_[i]->AcceptVisitor(this);

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

    void IFProcessor_::Visit(NodeAssign_* node) {
        if (nestedIFLv_ > 0)
            node->arguments_[0]->AcceptVisitor(this);
    }

    void IFProcessor_::Visit(NodePays_* node) {
        if (nestedIFLv_ > 0)
            node->arguments_[0]->AcceptVisitor(this);
    }

    void IFProcessor_::Visit(NodeVar_* node) {
        if (nestedIFLv_ > 0)
            varStack_.Top().insert(node->index_);
    }
}