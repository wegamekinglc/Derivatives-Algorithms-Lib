//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>
#include <iterator>
#include <set>

namespace Dal::Script {
    class IFProcessor_ : public Visitor_<IFProcessor_> {
        // Top of the stack: current (possibly nested) if being processed
        // Each element in stack: set of indices of variables modified by the corresponding if and nested ifs
        StaticStack_<std::set<size_t>> varStack_;

        // Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t nestedIfLvl_;

        // Keep track of the maximum number of nested ifs
        size_t maxNestedIfs_;

    public:
        using Visitor_<IFProcessor_>::Visit;

        IFProcessor_() : nestedIfLvl_(0), maxNestedIfs_(0) {}

        // Access to the max nested ifs after the processor is run
        [[nodiscard]] const size_t MaxNestedIFs() const { return maxNestedIfs_; }

        // Visitors
        void Visit(NodeIf_& node) {
            //	Increase nested if level
            ++nestedIfLvl_;
            if (nestedIfLvl_ > maxNestedIfs_)
                maxNestedIfs_ = nestedIfLvl_;

            //	Put new element on the stack
            varStack_.Push(std::set<size_t>());

            //	Visit arguments_, excluding condition
            for (size_t i = 1; i < node.arguments_.size(); ++i)
                node.arguments_[i]->Accept(*this);

            //	Copy the Top of the stack into the node
            node.affectedVars_.clear();
            copy(varStack_.Top().begin(), varStack_.Top().end(), back_inserter(node.affectedVars_));

            //	Pop
            varStack_.Pop();

            //	Decrease nested if level
            --nestedIfLvl_;

            //	If not out-most if, copy changed vars into the immediately outer if
            //	Variables changed in a nested if are also changed in the in-globing if
            if (nestedIfLvl_)
                copy(node.affectedVars_.begin(), node.affectedVars_.end(),
                     inserter(varStack_.Top(), varStack_.Top().end()));
        }

        void Visit(NodeAssign_& node) {
            //	Visit the lhs var
            if (nestedIfLvl_)
                node.arguments_[0]->Accept(*this);
        }

        void Visit(NodePays_& node) {
            //	Visit the lhs var
            if (nestedIfLvl_)
                node.arguments_[0]->Accept(*this);
        }

        void Visit(NodeVar_& node) {
            //	Insert the var idx
            if (nestedIfLvl_)
                varStack_.Top().insert(node.index_);
        }
    };
} // namespace Dal::Script
