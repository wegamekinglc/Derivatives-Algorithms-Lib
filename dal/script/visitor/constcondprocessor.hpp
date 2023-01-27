//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {

    //	ConstCond processor
    //	Processes all constant (always true/false) conditions and conditional statements
    //	Remove all the if and condition nodes that are always true or always false
    //	The domain proc must have been run first, so always true/false flags are properly set inside the nodes
    //	The always true/false if nodes are replaced by collections of statements to be evaluated
    //	The always true/false conditions are replaced by true/false nodes

    class ConstCondProcessor_ : public Visitor_<ConstCondProcessor_> {
        //	The (unique) pointer on the node currently being visited
        ExprTree_* myCurrent;

        //  Visit arguments plus set myCurrent pointer
        void visitArgsSetCurrent(Node_& node) {
            for (auto& arg : node.arguments) {
                myCurrent = &arg;
                arg->Accept(*this);
            }
        }

    public:
        //	Overload catch-all-nodes visitor to Visit arguments plus set myCurrent
        template <class NODE>
        std::enable_if_t<std::is_same<NODE, std::remove_const_t<NODE>>::value &&
                    !HasConstVisit_<ConstCondProcessor_>::ForNodeType<NODE>()>
        Visit(NODE& node) {
            visitArgsSetCurrent(node);
        }

        //	This patricular visitor modifies the structure of the tree, hence it must be called only
        //		with this method from the top of every tree, passing a ref on the unique_ptr holding
        //		the top node of the tree
        void processFromTop(std::unique_ptr<Node_>& top) {
            myCurrent = &top;
            top->Accept(*this);
        }

        //	Conditions

        //	One visitor for all booleans
        void visitBool(BoolNode_& node) {
            //	Always true ==> replace the tree by a True node
            if (node.alwaysTrue)
                myCurrent->reset(new NodeTrue);

            //	Always false ==> replace the tree by a False node
            else if (node.alwaysFalse)
                myCurrent->reset(new NodeFalse);

            //	Nothing to do here ==> Visit the arguments
            else
                visitArgsSetCurrent(node);
        }

        //	Visitors
        void Visit(NodeEqual& node) { visitBool(node); }
        void Visit(NodeSup& node) { visitBool(node); }
        void Visit(NodeSupEqual& node) { visitBool(node); }
        void Visit(NodeNot& node) { visitBool(node); }
        void Visit(NodeAnd& node) { visitBool(node); }
        void Visit(NodeOr& node) { visitBool(node); }

        //	If
        void Visit(NodeIf& node) {
            //	Always true ==> replace the tree by the collection of "if true" statements
            if (node.alwaysTrue) {
                size_t lastTrueStat = node.firstElse == -1 ? node.arguments.size() - 1 : node.firstElse - 1;

                //	Move arguments, destroy node
                Vector_<ExprTree_> args = std::move(node.arguments);
                myCurrent->reset(new NodeCollect);

                for (size_t i = 1; i <= lastTrueStat; ++i) {
                    (*myCurrent)->arguments.push_back(std::move(args[i]));
                }

                visitArgsSetCurrent(**myCurrent);
            }

            //	Always false ==> replace the tree by the collection of "else" statements
            else if (node.alwaysFalse) {
                int firstElseStatement = node.firstElse;

                //	Move arguments, destroy node
                Vector_<ExprTree_> args = std::move(node.arguments);
                myCurrent->reset(new NodeCollect);

                if (firstElseStatement != -1) {
                    for (size_t i = firstElseStatement; i < args.size(); ++i) {
                        (*myCurrent)->arguments.push_back(std::move(args[i]));
                    }
                }

                visitArgsSetCurrent(**myCurrent);
            }

            //	Nothing to do here ==> Visit the arguments
            else
                visitArgsSetCurrent(node);
        }
    };
} // namespace Dal::Script
