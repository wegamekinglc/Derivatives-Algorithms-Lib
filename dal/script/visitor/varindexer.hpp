//
// Created by wegam on 2022/5/21.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {

    class VarIndexer_ : public Visitor_<VarIndexer_> {
        std::map<String_, int> varMap_;

    public:
        [[nodiscard]] Vector_<String_> GetVarNames() const;

        using Visitor_<VarIndexer_>::operator();
        using Visitor_<VarIndexer_>::Visit;
        void operator()(std::unique_ptr<NodeVar_> &node);
    };

}
