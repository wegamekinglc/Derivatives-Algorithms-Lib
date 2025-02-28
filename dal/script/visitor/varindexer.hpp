//
// Created by wegam on 2022/5/21.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    class VarIndexer_ : public Visitor_<VarIndexer_> {
        // State
        std::map<String_, size_t> varMap_;
        std::map<String_, std::tuple<size_t, double>> constVarMap_;

    public:
        using Visitor_<VarIndexer_>::Visit;

        // Access vector of variable names v[index]=name after Visit to all events
        [[nodiscard]] Vector_<String_> VarNames() const {
            Vector_<String_> v(varMap_.size());
            for(const auto& [k, val]: varMap_)
                v[val] = k;

            // C++11: move not copy
            return v;
        }

        [[nodiscard]] Vector_<String_> ConstVarNames() const {
            Vector_<String_> v(constVarMap_.size());
            for(const auto& [k, val]: constVarMap_)
                v[std::get<0>(val)] = k;

            // C++11: move not copy
            return v;
        }

        [[nodiscard]] Vector_<double> ConstVarValues() const {
            Vector_<double> v(constVarMap_.size());
            for(const auto& [_, val]: constVarMap_)
                v[std::get<0>(val)] = std::get<1>(val);

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

        // Variable indexer: build map of names to indices and write indices on variable nodes
        void Visit(NodeConstVar_& node) {
            auto varIt = constVarMap_.find(node.name_);
            if (varIt == constVarMap_.end()) {
                constVarMap_[node.name_] = std::make_tuple(constVarMap_.size(), node.constVal_);
                node.index_ = static_cast<int>(std::get<0>(constVarMap_[node.name_]));
            }
            else
                node.index_ = static_cast<int>(std::get<0>(varIt->second));
        }
    };
} // namespace Dal::Script
