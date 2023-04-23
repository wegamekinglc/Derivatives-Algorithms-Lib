//
// Created by wegam on 2022/5/21.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <map>

namespace Dal::Script {

    class VarIndexer_ : public Visitor_<VarIndexer_> {
        // State
        std::map<String_, size_t> varMap_;

    public:
        using Visitor_<VarIndexer_>::Visit;

        // Access vector of variable names v[index]=name after Visit to all events
        [[nodiscard]] Vector_<String_> VarNames() const {
            Vector_<String_> v(varMap_.size());
            for(const auto& val: varMap_)
                v[val.second] = val.first;

            // C++11: move not copy
            return v;
        }

        // Variable indexer: build map of names to indices and write indices on variable nodes
        void Visit(NodeVar_& node) {
            auto varIt = varMap_.find(node.name_);
            if (varIt == varMap_.end()) {
                varMap_[node.name_] = varMap_.size();
                node.index_ = static_cast<int>(varMap_[node.name_]);
            }
            else
                node.index_ = static_cast<int>(varIt->second);
        }
    };
} // namespace Dal::Script
