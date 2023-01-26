//
// Created by wegam on 2022/7/10.
//

#pragma once

#include <set>
#include <iterator>
#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/math/stacks.hpp>

namespace Dal::Script {
    class IFProcessor_ : public Visitor<IFProcessor_>
    {
        //	Top of the stack: current (possibly nested) if being processed
        //	Each element in stack: set of indices of variables modified by the corresponding if and nested ifs
       StaticStack_<std::set<size_t>>    myVarStack;

        //	Nested if level, 0: not in an if, 1: in the outermost if, 2: if nested in another if, etc.
        size_t					    myNestedIfLvl;

        //	Keep track of the maximum number of nested ifs
        size_t					    myMaxNestedIfs;

    public:

        using Visitor<IFProcessor_>::Visit;

        IFProcessor_() : myNestedIfLvl( 0), myMaxNestedIfs( 0) {}

        //	Access to the max nested ifs after the prcessor is run
        const size_t maxNestedIfs() const
        {
            return myMaxNestedIfs;
        }

        //	Visitors

        void Visit( NodeIf& node)
        {
            //	Increase nested if level
            ++myNestedIfLvl;
            if( myNestedIfLvl > myMaxNestedIfs) myMaxNestedIfs = myNestedIfLvl;

            //	Put new element on the stack
            myVarStack.push(std::set<size_t>());

            //	Visit arguments, excluding condition
            for(size_t i = 1; i < node.arguments.size(); ++i) node.arguments[i]->accept( *this);

            //	Copy the top of the stack into the node
            node.affectedVars.clear();
            copy( myVarStack.top().begin(), myVarStack.top().end(), back_inserter( node.affectedVars));

            //	Pop
            myVarStack.pop();

            //	Decrease nested if level
            --myNestedIfLvl;

            //	If not outmost if, copy changed vars into the immediately outer if
            //	Variables changed in a nested if are also changed in the englobing if
            if( myNestedIfLvl) copy( node.affectedVars.begin(), node.affectedVars.end(), inserter( myVarStack.top(), myVarStack.top().end()));
        }

        void Visit( NodeAssign& node)
        {
            //	Visit the lhs var
            if( myNestedIfLvl) node.arguments[0]->accept( *this);
        }

        void Visit( NodePays& node)
        {
            //	Visit the lhs var
            if( myNestedIfLvl) node.arguments[0]->accept( *this);
        }

        void Visit( NodeVar& node)
        {
            //	Insert the var idx
            if( myNestedIfLvl) myVarStack.top().insert( node.index);
        }
    };
}
