//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {
    class Debugger_ : public constVisitor<Debugger_> {
        String_ myPrefix;
        StaticStack_<String_> myStack;

        //	The main function call from every node visitor
        void debug(const Node& node, const String_& nodeId) {
            //	One more tab
            myPrefix += '\t';

            //	Visit arguments, right to left
            for (auto it = node.arguments.rbegin(); it != node.arguments.rend(); ++it)
                (*it)->accept(*this);

            //	One less tab
            myPrefix.pop_back();

            String_ str(myPrefix + nodeId);
            if (!node.arguments.empty()) {
                str += "(\n";

                //	First argument, pushed last
                str += myStack.top();
                myStack.pop();
                if (node.arguments.size() > 1)
                    str += myPrefix + ",\n";

                //	Args 2 to n-1
                for (size_t i = 1; i < node.arguments.size() - 1; ++i) {
                    str += myStack.top() + myPrefix + ",\n";
                    myStack.pop();
                }

                if (node.arguments.size() > 1) {
                    //	Last argument, pushed first
                    str += myStack.top();
                    myStack.pop();
                }

                //	Close ')'
                str += myPrefix + ')';
            }

            str += '\n';
            myStack.push(std::move(str));
        }

    public:
        using constVisitor<Debugger_>::Visit;

        //	Access the top of the stack, contains the functional form after the tree is traversed
        const String_& String() const { return myStack.top(); }

        //	All concrete node visitors, Visit arguments by default unless overridden

        void Visit(const NodeCollect& node) { debug(node, "COLLECT"); }

        void Visit(const NodeUplus& node) { debug(node, "UPLUS"); }
        void Visit(const NodeUminus& node) { debug(node, "UMINUS"); }
        void Visit(const NodeAdd& node) { debug(node, "ADD"); }
        void Visit(const NodeSub& node) { debug(node, "SUBTRACT"); }
        void Visit(const NodeMult& node) { debug(node, "MULT"); }
        void Visit(const NodeDiv& node) { debug(node, "DIV"); }
        void Visit(const NodePow& node) { debug(node, "POW"); }
        void Visit(const NodeLog& node) { debug(node, "LOG"); }
        void Visit(const NodeSqrt& node) { debug(node, "SQRT"); }
        void Visit(const NodeMax& node) { debug(node, "MAX"); }
        void Visit(const NodeMin& node) { debug(node, "MIN"); }
        void Visit(const NodeSmooth& node) { debug(node, "SMOOTH"); }

        void Visit(const NodeEqual& node) {
            String_ s = "EQUALZERO";

            if (!node.discrete) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps) + "]");
            } else {
                s += String_("[DISCRETE,");
                s += String_("BOUNDS=" + std::to_string(node.lb) + "," + std::to_string(node.rb) + "]");
            }
            debug(node, s);
        }

        void Visit(const NodeNot& node) {
            String_ s = "NOT";
            debug(node, s);
        }

        void Visit(const NodeSup& node) {
            String_ s = "GTZERO";
            if (!node.discrete) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb) + "," + std::to_string(node.rb) + "]");
            }
            debug(node, s);
        }

        void Visit(const NodeSupEqual& node) {
            String_ s = "GTEQUALZERO";
            if (!node.discrete) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb) + "," + std::to_string(node.rb) + "]");
            }
            debug(node, s);
        }

        void Visit(const NodeAnd& node) {
            String_ s = "AND";
            debug(node, s);
        }

        void Visit(const NodeOr& node) {
            String_ s = "OR";
            debug(node, s);
        }

        void Visit(const NodeAssign& node) { debug(node, "ASSIGN"); }
        void Visit(const NodePays& node) { debug(node, "PAYS"); }
        void Visit(const NodeSpot& node) { debug(node, "SPOT"); }

        void Visit(const NodeIf& node) {
            String_ s = "IF";
            s += String_("[FIRSTELSE=" + std::to_string(node.firstElse) + "]");

            debug(node, s);
        }

        void Visit(const NodeTrue& node) { debug(node, "TRUE"); }
        void Visit(const NodeFalse& node) { debug(node, "FALSE"); }

        void Visit(const NodeConst& node) { debug(node, String_("CONST[") + String_(std::to_string(node.constVal) + ']')); }
        void Visit(const NodeVar& node) { debug(node, String_("VAR[") + node.name + String_(',' + std::to_string(node.index) + ']')); }
    };
}