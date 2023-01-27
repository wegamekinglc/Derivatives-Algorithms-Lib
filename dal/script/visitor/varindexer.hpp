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
        //	State
        std::map<String_, size_t> myVarMap;

    public:
        using Visitor_<VarIndexer_>::Visit;

        //	Access vector of variable names v[index]=name after Visit to all events
        Vector_<String_> VarNames() const {
            Vector_<String_> v(myVarMap.size());
            for (auto varMapIt = myVarMap.begin(); varMapIt != myVarMap.end(); ++varMapIt) {
                v[varMapIt->second] = varMapIt->first;
            }

            //	C++11: move not copy
            return v;
        }

        //	Variable indexer: build map of names to indices and write indices on variable nodes
        void Visit(NodeVar& node) {
            auto varIt = myVarMap.find(node.name);
            if (varIt == myVarMap.end())
                node.index = myVarMap[node.name] = myVarMap.size();
            else
                node.index = varIt->second;
        }
    };
} // namespace Dal::Script
