//
// Created by wegam on 2022/5/21.
//

#include <dal/script/experimental/visitor/varindexer.hpp>
#include <dal/script/experimental/node.hpp>

namespace Dal::Script::Experimental {

    Vector_<String_> VarIndexer_::GetVarNames() const {
        Vector_<String_> v(varMap_.size());
        for(const auto& var: varMap_)
            v[var.second] = var.first;
        return v;
    }

    void VarIndexer_::operator()(std::unique_ptr<NodeVar_> &node) {
        auto it = varMap_.find(node->name_);
        if (it == varMap_.end())
            node->index_ = varMap_[node->name_] = varMap_.size();
        else
            node->index_ = it->second;
    }
}