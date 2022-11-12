//
// Created by wegam on 2022/5/21.
//

#include <dal/script/experimental/visitor/varindexer.hpp>

namespace Dal::Script::Experimental {

    Vector_<String_> VarIndexer_::GetVarNames() const {
        Vector_<String_> v(varMap_.size());
        for(const auto& var: varMap_)
            v[var.second] = var.first;
        return v;
    }
}