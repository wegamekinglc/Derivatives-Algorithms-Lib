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
    class IFProcessor_ : public Visitor_<IFProcessor_> {
        Stack_<std::set<size_t>> varStack_;
        // Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t nestedIFLv_;
        size_t maxNestedIFs_;

    public:
        IFProcessor_(): nestedIFLv_(0), maxNestedIFs_(0) {}
        [[nodiscard]] size_t MaxNestedIFs() const {
            return maxNestedIFs_;
        }

        using Visitor_<IFProcessor_>::operator();
        using Visitor_<IFProcessor_>::Visit;

        void operator()(std::unique_ptr<NodeIf_> &node);
        void operator()(std::unique_ptr<NodeAssign_> &node);
        void operator()(std::unique_ptr<NodePays_> &node);
        void operator()(std::unique_ptr<NodeVar_> &node);
    };
}
