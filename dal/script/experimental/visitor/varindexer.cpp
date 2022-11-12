//
// Created by wegam on 2022/5/21.
//

#include <dal/script/visitor/varindexer.hpp>

namespace Dal::Script {

    Vector_<String_> VarIndexer_::GetVarNames() const {
        Vector_<String_> v(varMap_.size());
        for(const auto& var: varMap_)
            v[var.second] = var.first;
        return v;
    }
}