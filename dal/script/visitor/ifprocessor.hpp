//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <set>
#include <iterator>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor.hpp>
#include <dal/math/stacks.hpp>

namespace Dal::Script {
    class IFProcessor_ : public Visitor_ {
        Stack_<std::set<size_t>> varStack_;
        // Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t nestedIFLv_;
        size_t maxNestedIFs_;

    public:
        IFProcessor_(): nestedIFLv_(0), maxNestedIFs_(0) {}
        size_t MaxNestedIFs() const {
            return maxNestedIFs_;
        }
        void Visit(NodeIf_* node) override;
        void Visit(NodeAssign_* node) override;
        void Visit(NodePays_* node) override;
        void Visit(NodeVar_* node) override;
    };
}
