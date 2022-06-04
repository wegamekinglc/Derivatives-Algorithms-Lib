//
// Created by wegam on 2022/4/4.
//

#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>

namespace Dal::Script {
    std::unique_ptr<ScriptNode_> Parser_::ParseExpr(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL2(cur, end);
        while (cur != end && ((*cur)[0] == '+' || (*cur)[0] == '-')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL2(cur, end);
            lhs = op == '+' ? BuildBinary<NodePlus_>(lhs, rhs) : BuildBinary<NodeMinus_>(lhs, rhs);
        }
        return lhs;
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseExprL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL3(cur, end);
        while (cur != end && ((*cur)[0] == '*' || (*cur)[0] == '/')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL3(cur, end);
            lhs = op == '*' ? BuildBinary<NodeMultiply_>(lhs, rhs) : BuildBinary<NodeDivide_>(lhs, rhs);
        }
        return lhs;
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseExprL3(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL4(cur, end);
        while (cur != end && (*cur)[0] == '^') {
            ++cur;
            REQUIRE(cur != end, "unexpected end of statement");
            auto rhs = ParseExprL4(cur, end);
            lhs = BuildBinary<NodePower_>(lhs, rhs);
        }
        return lhs;
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseExprL4(TokIt_& cur, const TokIt_& end) {
        if (cur != end && ((*cur)[0] == '+' || (*cur)[0] == '-')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL4(cur, end);
            auto top = op == '+' ? MakeBaseNode<NodeUPlus_>() : MakeBaseNode<NodeUMinus_>();
            top->arguments_.Resize(1);
            top->arguments_[0] = move(rhs);
            return top;
        }
        return ParseParentheses<ParseExpr, ParseVarConstFunc>(cur, end);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseVarConstFunc(TokIt_& cur, const TokIt_& end) {
        if ((*cur)[0] == '.' || ((*cur)[0] >= '0' && (*cur)[0] <= '9'))
            return ParseConst(cur);

        std::unique_ptr<ScriptNode_> top;
        unsigned minArg, maxArg;
        if (*cur == "LOG") {
            top = MakeBaseNode<NodeLog_>();
            minArg = maxArg = 1;
        } else if (*cur == "SQRT") {
            top = MakeBaseNode<NodeSqrt_>();
            minArg = maxArg = 1;
        } else if (*cur == "MIN") {
            top = MakeBaseNode<NodeMin_>();
            minArg = 2;
            maxArg = 100;
        } else if (*cur == "MAX") {
            top = MakeBaseNode<NodeMax_>();
            minArg = 2;
            maxArg = 1000;
        } else
            THROW2((*cur) + " is not a valid function for script parsing", ScriptError_);

        if (top) {
            String_ func = *cur;
            ++cur;
            top->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(top->arguments_.size() >= minArg && top->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            return top;
        }

        // When everything else fails, we have a variable
        return ParseVar(cur);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseConst(TokIt_& cur) {
        double v = String::ToDouble(*cur);
        auto top = MakeNode<NodeConst_>(v);
        ++cur;
        return move(top);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseVar(TokIt_& cur) {
        REQUIRE2((*cur)[0] >= 'A' && (*cur)[0] <= 'Z', String_("Variable name ") + *cur + " is invalid", ScriptError_);
        auto top = MakeNode<NodeVar_>(*cur);
        ++cur;
        return move(top);
    }

    Vector_<std::unique_ptr<ScriptNode_>> Parser_::ParseFuncArg(TokIt_& cur, const TokIt_& end) {
        REQUIRE2((*cur)[0] == '(', "No opening ( following function name", ScriptError_);
        TokIt_ closeIt = FindMatch<'(', ')'>(cur, end);

        //	Parse expressions between parentheses
        Vector_<std::unique_ptr<ScriptNode_>> args;
        ++cur;
        while (cur != closeIt) {
            args.push_back(ParseExpr(cur, end));
            if ((*cur)[0] == ',')
                ++cur;
            else if (cur != closeIt)
                THROW2("Arguments must be separated by commas", ScriptError_);
        }
        cur = ++closeIt;
        return args;
    }
} // namespace Dal::Script
