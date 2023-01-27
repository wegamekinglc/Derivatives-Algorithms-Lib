//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>


namespace Dal::Script {
    class Debugger_ : public ConstVisitor_<Debugger_> {
        String_ myPrefix;
        StaticStack_<String_> myStack;

        //	The main function call from every node visitor
        void Debug(const Node_& node, const String_& nodeId) {
            //	One more tab
            myPrefix += '\t';

            //	Visit arguments, right to left
            for (auto it = node.arguments.rbegin(); it != node.arguments.rend(); ++it)
                (*it)->Accept(*this);

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
        using ConstVisitor_<Debugger_>::Visit;

        //	Access the top of the stack, contains the functional form after the tree is traversed
        const String_& String() const { return myStack.top(); }

        //	All concrete node visitors, Visit arguments by default unless overridden

        void Visit(const NodeCollect& node) { Debug(node, "COLLECT"); }

        void Visit(const NodeUplus& node) { Debug(node, "UPLUS"); }
        void Visit(const NodeUminus& node) { Debug(node, "UMINUS"); }
        void Visit(const NodeAdd& node) { Debug(node, "ADD"); }
        void Visit(const NodeSub& node) { Debug(node, "SUBTRACT"); }
        void Visit(const NodeMult& node) { Debug(node, "MULT"); }
        void Visit(const NodeDiv& node) { Debug(node, "DIV"); }
        void Visit(const NodePow& node) { Debug(node, "POW"); }
        void Visit(const NodeLog& node) { Debug(node, "LOG"); }
        void Visit(const NodeSqrt& node) { Debug(node, "SQRT"); }
        void Visit(const NodeMax& node) { Debug(node, "MAX"); }
        void Visit(const NodeMin& node) { Debug(node, "MIN"); }
        void Visit(const NodeSmooth& node) { Debug(node, "SMOOTH"); }

        void Visit(const NodeEqual& node) {
            String_ s = "EQUALZERO";

            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += String_("[DISCRETE,");
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeNot& node) {
            String_ s = "NOT";
            Debug(node, s);
        }

        void Visit(const NodeSup& node) {
            String_ s = "GTZERO";
            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeSupEqual& node) {
            String_ s = "GTEQUALZERO";
            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeAnd& node) {
            String_ s = "AND";
            Debug(node, s);
        }

        void Visit(const NodeOr& node) {
            String_ s = "OR";
            Debug(node, s);
        }

        void Visit(const NodeAssign& node) { Debug(node, "ASSIGN"); }
        void Visit(const NodePays& node) { Debug(node, "PAYS"); }
        void Visit(const NodeSpot& node) { Debug(node, "SPOT"); }

        void Visit(const NodeIf& node) {
            String_ s = "IF";
            s += String_("[FIRSTELSE=" + std::to_string(node.firstElse_) + "]");

            Debug(node, s);
        }

        void Visit(const NodeTrue& node) { Debug(node, "TRUE"); }
        void Visit(const NodeFalse& node) { Debug(node, "FALSE"); }

        void Visit(const NodeConst& node) {
            Debug(node, String_("CONST[") + String_(std::to_string(node.constVal_) + ']')); }
        void Visit(const NodeVar& node) {
            Debug(node, String_("VAR[") + node.name_ + String_(',' + std::to_string(node.index_) + ']')); }
    };
}