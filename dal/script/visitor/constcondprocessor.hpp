//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    // ConstCond processor
    // Processes all IsConstant (always true/false) conditions and conditional statements
    // Remove all the if and condition nodes that are always true or always false
    // The domain proc must have been run first, so always true/false flags are properly set inside the nodes
    // The always true/false if nodes are replaced by collections of statements to be evaluated
    // The always true/false conditions are replaced by true/false nodes

    class ConstCondProcessor_ : public Visitor_<ConstCondProcessor_> {
        // The (unique) pointer on the node currently being visited
        ExprTree_* current_;

        // Visit arguments_ plus set current_ pointer
        void VisitArgsSetCurrent(Node_& node) {
            for (auto& arg : node.arguments_) {
                current_ = &arg;
                arg->Accept(*this);
            }
        }

    public:
        // Overload catch-all-nodes visitor to Visit arguments_ plus set current_
        template <class N_>
        std::enable_if_t<std::is_same<N_, std::remove_const_t<N_>>::value &&
                    !HasConstVisit_<ConstCondProcessor_>::ForNodeType<N_>()>
        Visit(N_& node) {
            VisitArgsSetCurrent(node);
        }

        // This particular visitor modifies the structure of the tree, hence it must be called only
        // with this method from the Top of every tree, passing a ref on the unique_ptr holding
        // the Top node of the tree
        void ProcessFromTop(std::unique_ptr<Node_>& top) {
            current_ = &top;
            top->Accept(*this);
        }

        // Conditions

        // One visitor for all booleans
        void VisitBool(BoolNode_& node) {
            // Always true ==> replace the tree by a True node
            if (node.alwaysTrue_)
                current_->reset(new NodeTrue_);

            // Always false ==> replace the tree by a False node
            else if (node.alwaysFalse_)
                current_->reset(new NodeFalse_);

            // Nothing to do here ==> Visit the arguments_
            else
                VisitArgsSetCurrent(node);
        }

        // Visitors
        void Visit(NodeEqual_& node) { VisitBool(node); }
        void Visit(NodeSup_& node) { VisitBool(node); }
        void Visit(NodeSupEqual_& node) { VisitBool(node); }
        void Visit(NodeNot_& node) { VisitBool(node); }
        void Visit(NodeAnd_& node) { VisitBool(node); }
        void Visit(NodeOr_& node) { VisitBool(node); }

        // If
        void Visit(NodeIf_& node) {
            // Always true ==> replace the tree by the collection of "if true" statements
            if (node.alwaysTrue_) {
                size_t lastTrueStat = node.firstElse_ == -1 ? node.arguments_.size() - 1 : node.firstElse_ - 1;

                // Move arguments_, destroy node
                Vector_<ExprTree_> args = std::move(node.arguments_);
                current_->reset(new NodeCollect_);

                for (size_t i = 1; i <= lastTrueStat; ++i) {
                    (*current_)->arguments_.push_back(std::move(args[i]));
                }
                VisitArgsSetCurrent(**current_);
            }

            // Always false ==> replace the tree by the collection of "else" statements
            else if (node.alwaysFalse_) {
                int firstElseStatement = node.firstElse_;

                // Move arguments_, destroy node
                Vector_<ExprTree_> args = std::move(node.arguments_);
                current_->reset(new NodeCollect_);

                if (firstElseStatement != -1) {
                    for (size_t i = firstElseStatement; i < args.size(); ++i) {
                        (*current_)->arguments_.push_back(std::move(args[i]));
                    }
                }
                VisitArgsSetCurrent(**current_);
            }
            // Nothing to do here ==> Visit the arguments_
            else
                VisitArgsSetCurrent(node);
        }
    };
} // namespace Dal::Script
