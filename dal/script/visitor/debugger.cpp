//
// Created by wegam on 2022/2/17.
//

#include <dal/script/visitor/debugger.hpp>

namespace Dal::Script {
    void Debugger_::Debug(const ScriptNode_& node, const String_& nodeId) {
        prefix_ += " ";
        bool isEmpty = node.arguments_.empty();
        if (!isEmpty)
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                (*it)->AcceptVisitor(this);

        prefix_.pop_back();

        String_ str(prefix_ + nodeId);
        if (!isEmpty) {
            str += "(\n";
            str += stack_.Top();
            stack_.Pop();
            if (node.arguments_.size() > 1)
                str += prefix_ + ",\n";
            for (size_t i = 1; i < node.arguments_.size() - 1; ++i) {
                str += stack_.Top() + prefix_ + ",\n";
                stack_.Pop();
            }
            if (node.arguments_.size() > 1) {
                str += stack_.Top();
                stack_.Pop();
            }

            str += prefix_ + ')';
        }
        str += '\n';
        stack_.Push(std::move(str));
    }

    String_ Debugger_::String() const { return stack_.Top(); }

    void Debugger_::Visit(const NodeCollect_* node) { Debug(*node, "COLLECT"); }

    void Debugger_::Visit(const NodeTrue_* node) { Debug(*node, "TRUE"); }

    void Debugger_::Visit(const NodeFalse_* node) { Debug(*node, "FALSE"); }

    void Debugger_::Visit(const NodeUPlus_* node) { Debug(*node, "U-PLUS"); }

    void Debugger_::Visit(const NodeUMinus_* node) { Debug(*node, "U-Minus"); }

    void Debugger_::Visit(const NodePlus_* node) { Debug(*node, "PLUS"); }

    void Debugger_::Visit(const NodeMinus_* node) { Debug(*node, "MINUS"); }

    void Debugger_::Visit(const NodeMultiply_* node) { Debug(*node, "MULTIPLY"); }

    void Debugger_::Visit(const NodeDivide_* node) { Debug(*node, "DIV"); }

    void Debugger_::Visit(const NodePower_* node) { Debug(*node, "POW"); }

    void Debugger_::Visit(const NodeLog_* node) { Debug(*node, "LOG"); }

    void Debugger_::Visit(const NodeSqrt_* node) { Debug(*node, "SQRT"); }

    void Debugger_::Visit(const NodeMax_* node) { Debug(*node, "MAX"); }

    void Debugger_::Visit(const NodeMin_* node) { Debug(*node, "MIN"); }

    void Debugger_::Visit(const NodeConst_* node) {
        Debug(*node, String_("CONST[") + String::FromDouble(node->val_) + ']');
    }

    void Debugger_::Visit(const NodeVar_* node) { Debug(*node, String_("VAR[") + String::FromInt(node->index_) + ']'); }

    void Debugger_::Visit(const NodeAssign_* node) { Debug(*node, "ASSIGN"); }

    void Debugger_::Visit(const NodeIf_* node) {
        String_ s = "IF";
        s += "[FIRST ELSE = " + String::FromInt(node->firstElse_) + "]";
        Debug(*node, s);
    }

    void Debugger_::Visit(const NodeEqual_* node) {
        String_ s = "EQUALZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(*node, s);
    }

    void Debugger_::Visit(const NodeNot_* node) {
        String_ s = "NOT";
        return Debug(*node, s);
    }

    void Debugger_::Visit(const NodeSuperior_* node) {
        String_ s = "GTZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(*node, s);
    }

    void Debugger_::Visit(const NodeSupEqual_* node) {
        String_ s = "GTEQUALZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(*node, s);
    }

    void Debugger_::Visit(const NodeAnd_* node) {
        String_ s = "AND";
        return Debug(*node, s);
    }

    void Debugger_::Visit(const NodeOr_* node) {
        String_ s = "OR";
        return Debug(*node, s);
    }

    void Debugger_::Visit(const NodePays_* node) { Debug(*node, "PAYS"); }

    void Debugger_::Visit(const NodeSpot_* node) { Debug(*node, "SPOT"); }

    void Debugger_::Visit(const NodeSmooth_* node) { Debug(*node, "SMOOTH"); }
} // namespace Dal::Script