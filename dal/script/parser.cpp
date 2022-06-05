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

    std::unique_ptr<ScriptNode_> Parser_::ParseIf(TokIt_& cur, const TokIt_& end) {
        ++cur;
        REQUIRE2(cur != end, "`if` is not followed by `then`", ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur == end || *cur != "then")
            THROW2("`if` is not followed by `then`", ScriptError_);
        ++cur;
        Vector_<std::unique_ptr<ScriptNode_>> stats;
        while (cur != end && *cur != "ELSE" && *cur != "ENDIF")
            stats.push_back(ParseStatement(cur, end));

        REQUIRE2(cur != end, "`if/then` is not followed by `else` or `endif`", ScriptError_);
        Vector_<std::unique_ptr<ScriptNode_>> elseStats;
        int elseIdx = -1;
        while (*cur == "ELSE") {
            ++cur;
            while (cur != end && *cur != "ENDIF")
                elseStats.push_back(ParseStatement(cur, end));
            REQUIRE2(cur != end, "`if/then/else` is not followed by `endif`", ScriptError_);
            elseIdx = stats.size() + 1;
        }

        auto top = MakeNode<NodeIf_>();
        top->arguments_.Resize(1 + stats.size() + elseStats.size());
        top->arguments_[0] = std::move(cond);
        for (auto i = 0; i < stats.size(); ++i)
            top->arguments_[i + 1] = std::move(stats[i]);
        for (auto i = 0; i < elseStats.size(); ++i)
            top->arguments_[i + elseIdx] = std::move(elseStats[i]);
        top->firstElse_ = elseIdx;

        ++cur;
        return std::move(top);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseStatement(TokIt_& cur, const TokIt_& end) {
        if (*cur == "IF")
            return ParseIf(cur, end);
        auto lhs = ParseVar(cur);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        if (*cur == "=")
            return ParseAssign(cur, end, lhs);
        else if (*cur == "PAYS") return ParsePays(cur, end, lhs);
        THROW2("statement without an instruction", ScriptError_);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseAssign(TokIt_& cur, const TokIt_& end, std::unique_ptr<ScriptNode_>& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return BuildBinary<NodeAssign_>(lhs, rhs);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParsePays(TokIt_& cur, const TokIt_& end, std::unique_ptr<ScriptNode_>& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return BuildBinary<NodePays_>(lhs, rhs);
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseCond(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseCondL2(cur, end);
        while (cur != end && *cur == "OR") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseCondL2(cur, end);
            lhs = BuildBinary<NodeOr_>(lhs, rhs);
        }
        return lhs;
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseCondL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
        while (cur != end && *cur == "AND") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
            lhs = BuildBinary<NodeAnd_>(lhs, rhs);
        }
        return lhs;
    }

    std::unique_ptr<ScriptNode_> Parser_::ParseCondElem(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExpr(cur, end);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        String_ comparator = *cur;
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);

        double eps;
        ParseCondOptionals(cur, end, eps);

        if (comparator == "=")
            return BuildEqual(lhs, rhs, eps);
        else if (comparator == "!=")
            return BuildDifferent(lhs, rhs, eps);
        else if (comparator == "<")
            return BuildSuperior(lhs, rhs, eps);
        else if (comparator == ">")
            return BuildSuperior(rhs, lhs, eps);
        else if (comparator == "<=")
            return BuildSupEqual(lhs, rhs, eps);
        else if (comparator == ">=")
            return BuildSupEqual(rhs, lhs, eps);
        else
            THROW2("elementary condition has no valid comparator", ScriptError_);

    }

    void Parser_::ParseCondOptionals(TokIt_& cur, const TokIt_& end, double& eps) {
        eps = 1e-12;
        while (*cur == ";" || *cur == ":") {
            const char c = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            eps = String::ToDouble(*cur);
            ++cur;
        }
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

    std::unique_ptr<ScriptNode_> Parser_::BuildEqual(std::unique_ptr<ScriptNode_>& lhs, std::unique_ptr<ScriptNode_>& rhs, double eps) {
        auto expr = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = MakeNode<NodeEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(expr);
        top->eps_ = eps;
        return top;
    }

    std::unique_ptr<ScriptNode_> Parser_::BuildDifferent(std::unique_ptr<ScriptNode_>& lhs, std::unique_ptr<ScriptNode_>& rhs, double eps) {
        auto eq = BuildEqual(lhs, rhs, eps);
        auto top = MakeNode<NodeNot_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        return top;
    }

    std::unique_ptr<ScriptNode_> Parser_::BuildSuperior(std::unique_ptr<ScriptNode_>& lhs, std::unique_ptr<ScriptNode_>& rhs, double eps) {
        auto eq = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = MakeNode<NodeSuperior_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }

    std::unique_ptr<ScriptNode_> Parser_::BuildSupEqual(std::unique_ptr<ScriptNode_>& lhs, std::unique_ptr<ScriptNode_>& rhs, double eps) {
        auto eq = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = MakeNode<NodeSupEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }
} // namespace Dal::Script
