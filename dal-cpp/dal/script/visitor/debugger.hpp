//
// Created by wegam on 2022/2/14.
//

#pragma once

#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

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
            const auto byte = static_cast<unsigned char>(raw);
            switch (byte) {
            case '"': ost << "\\\""; break;
            case '\\': ost << "\\\\"; break;
            case '\n': ost << "\\n"; break;
            case '\r': ost << "\\r"; break;
            case '\t': ost << "\\t"; break;
            default:
                //  Bytes from 0x20 up pass through: JSON strings are UTF-8, so
                //  multi-byte sequences must not be \u-escaped byte by byte
                if (byte >= 0x20)
                    ost << raw;
                else
                    ost << "\\u00" << HEX[byte >> 4] << HEX[byte & 0xf];
                break;
            }
        }
        ost << '"';
    }

    inline void DebugNodeJson(const DebugNode_& node, size_t& id, std::ostream& ost);

    inline void JsonWriteChildren(const DebugNode_& node, size_t& id, std::ostream& ost) {
        ost << ",\"children\":[";
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i)
                ost << ',';
            DebugNodeJson(node.children[i], id, ost);
        }
        ost << ']';
    }

    inline bool JsonWriteIf(const DebugNode_& node, size_t& id, std::ostream& ost) {
        if (node.kind != "if")
            return false;
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
        return true;
    }

    inline bool JsonWriteAssign(const DebugNode_& node, size_t& id, std::ostream& ost) {
        if (node.kind != "assign" && node.kind != "pays")
            return false;
        ost << ",\"target\":";
        DebugNodeJson(node.children[0], id, ost);
        ost << ",\"value\":";
        DebugNodeJson(node.children[1], id, ost);
        return true;
    }

    inline bool JsonWriteNamed(const DebugNode_& node, size_t& id, std::ostream& ost) {
        if (node.kind != "var" && node.kind != "const_var")
            return false;
        ost << ",\"name\":";
        JsonWriteString(node.name, ost);
        ost << ",\"index\":" << node.index << (node.kind == "var" ? ",\"const_value\":" : ",\"value\":")
            << DebugNumber(node.number);
        return true;
    }

    inline bool JsonWriteConst(const DebugNode_& node, size_t& id, std::ostream& ost) {
        if (node.kind != "const")
            return false;
        ost << ",\"value\":" << DebugNumber(node.number);
        return true;
    }

    inline bool JsonWriteCompare(const DebugNode_& node, size_t& id, std::ostream& ost) {
        if (node.kind != "eq0" && node.kind != "gt0" && node.kind != "ge0")
            return false;
        if (node.discrete)
            ost << ",\"mode\":\"discrete\",\"lb\":" << DebugNumber(node.lb) << ",\"rb\":" << DebugNumber(node.rb);
        else
            ost << ",\"mode\":\"continuous\",\"eps\":" << DebugNumber(node.number);
        JsonWriteChildren(node, id, ost);
        return true;
    }

    //  Writes the kind-specific fields; false when the kind takes only children
    inline bool JsonWriteFields(const DebugNode_& node, size_t& id, std::ostream& ost) {
        return JsonWriteIf(node, id, ost) || JsonWriteAssign(node, id, ost) || JsonWriteNamed(node, id, ost) ||
               JsonWriteConst(node, id, ost) || JsonWriteCompare(node, id, ost);
    }

    //  Machine-friendly JSON; ids are pre-order and unique per dump.
    inline void DebugNodeJson(const DebugNode_& node, size_t& id, std::ostream& ost) {
        ost << "{\"id\":\"n" << id++ << "\",\"kind\":";
        JsonWriteString(node.kind, ost);
        if (!JsonWriteFields(node, id, ost) && !node.children.empty())
            JsonWriteChildren(node, id, ost);
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
    inline bool IsWideCodePoint(unsigned codePoint) {
        static const std::pair<unsigned, unsigned> WIDE[] = {{0x1100, 0x115F}, {0x2E80, 0xA4CF}, {0xAC00, 0xD7A3},
                                                             {0xF900, 0xFAFF}, {0xFE30, 0xFE6F}, {0xFF00, 0xFF60},
                                                             {0x1F300, 0x1F64F}, {0x1F900, 0x1F9FF}};
        for (const auto& range : WIDE)
            if (codePoint >= range.first && codePoint <= range.second)
                return true;
        return false;
    }

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
            width += IsWideCodePoint(codePoint) ? 2 : 1;
        }
        return width;
    }

    inline int TreePrec(const String_& kind) {
        static const std::map<String_, int> PRECEDENCE = {
            {"assign", 0}, {"pays", 0},     {"if", 0},      {"collect", 0}, {"or", 1},   {"and", 2},
            {"eq0", 3},    {"gt0", 3},      {"ge0", 3},     {"add", 4},     {"sub", 4},  {"mul", 5},
            {"div", 5},    {"not", 6},      {"neg", 6},     {"uplus", 6},   {"pow", 7}};
        const auto found = PRECEDENCE.find(kind);
        return found == PRECEDENCE.end() ? 8 : found->second;
    }

    inline String_ TreeInline(const DebugNode_& node, const TreeStyle_& st);

    inline String_ TreeParen(const DebugNode_& child, const TreeStyle_& st, int context) {
        const String_ inner = TreeInline(child, st);
        //  An if/return keeps both branches String_ — a ternary would mix base_t and String_
        if (TreePrec(child.kind) >= context)
            return inner;
        return String_("(") + inner + ")";
    }

    inline String_ FuzzySuffix(const DebugNode_& node, const TreeStyle_& st) {
        if (node.discrete)
            return String_(" ") + st.lAng + "[" + DebugNumber(node.lb) + ", " + DebugNumber(node.rb) + "]" + st.rAng;
        //  eps is only a smoothing hint when positive; the parser marks unset with -1
        if (node.number > 0.0)
            return String_(" ") + st.lAng + st.epsS + "=" + DebugNumber(node.number) + st.rAng;
        return String_();
    }

    inline bool TreeInlineLeaf(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        if (node.kind == "const")
            out = DebugNumber(node.number);
        else if (node.kind == "var" || node.kind == "const_var")
            out = node.name;
        else if (node.kind == "spot")
            out = String_("spot()");
        else if (node.kind == "true" || node.kind == "false")
            out = String_(node.kind == "true" ? st.trueS : st.falseS);
        else
            return false;
        return true;
    }

    inline const char* FunctionSymbol(const String_& kind, const TreeStyle_& st) {
        if (kind == "log")
            return st.logS;
        if (kind == "exp")
            return st.expS;
        return st.sqrtS;
    }

    inline String_ TreeInlineNeg(const DebugNode_& node, const TreeStyle_& st) {
        //  Parenthesize a nested neg (avoid "−−x") and a pow operand (avoid the
        //  "−a ^ b" ambiguity between −(a ^ b) and (−a) ^ b)
        if (node.children[0].kind == "neg")
            return String_(st.negate) + "(" + TreeInline(node.children[0], st) + ")";
        return String_(st.negate) + TreeParen(node.children[0], st, 8);
    }

    inline bool TreeInlineUnary(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        const String_& k = node.kind;
        if (k == "log" || k == "exp" || k == "sqrt") {
            out = String_(FunctionSymbol(k, st)) + "(" + TreeInline(node.children[0], st) + ")";
            return true;
        }
        if (k == "uplus") {
            out = TreeParen(node.children[0], st, 6);
            return true;
        }
        if (k == "neg")
            out = TreeInlineNeg(node, st);
        else
            return false;
        return true;
    }

    inline bool TreeInlineCall(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        if (node.kind != "max" && node.kind != "min")
            return false;
        out = String_(node.kind == "max" ? st.maxS : st.minS) + "(" + TreeInline(node.children[0], st);
        for (size_t i = 1; i < node.children.size(); ++i)
            out += ", " + TreeInline(node.children[i], st);
        out += ")";
        return true;
    }

    inline const char* BinarySymbol(const String_& kind, const TreeStyle_& st) {
        if (kind == "add")
            return st.plus;
        if (kind == "sub")
            return st.minus;
        if (kind == "mul")
            return st.times;
        return kind == "div" ? st.over : st.power;
    }

    inline bool TreeInlineBinary(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        const String_& k = node.kind;
        if (k != "add" && k != "sub" && k != "mul" && k != "div" && k != "pow")
            return false;
        const int prec = TreePrec(k);
        //  Context bumps keep a − (b − c), a ÷ (b ÷ c) and (a ^ b) ^ c unambiguous
        const int leftContext = k == "pow" ? 8 : prec;
        out = TreeParen(node.children[0], st, leftContext) + " " + BinarySymbol(k, st) + " " +
              TreeParen(node.children[1], st, prec + 1);
        return true;
    }

    inline bool TreeInlineLogical(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        const String_& k = node.kind;
        if (k == "not") {
            out = String_(st.notS) + TreeParen(node.children[0], st, 7);
            return true;
        }
        if (k != "and" && k != "or")
            return false;
        const int prec = TreePrec(k);
        out = TreeParen(node.children[0], st, prec);
        for (size_t i = 1; i < node.children.size(); ++i)
            out += String_(" ") + (k == "and" ? st.andS : st.orS) + " " + TreeParen(node.children[i], st, prec);
        return true;
    }

    inline bool TreeInlineCompare(const DebugNode_& node, const TreeStyle_& st, String_& out) {
        const String_& k = node.kind;
        if (k != "eq0" && k != "gt0" && k != "ge0")
            return false;
        const char* op = k == "eq0" ? st.eqS : k == "gt0" ? st.gtS : st.geS;
        const DebugNode_& operand = node.children[0];
        //  Comparisons are normalized to expr OP 0; fold the subtraction back to lhs OP rhs
        if (operand.kind == "sub")
            out = TreeParen(operand.children[0], st, 3) + " " + op + " " + TreeParen(operand.children[1], st, 3);
        else
            out = TreeParen(operand, st, 3) + " " + op + " 0";
        out += FuzzySuffix(node, st);
        return true;
    }

    inline String_ TreeInlineIf(const DebugNode_& node, const TreeStyle_& st) {
        const size_t firstElse = node.firstElse < 0 ? node.children.size() : static_cast<size_t>(node.firstElse);
        String_ rtn = String_("if ") + TreeInline(node.children[0], st) + " then";
        for (size_t i = 1; i < firstElse; ++i)
            rtn += " " + TreeInline(node.children[i], st);
        for (size_t i = firstElse; i < node.children.size(); ++i)
            rtn += " else " + TreeInline(node.children[i], st);
        return rtn;
    }

    inline String_ TreeInlineStatement(const DebugNode_& node, const TreeStyle_& st) {
        const String_& k = node.kind;
        if (k == "assign" || k == "pays")
            return TreeInline(node.children[0], st) + " " + (k == "assign" ? st.assignS : st.paysS) + " " +
                   TreeInline(node.children[1], st);
        if (k == "if")
            return TreeInlineIf(node, st);
        //  collect
        String_ rtn;
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i)
                rtn += "; ";
            rtn += TreeInline(node.children[i], st);
        }
        return rtn;
    }

    //  Best-effort single-line form; parens follow minimal-precedence rules.
    inline String_ TreeInline(const DebugNode_& node, const TreeStyle_& st) {
        String_ out;
        if (TreeInlineLeaf(node, st, out) || TreeInlineUnary(node, st, out) || TreeInlineCall(node, st, out) ||
            TreeInlineBinary(node, st, out) || TreeInlineLogical(node, st, out) || TreeInlineCompare(node, st, out))
            return out;
        return TreeInlineStatement(node, st);
    }

    //  A child of an overflowing node, with its branch prefix and connector mode
    struct TreeBranch_ {
        const DebugNode_* node;
        String_ marker;
        bool connected;
    };

    inline void TreeBranchIf(const DebugNode_& node, const String_& first, const TreeStyle_& st, size_t width,
                             String_& header, Vector_<TreeBranch_>& branches) {
        const size_t firstElse = node.firstElse < 0 ? node.children.size() : static_cast<size_t>(node.firstElse);
        const String_ condInline = TreeInline(node.children[0], st);
        if (DisplayWidth(first + "if " + condInline + " then") <= width)
            header = first + "if " + condInline + " then";
        else {
            header = first + "if";
            branches.push_back(TreeBranch_{&node.children[0], String_(st.condS), false});
        }
        for (size_t i = 1; i < firstElse; ++i)
            branches.push_back(TreeBranch_{&node.children[i], String_(st.thenS), false});
        for (size_t i = firstElse; i < node.children.size(); ++i)
            branches.push_back(TreeBranch_{&node.children[i], String_(st.elseS), false});
    }

    //  Fills the statement-family header (assign, pays, if); false for other kinds
    inline bool TreeBranchStatement(const DebugNode_& node, const String_& first, const TreeStyle_& st, size_t width,
                                    String_& header, Vector_<TreeBranch_>& branches) {
        const String_& k = node.kind;
        if (k == "assign" || k == "pays") {
            header = first + TreeInline(node.children[0], st) + " " + (k == "assign" ? st.assignS : st.paysS);
            branches.push_back(TreeBranch_{&node.children[1], String_(), true});
            return true;
        }
        if (k != "if")
            return false;
        TreeBranchIf(node, first, st, width, header, branches);
        return true;
    }

    inline const char* UnarySymbol(const String_& kind, const TreeStyle_& st) {
        if (kind == "not")
            return st.notS;
        if (kind == "neg")
            return st.negate;
        if (kind == "uplus")
            return "+";
        return FunctionSymbol(kind, st);
    }

    inline const char* CompareSymbol(const String_& kind, const TreeStyle_& st) {
        if (kind == "eq0")
            return st.eqS;
        if (kind == "gt0")
            return st.gtS;
        return st.geS;
    }

    inline void PushOperand(const DebugNode_& node, Vector_<TreeBranch_>& branches) {
        branches.push_back(TreeBranch_{&node.children[0], String_(), true});
    }

    inline bool TreeBranchCompare(const DebugNode_& node, const String_& first, const TreeStyle_& st, String_& header,
                                  Vector_<TreeBranch_>& branches) {
        if (node.kind != "eq0" && node.kind != "gt0" && node.kind != "ge0")
            return false;
        header = first + CompareSymbol(node.kind, st) + FuzzySuffix(node, st);
        PushOperand(node, branches);
        return true;
    }

    inline bool TreeBranchUnaryHeader(const DebugNode_& node, const String_& first, const TreeStyle_& st, String_& header,
                                      Vector_<TreeBranch_>& branches) {
        if (node.kind != "not" && node.kind != "neg" && node.kind != "uplus" && node.kind != "log" &&
            node.kind != "exp" && node.kind != "sqrt")
            return false;
        header = first + UnarySymbol(node.kind, st);
        PushOperand(node, branches);
        return true;
    }

    //  Header symbol for everything that branches by operands: binary operators
    //  and max/min; collect gets no symbol
    inline String_ OperatorHeader(const String_& kind, const TreeStyle_& st) {
        if (kind == "max")
            return String_(st.maxS);
        if (kind == "min")
            return String_(st.minS);
        if (kind == "collect")
            return String_();
        if (kind == "pow")
            return String_(st.power);
        return String_(BinarySymbol(kind, st));
    }

    inline void TreeBranchOperators(const DebugNode_& node, const String_& first, const TreeStyle_& st, String_& header,
                                    Vector_<TreeBranch_>& branches) {
        header = first + OperatorHeader(node.kind, st);
        for (const auto& child : node.children)
            branches.push_back(TreeBranch_{&child, String_(), true});
    }

    inline void DebugNodeTree(const DebugNode_& node, const String_& first, const String_& cont, const TreeStyle_& st,
                              size_t width, Vector_<String_>& out);

    inline void EmitBranches(const Vector_<TreeBranch_>& branches, const String_& cont, const TreeStyle_& st,
                             size_t width, Vector_<String_>& out) {
        for (size_t i = 0; i < branches.size(); ++i) {
            const bool last = i + 1 == branches.size();
            const TreeBranch_& branch = branches[i];
            const String_ branchFirst =
                branch.connected ? cont + (last ? st.elbow : st.tee) + branch.marker : cont + branch.marker;
            const String_ branchCont =
                branch.connected ? cont + (last ? st.blank : st.pipe) : cont + String_(DisplayWidth(branch.marker), ' ');
            DebugNodeTree(*branch.node, branchFirst, branchCont, st, width, out);
        }
    }

    //  Human-friendly tree block: inline while it fits the width budget, branches below otherwise.
    //  `first` prefixes the node's own line, `cont` prefixes every continuation line.
    inline void DebugNodeTree(const DebugNode_& node, const String_& first, const String_& cont, const TreeStyle_& st,
                              size_t width, Vector_<String_>& out) {
        const String_ whole = first + TreeInline(node, st);
        //  Inline when it fits; an oversized leaf has nothing to branch on
        if (DisplayWidth(whole) <= width || node.children.empty()) {
            out.push_back(whole);
            return;
        }

        String_ header = first;
        Vector_<TreeBranch_> branches;
        if (!TreeBranchStatement(node, first, st, width, header, branches) &&
            !TreeBranchCompare(node, first, st, header, branches) && !TreeBranchUnaryHeader(node, first, st, header, branches))
            TreeBranchOperators(node, first, st, header, branches);

        out.push_back(header);
        EmitBranches(branches, cont, st, width, out);
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
