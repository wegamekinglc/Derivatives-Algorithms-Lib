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
        String_ prefix_;
        StaticStack_<String_> stack_;

        //	The main function call from every node visitor
        void Debug(const Node_& node, const String_& nodeId) {
            //	One more tab
            prefix_ += '\t';

            //	Visit arguments_, right to left
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                (*it)->Accept(*this);

            //	One less tab
            prefix_.pop_back();

            String_ str(prefix_ + nodeId);
            if (!node.arguments_.empty()) {
                str += "(\n";

                //	First argument, pushed last
                str += stack_.Top();
                stack_.Pop();
                if (node.arguments_.size() > 1)
                    str += prefix_ + ",\n";

                //	Args 2 to n-1
                for (size_t i = 1; i < node.arguments_.size() - 1; ++i) {
                    str += stack_.Top() + prefix_ + ",\n";
                    stack_.Pop();
                }

                if (node.arguments_.size() > 1) {
                    //	Last argument, pushed first
                    str += stack_.Top();
                    stack_.Pop();
                }

                //	Close ')'
                str += prefix_ + ')';
            }

            str += '\n';
            stack_.Push(std::move(str));
        }

    public:
        using ConstVisitor_<Debugger_>::Visit;

        //	Access the Top of the stack, contains the functional form after the tree is traversed
        const String_& String() const { return stack_.Top(); }

        //	All concrete node visitors, Visit arguments_ by default unless overridden

        void Visit(const NodeCollect_& node) { Debug(node, "COLLECT"); }

        void Visit(const NodeUplus_& node) { Debug(node, "UPLUS"); }
        void Visit(const NodeUminus_& node) { Debug(node, "UMINUS"); }
        void Visit(const NodeAdd_& node) { Debug(node, "ADD"); }
        void Visit(const NodeSub_& node) { Debug(node, "SUBTRACT"); }
        void Visit(const NodeMult_& node) { Debug(node, "MULT"); }
        void Visit(const NodeDiv_& node) { Debug(node, "DIV"); }
        void Visit(const NodePow_& node) { Debug(node, "POW"); }
        void Visit(const NodeLog_& node) { Debug(node, "LOG"); }
        void Visit(const NodeSqrt_& node) { Debug(node, "SQRT"); }
        void Visit(const NodeMax_& node) { Debug(node, "MAX"); }
        void Visit(const NodeMin_& node) { Debug(node, "MIN"); }
        void Visit(const NodeSmooth_& node) { Debug(node, "SMOOTH"); }

        void Visit(const NodeEqual_& node) {
            String_ s = "EQUALZERO";

            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += String_("[DISCRETE,");
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeNot_& node) {
            String_ s = "NOT";
            Debug(node, s);
        }

        void Visit(const NodeSup_& node) {
            String_ s = "GTZERO";
            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeSupEqual_& node) {
            String_ s = "GTEQUALZERO";
            if (!node.isDiscrete_) {
                s += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
            } else {
                s += "[DISCRETE,";
                s += String_("BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
            }
            Debug(node, s);
        }

        void Visit(const NodeAnd_& node) {
            String_ s = "AND";
            Debug(node, s);
        }

        void Visit(const NodeOr_& node) {
            String_ s = "OR";
            Debug(node, s);
        }

        void Visit(const NodeAssign_& node) { Debug(node, "ASSIGN"); }
        void Visit(const NodePays_& node) { Debug(node, "PAYS"); }
        void Visit(const NodeSpot_& node) { Debug(node, "SPOT"); }

        void Visit(const NodeIf_& node) {
            String_ s = "IF";
            s += String_("[FIRSTELSE=" + std::to_string(node.firstElse_) + "]");

            Debug(node, s);
        }

        void Visit(const NodeTrue_& node) { Debug(node, "TRUE"); }
        void Visit(const NodeFalse_& node) { Debug(node, "FALSE"); }

        void Visit(const NodeConst_& node) {
            Debug(node, String_("CONST[") + String_(std::to_string(node.constVal_) + ']')); }
        void Visit(const NodeVar_& node) {
            Debug(node, String_("VAR[") + node.name_ + String_(',' + std::to_string(node.index_) + ']')); }
    };
}