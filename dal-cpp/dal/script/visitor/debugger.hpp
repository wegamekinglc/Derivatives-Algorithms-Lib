//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>

#include <dal/platform/platform.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor.hpp>

namespace Dal::Script {
    //  Debug IR: one entry per AST node, produced by Debugger_.  The legacy
    //  s-expression label feeds the text renderer; kind and the structured
    //  fields feed the JSON and tree renderers.
    struct DebugNode_ {
        String_ label;
        String_ kind;
        String_ name;          //  var, const_var
        int index = -1;        //  var, const_var
        int firstElse = -1;    //  if
        double number = 0.0;   //  const / const_var value, comparison eps
        double lb = 0.0;       //  discrete comparison bounds
        double rb = 0.0;
        bool discrete = false; //  comparison mode
        Vector_<DebugNode_> children;
    };

    //  Shortest decimal representation that round-trips.
    inline String_ DebugNumber(double src) {
        if (!std::isfinite(src))
            return String_("null");
        for (int precision : {6, 9, 12, 17}) {
            std::ostringstream ost;
            ost << std::setprecision(precision) << src;
            const String_ text(ost.str());
            if (String::IsNumber(text) && String::ToDouble(text) == src)
                return text;
        }
        return String_(std::to_string(src));
    }

    //  Legacy s-expression text, byte-identical with the pre-IR debugger.
    inline void DebugNodeText(const DebugNode_& node, size_t depth, std::ostream& ost) {
        for (size_t i = 0; i < depth; ++i)
            ost << '\t';
        ost << node.label;
        if (node.children.empty()) {
            ost << '\n';
            return;
        }
        ost << "(\n";
        for (size_t i = 0; i < node.children.size(); ++i) {
            DebugNodeText(node.children[i], depth + 1, ost);
            if (i + 1 < node.children.size()) {
                for (size_t d = 0; d < depth; ++d)
                    ost << '\t';
                ost << ",\n";
            }
        }
        for (size_t i = 0; i < depth; ++i)
            ost << '\t';
        ost << ")\n";
    }

    inline void JsonWriteString(const String_& src, std::ostream& ost) {
        static const char* HEX = "0123456789abcdef";
        ost << '"';
        for (const char raw : src) {
            switch (raw) {
            case '"': ost << "\\\""; break;
            case '\\': ost << "\\\\"; break;
            case '\n': ost << "\\n"; break;
            case '\r': ost << "\\r"; break;
            case '\t': ost << "\\t"; break;
            default:
                if (raw >= 0x20)
                    ost << raw;
                else {
                    const auto val = static_cast<unsigned char>(raw);
                    ost << "\\u00" << HEX[val >> 4] << HEX[val & 0xf];
                }
                break;
            }
        }
        ost << '"';
    }

    //  Machine-friendly JSON; ids are pre-order and unique per dump.
    inline void DebugNodeJson(const DebugNode_& node, size_t& id, std::ostream& ost) {
        ost << "{\"id\":\"n" << id++ << "\",\"kind\":";
        JsonWriteString(node.kind, ost);
        const String_& k = node.kind;
        if (k == "if") {
            const size_t firstElse = node.firstElse < 0 ? node.children.size() : static_cast<size_t>(node.firstElse);
            ost << ",\"condition\":";
            DebugNodeJson(node.children[0], id, ost);
            ost << ",\"then\":[";
            for (size_t i = 1; i < firstElse; ++i) {
                if (i > 1)
                    ost << ',';
                DebugNodeJson(node.children[i], id, ost);
            }
            ost << "],\"else\":[";
            for (size_t i = firstElse; i < node.children.size(); ++i) {
                if (i > firstElse)
                    ost << ',';
                DebugNodeJson(node.children[i], id, ost);
            }
            ost << ']';
        } else if (k == "assign" || k == "pays") {
            ost << ",\"target\":";
            DebugNodeJson(node.children[0], id, ost);
            ost << ",\"value\":";
            DebugNodeJson(node.children[1], id, ost);
        } else if (k == "var" || k == "const_var") {
            ost << ",\"name\":";
            JsonWriteString(node.name, ost);
            ost << ",\"index\":" << node.index << (k == "var" ? ",\"const_value\":" : ",\"value\":")
                << DebugNumber(node.number);
        } else if (k == "const") {
            ost << ",\"value\":" << DebugNumber(node.number);
        } else if (k == "eq0" || k == "gt0" || k == "ge0") {
            if (node.discrete)
                ost << ",\"mode\":\"discrete\",\"lb\":" << DebugNumber(node.lb) << ",\"rb\":" << DebugNumber(node.rb);
            else
                ost << ",\"mode\":\"continuous\",\"eps\":" << DebugNumber(node.number);
        }
        if (k != "if" && k != "assign" && k != "pays" && !node.children.empty()) {
            ost << ",\"children\":[";
            for (size_t i = 0; i < node.children.size(); ++i) {
                if (i)
                    ost << ',';
                DebugNodeJson(node.children[i], id, ost);
            }
            ost << ']';
        }
        ost << '}';
    }

    //  Tree rendering symbols; the ascii set keeps constrained consoles readable.
    struct TreeStyle_ {
        const char* tee;     //  "├── "
        const char* elbow;   //  "└── "
        const char* pipe;    //  "│   "
        const char* blank;   //  "    "
        const char* plus;    //  "+"
        const char* minus;   //  "−"
        const char* times;   //  "×"
        const char* over;    //  "÷"
        const char* power;   //  "^"
        const char* negate;  //  "−"
        const char* logS;    //  "ln"
        const char* expS;    //  "exp"
        const char* sqrtS;   //  "√"
        const char* maxS;    //  "max"
        const char* minS;    //  "min"
        const char* eqS;     //  "="
        const char* gtS;     //  ">"
        const char* geS;     //  "≥"
        const char* andS;    //  "∧"
        const char* orS;     //  "∨"
        const char* notS;    //  "¬"
        const char* trueS;   //  "⊤"
        const char* falseS;  //  "⊥"
        const char* assignS; //  "←"
        const char* paysS;   //  "⇐"
        const char* thenS;   //  "▶ "
        const char* elseS;   //  "▷ "
        const char* condS;   //  "? "
        const char* eventS;  //  "📅"
        const char* dotS;    //  "·"
        const char* lAng;    //  "⟨"
        const char* rAng;    //  "⟩"
        const char* epsS;    //  "ε"
    };

    inline const TreeStyle_& TreeStyle(bool ascii) {
        struct Styles_ {
            TreeStyle_ unicode{"├── ", "└── ", "│   ", "    ", "+",  "−", "×", "÷", "^",  "−",  "ln",   "exp", "√",
                               "max", "min", "=",   ">",   "≥",  "∧", "∨", "¬", "⊤", "⊥",  "←",   "⇐",   "▶ ",
                               "▷ ",  "? ",  "📅",  "·",   "⟨",  "⟩", "ε"};
            TreeStyle_ ascii{"|-- ", "`-- ", "|   ", "    ", "+", "-", "*", "/", "^", "-", "ln", "exp", "sqrt",
                             "max", "min", "=", ">", ">=", "and", "or", "not", "true", "false", "<-", "<=", "> ",
                             ". ", "? ", "#", "@", "<", ">", "eps"};
        };
        static const Styles_ styles;
        return ascii ? styles.ascii : styles.unicode;
    }

    //  UTF-8 aware display width, doubling the CJK and emoji ranges.
    inline size_t DisplayWidth(const String_& src) {
        size_t width = 0;
        for (size_t i = 0; i < src.size();) {
            const auto lead = static_cast<unsigned char>(src[i]);
            unsigned codePoint;
            size_t length;
            if (lead < 0x80) {
                codePoint = lead;
                length = 1;
            } else if ((lead & 0xE0) == 0xC0) {
                codePoint = lead & 0x1F;
                length = 2;
            } else if ((lead & 0xF0) == 0xE0) {
                codePoint = lead & 0x0F;
                length = 3;
            } else {
                codePoint = lead & 0x07;
                length = 4;
            }
            for (size_t j = 1; j < length && i + j < src.size(); ++j)
                codePoint = (codePoint << 6) | (static_cast<unsigned char>(src[i + j]) & 0x3F);
            i += length;
            const bool wide = (codePoint >= 0x1100 && codePoint <= 0x115F) || (codePoint >= 0x2E80 && codePoint <= 0xA4CF) ||
                              (codePoint >= 0xAC00 && codePoint <= 0xD7A3) || (codePoint >= 0xF900 && codePoint <= 0xFAFF) ||
                              (codePoint >= 0xFE30 && codePoint <= 0xFE6F) || (codePoint >= 0xFF00 && codePoint <= 0xFF60) ||
                              (codePoint >= 0x1F300 && codePoint <= 0x1F64F) ||
                              (codePoint >= 0x1F900 && codePoint <= 0x1F9FF);
            width += wide ? 2 : 1;
        }
        return width;
    }

    inline int TreePrec(const String_& kind) {
        if (kind == "assign" || kind == "pays" || kind == "if" || kind == "collect")
            return 0;
        if (kind == "or")
            return 1;
        if (kind == "and")
            return 2;
        if (kind == "eq0" || kind == "gt0" || kind == "ge0")
            return 3;
        if (kind == "add" || kind == "sub")
            return 4;
        if (kind == "mul" || kind == "div")
            return 5;
        if (kind == "not" || kind == "neg" || kind == "uplus")
            return 6;
        if (kind == "pow")
            return 7;
        return 8;
    }

    inline String_ TreeInline(const DebugNode_& node, const TreeStyle_& st);

    inline String_ TreeParen(const DebugNode_& child, const TreeStyle_& st, int context) {
        const String_ inner = TreeInline(child, st);
        return TreePrec(child.kind) < context ? String_("(") + inner + ")" : inner;
    }

    inline String_ FuzzySuffix(const DebugNode_& node, const TreeStyle_& st) {
        if (node.discrete)
            return String_(" ") + st.lAng + "[" + DebugNumber(node.lb) + ", " + DebugNumber(node.rb) + "]" + st.rAng;
        //  eps is only a smoothing hint when positive; the parser marks unset with -1
        if (node.number > 0.0)
            return String_(" ") + st.lAng + st.epsS + "=" + DebugNumber(node.number) + st.rAng;
        return String_();
    }

    inline String_ TreeInline(const DebugNode_& node, const TreeStyle_& st) {
        const String_& k = node.kind;
        if (k == "const")
            return DebugNumber(node.number);
        if (k == "var" || k == "const_var")
            return node.name;
        if (k == "spot")
            return String_("spot()");
        if (k == "true" || k == "false")
            return String_(k == "true" ? st.trueS : st.falseS);
        if (k == "uplus")
            return TreeParen(node.children[0], st, 6);
        if (k == "neg") {
            const String_ operand = TreeInline(node.children[0], st);
            return node.children[0].kind == "neg" ? String_(st.negate) + "(" + operand + ")"
                                                  : String_(st.negate) + TreeParen(node.children[0], st, 7);
        }
        if (k == "log" || k == "exp" || k == "sqrt")
            return String_(k == "log" ? st.logS : k == "exp" ? st.expS : st.sqrtS) + "(" +
                   TreeInline(node.children[0], st) + ")";
        if (k == "add" || k == "sub" || k == "mul" || k == "div" || k == "pow") {
            const char* op = k == "add" ? st.plus : k == "sub" ? st.minus : k == "mul" ? st.times
                          : k == "div" ? st.over : st.power;
            const int prec = TreePrec(k);
            //  Context bumps keep a − (b − c), a ÷ (b ÷ c) and (a ^ b) ^ c unambiguous
            const int leftContext = k == "pow" ? 8 : prec;
            return TreeParen(node.children[0], st, leftContext) + " " + op + " " +
                   TreeParen(node.children[1], st, prec + 1);
        }
        if (k == "max" || k == "min") {
            String_ rtn = String_(k == "max" ? st.maxS : st.minS) + "(" + TreeInline(node.children[0], st);
            for (size_t i = 1; i < node.children.size(); ++i)
                rtn += ", " + TreeInline(node.children[i], st);
            return rtn + ")";
        }
        if (k == "eq0" || k == "gt0" || k == "ge0") {
            const char* op = k == "eq0" ? st.eqS : k == "gt0" ? st.gtS : st.geS;
            const DebugNode_& operand = node.children[0];
            //  Comparisons are normalized to expr OP 0; fold the subtraction back to lhs OP rhs
            const String_ comparison = operand.kind == "sub"
                ? TreeParen(operand.children[0], st, 3) + " " + op + " " + TreeParen(operand.children[1], st, 3)
                : TreeParen(operand, st, 3) + " " + op + " 0";
            return comparison + FuzzySuffix(node, st);
        }
        if (k == "and" || k == "or") {
            const int prec = TreePrec(k);
            String_ rtn = TreeParen(node.children[0], st, prec);
            for (size_t i = 1; i < node.children.size(); ++i)
                rtn += String_(" ") + (k == "and" ? st.andS : st.orS) + " " + TreeParen(node.children[i], st, prec);
            return rtn;
        }
        if (k == "not")
            return String_(st.notS) + TreeParen(node.children[0], st, 7);
        if (k == "assign" || k == "pays")
            return TreeInline(node.children[0], st) + " " + (k == "assign" ? st.assignS : st.paysS) + " " +
                   TreeInline(node.children[1], st);
        if (k == "if") {
            const size_t firstElse = node.firstElse < 0 ? node.children.size() : static_cast<size_t>(node.firstElse);
            String_ rtn = String_("if ") + TreeInline(node.children[0], st) + " then";
            for (size_t i = 1; i < firstElse; ++i)
                rtn += " " + TreeInline(node.children[i], st);
            for (size_t i = firstElse; i < node.children.size(); ++i)
                rtn += " else " + TreeInline(node.children[i], st);
            return rtn;
        }
        //  collect
        String_ rtn;
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i)
                rtn += "; ";
            rtn += TreeInline(node.children[i], st);
        }
        return rtn;
    }

    //  Human-friendly tree block: inline while it fits the width budget, branches below otherwise.
    //  `first` prefixes the node's own line, `cont` prefixes every continuation line.
    inline void DebugNodeTree(const DebugNode_& node, const String_& first, const String_& cont, const TreeStyle_& st,
                              size_t width, Vector_<String_>& out) {
        const String_ whole = first + TreeInline(node, st);
        if (DisplayWidth(whole) <= width) {
            out.push_back(whole);
            return;
        }
        if (node.children.empty()) {
            //  An oversized leaf has nothing to branch on
            out.push_back(whole);
            return;
        }

        struct Branch_ {
            const DebugNode_* node;
            String_ marker;
            bool connected;
        };
        const String_& k = node.kind;
        String_ header = first;
        Vector_<Branch_> branches;
        const auto add = [&](const DebugNode_& child, const char* marker, bool connected) {
            branches.push_back(Branch_{&child, String_(marker), connected});
        };

        if (k == "assign" || k == "pays") {
            header = first + TreeInline(node.children[0], st) + " " + (k == "assign" ? st.assignS : st.paysS);
            add(node.children[1], "", true);
        } else if (k == "if") {
            const size_t firstElse = node.firstElse < 0 ? node.children.size() : static_cast<size_t>(node.firstElse);
            const String_ condInline = TreeInline(node.children[0], st);
            if (DisplayWidth(first + "if " + condInline + " then") <= width)
                header = first + "if " + condInline + " then";
            else {
                header = first + "if";
                add(node.children[0], st.condS, false);
            }
            for (size_t i = 1; i < firstElse; ++i)
                add(node.children[i], st.thenS, false);
            for (size_t i = firstElse; i < node.children.size(); ++i)
                add(node.children[i], st.elseS, false);
        } else if (k == "eq0" || k == "gt0" || k == "ge0") {
            const char* op = k == "eq0" ? st.eqS : k == "gt0" ? st.gtS : st.geS;
            header = first + op + FuzzySuffix(node, st);
            add(node.children[0], "", true);
        } else if (k == "not" || k == "neg" || k == "uplus" || k == "log" || k == "exp" || k == "sqrt") {
            header = first + (k == "not" ? st.notS : k == "neg" ? st.negate : k == "uplus" ? "+" : k == "log" ? st.logS
                             : k == "exp" ? st.expS : st.sqrtS);
            add(node.children[0], "", true);
        } else if (k == "max" || k == "min" || k == "collect") {
            header = first + (k == "max" ? st.maxS : k == "min" ? st.minS : "");
            for (const auto& child : node.children)
                add(child, "", true);
        } else {
            //  Binary operators and any unlisted interior kind
            header = first + (k == "add" ? st.plus : k == "sub" ? st.minus : k == "mul" ? st.times
                        : k == "div" ? st.over : k == "pow" ? st.power : k.c_str());
            for (const auto& child : node.children)
                add(child, "", true);
        }

        out.push_back(header);
        for (size_t i = 0; i < branches.size(); ++i) {
            const bool last = i + 1 == branches.size();
            const Branch_& branch = branches[i];
            const String_ branchFirst = branch.connected ? cont + (last ? st.elbow : st.tee) + branch.marker
                                                         : cont + branch.marker;
            const String_ branchCont =
                branch.connected ? cont + (last ? st.blank : st.pipe) : cont + String_(DisplayWidth(branch.marker), ' ');
            DebugNodeTree(*branch.node, branchFirst, branchCont, st, width, out);
        }
    }

    class Debugger_ : public ConstVisitor_<Debugger_> {
        Stack_<DebugNode_> stack_;

        // The main function call from every node visitor
        void Debug(const Node_& node, DebugNode_ ir) {
            // Visit arguments_, right to left
            for (auto it = node.arguments_.rbegin(); it != node.arguments_.rend(); ++it)
                (*it)->Accept(*this);

            ir.children.Resize(node.arguments_.size());
            for (size_t i = 0; i < node.arguments_.size(); ++i)
                ir.children[i] = stack_.TopAndPop();
            stack_.Push(std::move(ir));
        }

        void DebugComp(const CompNode_& node, const char* label, const char* kind) {
            DebugNode_ ir;
            ir.label = label;
            ir.kind = kind;
            if (!node.isDiscrete_) {
                ir.label += String_("[CONT,EPS=" + std::to_string(node.eps_) + "]");
                ir.number = node.eps_;
            } else {
                ir.label += String_("[DISCRETE,BOUNDS=" + std::to_string(node.lb_) + "," + std::to_string(node.rb_) + "]");
                ir.discrete = true;
                ir.lb = node.lb_;
                ir.rb = node.rb_;
            }
            Debug(node, std::move(ir));
        }

    public:
        using ConstVisitor_::Visit;

        //  IR of the last accepted statement
        [[nodiscard]] const DebugNode_& Top() const { return stack_.Top(); }

        //  Legacy s-expression text of the last accepted statement
        [[nodiscard]] String_ String() const {
            std::ostringstream ost;
            DebugNodeText(Top(), 0, ost);
            return String_(ost.str());
        }

        // All concrete node visitors, Visit arguments_ by default unless overridden

        void Visit(const NodeCollect_& node) { Debug(node, {"COLLECT", "collect"}); }

        void Visit(const NodeUPlus_& node) { Debug(node, {"UPLUS", "uplus"}); }
        void Visit(const NodeUMinus_& node) { Debug(node, {"UMINUS", "neg"}); }
        void Visit(const NodeAdd_& node) { Debug(node, {"ADD", "add"}); }
        void Visit(const NodeSub_& node) { Debug(node, {"SUBTRACT", "sub"}); }
        void Visit(const NodeMulti_& node) { Debug(node, {"MULT", "mul"}); }
        void Visit(const NodeDiv_& node) { Debug(node, {"DIV", "div"}); }
        void Visit(const NodePow_& node) { Debug(node, {"POW", "pow"}); }
        void Visit(const NodeLog_& node) { Debug(node, {"LOG", "log"}); }
        void Visit(const NodeExp_& node) { Debug(node, {"EXP", "exp"}); }
        void Visit(const NodeSqrt_& node) { Debug(node, {"SQRT", "sqrt"}); }
        void Visit(const NodeMax_& node) { Debug(node, {"MAX", "max"}); }
        void Visit(const NodeMin_& node) { Debug(node, {"MIN", "min"}); }

        void Visit(const NodeEqual_& node) { DebugComp(node, "EQUALZERO", "eq0"); }

        void Visit(const NodeNot_& node) { Debug(node, {"NOT", "not"}); }

        void Visit(const NodeSup_& node) { DebugComp(node, "GTZERO", "gt0"); }
        void Visit(const NodeSupEqual_& node) { DebugComp(node, "GTEQUALZERO", "ge0"); }

        void Visit(const NodeAnd_& node) { Debug(node, {"AND", "and"}); }
        void Visit(const NodeOr_& node) { Debug(node, {"OR", "or"}); }
        void Visit(const NodeAssign_& node) { Debug(node, {"ASSIGN", "assign"}); }
        void Visit(const NodePays_& node) { Debug(node, {"PAYS", "pays"}); }
        void Visit(const NodeSpot_& node) { Debug(node, {"SPOT", "spot"}); }

        void Visit(const NodeIf_& node) {
            DebugNode_ ir;
            ir.label = String_("IF[FIRSTELSE=" + std::to_string(node.firstElse_) + "]");
            ir.kind = "if";
            ir.firstElse = node.firstElse_;
            Debug(node, std::move(ir));
        }

        void Visit(const NodeTrue_& node) { Debug(node, {"TRUE", "true"}); }
        void Visit(const NodeFalse_& node) { Debug(node, {"FALSE", "false"}); }

        void Visit(const NodeConst_& node) {
            DebugNode_ ir;
            ir.label = String_("CONST[" + std::to_string(node.constVal_) + "]");
            ir.kind = "const";
            ir.number = node.constVal_;
            Debug(node, std::move(ir));
        }

        void Visit(const NodeVar_& node) {
            DebugNode_ ir;
            ir.label = String_("VAR[") + node.name_ + String_(',' + std::to_string(node.index_)) + ',' +
                       String_(String::FromDouble(node.constVal_)) + ']';
            ir.kind = "var";
            ir.name = node.name_;
            ir.index = node.index_;
            ir.number = node.constVal_;
            Debug(node, std::move(ir));
        }

        void Visit(const NodeConstVar_& node) {
            DebugNode_ ir;
            ir.label = String_("CONST_VAR[") + node.name_ + String_(',' + std::to_string(node.index_)) + ',' +
                       String_(String::FromDouble(node.constVal_)) + ']';
            ir.kind = "const_var";
            ir.name = node.name_;
            ir.index = node.index_;
            ir.number = node.constVal_;
            Debug(node, std::move(ir));
        }
    };
} // namespace Dal::Script
