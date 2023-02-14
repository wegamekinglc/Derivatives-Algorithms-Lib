//
// Created by wegam on 2022/11/11.
//

#pragma once

#include <dal/script/nodebase.hpp>
#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/visitorlist.hpp>

namespace Dal::Script {

    template <class V_> struct Visitor_ {
        //  Visit a node with concrete visitor
        //      use this to hide the statc_cast
        void VisitNode(Node_& node) {
            //  static_cast : Visit as concrete visitor
            node.Accept(static_cast<V_&>(*this));
        }

        //  Visit all the arguments_ with concrete (type V_) visitor
        void VisitArguments(Node_& node) {
            for (auto& arg : node.arguments_) {
                //  static_cast : Visit as concrete visitor
                arg->Accept(static_cast<V_&>(*this));
            }
        }

        //  Default catch all = Visit arguments_
        void Visit(Node_& node) {
            //  V_ does not declare a Visit to that node type,
            //      either const or non const - fall back to default Visit arguments_
            VisitArguments(node);
        }
    };

    //  Const visitor

    template <class V_> struct ConstVisitor_ {
        void VisitNode(const Node_& node) {
            //  static_cast : Visit as concrete visitor
            node.Accept(static_cast<V_&>(*this));
        }

        void VisitArguments(const Node_& node) {
            for (const auto& arg : node.arguments_) {
                //  static_cast : Visit as visitor of type V_
                arg->Accept(static_cast<V_&>(*this));
            }
        }

        template <class N_> void Visit(const N_& node) {
            //  Const visitors cannot declare non const visits: we check that and produce a compilation error
            // static_assert(!HasNonConstVisit_<V_>::ForNodeType<NODE>(), "CONST VISITOR DECLARES A NON-CONST VISIT");

            //  V_ does not declare a Visit to that node type,
            //      either const or non const - fall back to visiting arguments_
            VisitArguments(node);
        }
    };

    //  Visitable_ classes

    //  Base Node_ must inherit visitableBase
    //      so it (automatically) declares pure virtual Accept methods for all visitors on the list

    //  Concrete Nodes must inherit Visitable_
    //      so they (automatically) declare overrides accepts for all visitors on the list

    /*
    Note that despite the complexity of that meta code, its use is trivial:

    To declare struct Node_ : VisitableBase_<Visitor1, Visitor2, ...> {};

        is only sugar for :

        struct Node_
        {
            virtual void Accept(Visitor1&) = 0;
            virtual void Accept(Visitor2&) = 0;
            ...
        };

    And to declare a concrete node (say, NodeAdd_) as NodeAdd_ : Visitable_<Node_, AddNode, Visitor1, Visitor2, ...> {};

        is sugar for:

        struct NodeAdd_ : Node_
        {
            void Accept(Visitor1& v) override
            {
                v.Visit(*this);
            }

            void Accept(Visitor2& v) override
            {
                v.Visit(*this);
            }

            ...
        };

    */
}
