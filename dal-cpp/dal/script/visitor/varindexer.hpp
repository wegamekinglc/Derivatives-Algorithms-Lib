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
        struct VectorInfo_ {
            size_t index_;
            size_t largestEntry_ = 0;
            size_t appends_ = 0;
        };
        std::map<String_, VectorInfo_> vectorMap_;

        size_t VectorIndex(const String_& name, const SourceLocation_& source) {
            REQUIRE2(!varMap_.count(name) && !constVarMap_.count(name), "VectorNameConflict: vector name also names a scalar; " + source.Describe(),
                     ScriptError_);
            auto found = vectorMap_.find(name);
            if (found == vectorMap_.end())
                found = vectorMap_.emplace(name, VectorInfo_{vectorMap_.size()}).first;
            return found->second.index_;
        }

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

        [[nodiscard]] Vector_<String_> VectorNames() const {
            Vector_<String_> names(vectorMap_.size());
            for (const auto& [name, info] : vectorMap_)
                names[info.index_] = name;
            return names;
        }

        [[nodiscard]] Vector_<size_t> VectorCapacities() const {
            Vector_<size_t> capacities(vectorMap_.size());
            for (const auto& [_, info] : vectorMap_)
                capacities[info.index_] = info.largestEntry_ + info.appends_;
            return capacities;
        }

        // Variable indexer: build map of names to indices and write indices on variable nodes
        void Visit(NodeVar_& node) {
            REQUIRE2(!vectorMap_.count(node.name_), "VectorNameConflict: scalar name also names a vector", ScriptError_);
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
            REQUIRE2(!vectorMap_.count(node.name_), "VectorNameConflict: scalar name also names a vector", ScriptError_);
            auto varIt = constVarMap_.find(node.name_);
            if (varIt == constVarMap_.end()) {
                constVarMap_[node.name_] = std::make_tuple(constVarMap_.size(), node.constVal_);
                node.index_ = static_cast<int>(std::get<0>(constVarMap_[node.name_]));
            }
            else
                node.index_ = static_cast<int>(std::get<0>(varIt->second));
        }

        void Visit(NodeVectorEntry_& node) {
            node.index_ = static_cast<int>(VectorIndex(node.name_, node.source_));
            auto& info = vectorMap_.at(node.name_);
            info.largestEntry_ = std::max(info.largestEntry_, node.entry_ + 1);
        }

        void Visit(NodeVectorReduce_& node) { node.index_ = static_cast<int>(VectorIndex(node.name_, node.source_)); }

        void Visit(NodeVectorAppend_& node) {
            node.index_ = static_cast<int>(VectorIndex(node.name_, node.source_));
            ++vectorMap_.at(node.name_).appends_;
            VisitArguments(node);
        }
    };
} // namespace Dal::Script
