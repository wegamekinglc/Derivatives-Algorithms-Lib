//
// Created by wegam on 2022/5/21.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {

    class VarIndexer_ : public Visitor_ {
        std::map<String_, int> varMap_;

    public:
        Vector_<String_> GetVarNames() const;
        void Visit(NodeVar_* node) override;
    };

}
