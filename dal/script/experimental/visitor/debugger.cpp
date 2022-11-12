//
// Created by wegam on 2022/2/17.
//

#if 0
#include <dal/platform/strict.hpp>
#include <dal/script/experimental/visitor/debugger.hpp>

namespace Dal::Script::Experimental {
    String_ Debugger_::String() const { return stack_.Top(); }

    void Debugger_::operator()(const std::unique_ptr<NodeCollect_>& node) { Debug(node, "COLLECT"); }

    void Debugger_::operator()(const std::unique_ptr<NodeTrue_>& node) { Debug(node, "TRUE"); }

    void Debugger_::operator()(const std::unique_ptr<NodeFalse_>& node) { Debug(node, "FALSE"); }

    void Debugger_::operator()(const std::unique_ptr<NodeUPlus_>& node) { Debug(node, "U-PLUS"); }

    void Debugger_::operator()(const std::unique_ptr<NodeUMinus_>& node) { Debug(node, "U-Minus"); }

    void Debugger_::operator()(const std::unique_ptr<NodePlus_>& node) { Debug(node, "PLUS"); }

    void Debugger_::operator()(const std::unique_ptr<NodeMinus_>& node) { Debug(node, "MINUS"); }

    void Debugger_::operator()(const std::unique_ptr<NodeMultiply_>& node) { Debug(node, "MULTIPLY"); }

    void Debugger_::operator()(const std::unique_ptr<NodeDivide_>& node) { Debug(node, "DIV"); }

    void Debugger_::operator()(const std::unique_ptr<NodePower_>& node) { Debug(node, "POW"); }

    void Debugger_::operator()(const std::unique_ptr<NodeLog_>& node) { Debug(node, "LOG"); }

    void Debugger_::operator()(const std::unique_ptr<NodeSqrt_>& node) { Debug(node, "SQRT"); }

    void Debugger_::operator()(const std::unique_ptr<NodeMax_>& node) { Debug(node, "MAX"); }

    void Debugger_::operator()(const std::unique_ptr<NodeMin_>& node) { Debug(node, "MIN"); }

    void Debugger_::operator()(const std::unique_ptr<NodeConst_>& node) {
        Debug(node, String_("CONST[") + String::FromDouble(node->val_) + ']');
    }

    void Debugger_::operator()(const std::unique_ptr<NodeVar_>& node) { Debug(node, String_("VAR[") + String::FromInt(node->index_) + ']'); }

    void Debugger_::operator()(const std::unique_ptr<NodeAssign_>& node) { Debug(node, "ASSIGN"); }

    void Debugger_::operator()(const std::unique_ptr<NodeIf_>& node) {
        String_ s = "IF";
        s += "[FIRST ELSE = " + String::FromInt(node->firstElse_) + "]";
        Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeEqual_>& node) {
        String_ s = "EQUALZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeNot_>& node) {
        String_ s = "NOT";
        return Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeSuperior_>& node) {
        String_ s = "GTZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeSupEqual_>& node) {
        String_ s = "GTEQUALZERO";
        if (!node->discrete_)
            s += "[CONT,EPS=" + String::FromDouble(node->eps_) + "]";
        else {
            s += "[DISCRETE,";
            s += "BOUNDS=" + String::FromDouble(node->lb_) + "," + String::FromDouble(node->ub_) + "]";
        }
        Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeAnd_>& node) {
        String_ s = "AND";
        return Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodeOr_>& node) {
        String_ s = "OR";
        return Debug(node, s);
    }

    void Debugger_::operator()(const std::unique_ptr<NodePays_>& node) { Debug(node, "PAYS"); }

    void Debugger_::operator()(const std::unique_ptr<NodeSpot_>& node) { Debug(node, "SPOT"); }

    void Debugger_::operator()(const std::unique_ptr<NodeSmooth_>& node) { Debug(node, "SMOOTH"); }
} // namespace Dal::Script

#endif