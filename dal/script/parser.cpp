//
// Created by wegam on 2022/4/4.
//

#include <dal/script/experimental/node.hpp>
#include <dal/script/parser.hpp>

namespace Dal::Script {
    ScriptNode_ Parser_::ParseExpr(TokIt_& cur, const TokIt_& end) {
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

    ScriptNode_ Parser_::ParseExprL2(TokIt_& cur, const TokIt_& end) {
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

    ScriptNode_ Parser_::ParseExprL3(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL4(cur, end);
        while (cur != end && (*cur)[0] == '^') {
            ++cur;
            REQUIRE(cur != end, "unexpected end of statement");
            auto rhs = ParseExprL4(cur, end);
            lhs = BuildBinary<NodePower_>(lhs, rhs);
        }
        return lhs;
    }

    ScriptNode_ Parser_::ParseExprL4(TokIt_& cur, const TokIt_& end) {
        if (cur != end && ((*cur)[0] == '+' || (*cur)[0] == '-')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL4(cur, end);
            auto top = op == '+' ? MakeBaseNode<NodeUPlus_>() : MakeBaseNode<NodeUMinus_>();
            if (op == '+') {
                std::get<std::unique_ptr<NodeUPlus_>>(top)->arguments_.Resize(1);
                std::get<std::unique_ptr<NodeUPlus_>>(top)->arguments_[0] = move(rhs);
            } else {
                std::get<std::unique_ptr<NodeUMinus_>>(top)->arguments_.Resize(1);
                std::get<std::unique_ptr<NodeUMinus_>>(top)->arguments_[0] = move(rhs);
            }
            return top;
        }
        return ParseParentheses<ParseExpr, ParseVarConstFunc>(cur, end);
    }

    ScriptNode_ Parser_::ParseVarConstFunc(TokIt_& cur, const TokIt_& end) {
        if ((*cur)[0] == '.' || ((*cur)[0] >= '0' && (*cur)[0] <= '9'))
            return ParseConst(cur);

        ScriptNode_ top;
        bool empty = true;
        unsigned minArg, maxArg;
        if(*cur == "SPOT") {
            auto res = std::make_unique<NodeSpot_>();
            minArg = maxArg = 0;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        } else if (*cur == "LOG") {
            auto res = std::make_unique<NodeLog_>();
            minArg = maxArg = 1;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        } else if (*cur == "SQRT") {
            auto res = std::make_unique<NodeSqrt_>();
            minArg = maxArg = 1;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        } else if (*cur == "MIN") {
            auto res = std::make_unique<NodeMin_>();
            minArg = 2;
            maxArg = 100;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        } else if (*cur == "MAX") {
            auto res = std::make_unique<NodeMax_>();
            minArg = 2;
            maxArg = 1000;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        } else if(*cur == "SMOOTH") {
            auto res = std::make_unique<NodeSmooth_>();
            minArg = 4;
            maxArg = 4;
            String_ func = *cur;
            ++cur;
            res->arguments_ = ParseFuncArg(cur, end);
            REQUIRE2(res->arguments_.size() >= minArg && res->arguments_.size() <= maxArg,
                     String_("function ") + func + ": wrong number of arguments", ScriptError_);
            top = std::move(res);
            empty = false;
        }

        if (!empty) {
            return top;
        }

        // When everything else fails, we have a variable
        return ParseVar(cur);
    }

    ScriptNode_ Parser_::ParseConst(TokIt_& cur) {
        double v = String::ToDouble(*cur);
        auto top = std::make_unique<NodeConst_>(v);
        ++cur;
        return std::move(top);
    }

    ScriptNode_ Parser_::ParseVar(TokIt_& cur) {
        REQUIRE2((*cur)[0] >= 'A' && (*cur)[0] <= 'z', String_("Variable name ") + *cur + " is invalid", ScriptError_);
        auto top = std::make_unique<NodeVar_>(String_(*cur));
        ++cur;
        return std::move(top);
    }

    ScriptNode_ Parser_::ParseIf(TokIt_& cur, const TokIt_& end) {
        ++cur;
        REQUIRE2(cur != end, "`if` is not followed by `then`", ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur == end || *cur != "then")
            THROW2("`if` is not followed by `then`", ScriptError_);
        ++cur;
        Vector_<ScriptNode_> stats;
        while (cur != end && *cur != "ELSE" && *cur != "ENDIF")
            stats.push_back(ParseStatement(cur, end));

        REQUIRE2(cur != end, "`if/then` is not followed by `else` or `endif`", ScriptError_);
        Vector_<ScriptNode_> elseStats;
        int elseIdx = -1;
        while (*cur == "ELSE") {
            ++cur;
            while (cur != end && *cur != "ENDIF")
                elseStats.push_back(ParseStatement(cur, end));
            REQUIRE2(cur != end, "`if/then/else` is not followed by `endif`", ScriptError_);
            elseIdx = stats.size() + 1;
        }

        auto top = std::make_unique<NodeIf_>();
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

    ScriptNode_ Parser_::ParseAssign(TokIt_& cur, const TokIt_& end, ScriptNode_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return BuildBinary<NodeAssign_>(lhs, rhs);
    }

    ScriptNode_ Parser_::ParsePays(TokIt_& cur, const TokIt_& end, ScriptNode_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return BuildBinary<NodePays_>(lhs, rhs);
    }

    ScriptNode_ Parser_::ParseCond(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseCondL2(cur, end);
        while (cur != end && *cur == "OR") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseCondL2(cur, end);
            lhs = BuildBinary<NodeOr_>(lhs, rhs);
        }
        return lhs;
    }

    ScriptNode_ Parser_::ParseCondL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
        while (cur != end && *cur == "AND") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
            lhs = BuildBinary<NodeAnd_>(lhs, rhs);
        }
        return lhs;
    }

    ScriptNode_ Parser_::ParseCondElem(TokIt_& cur, const TokIt_& end) {
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
            return BuildSuperior(rhs, lhs, eps);
        else if (comparator == ">")
            return BuildSuperior(lhs, rhs, eps);
        else if (comparator == "<=")
            return BuildSupEqual(rhs, lhs, eps);
        else if (comparator == ">=")
            return BuildSupEqual(lhs, rhs, eps);
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

    Vector_<ScriptNode_> Parser_::ParseFuncArg(TokIt_& cur, const TokIt_& end) {
        REQUIRE2((*cur)[0] == '(', "No opening ( following function name", ScriptError_);
        TokIt_ closeIt = FindMatch<'(', ')'>(cur, end);

        //	Parse expressions between parentheses
        Vector_<ScriptNode_> args;
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

    ScriptNode_ Parser_::BuildEqual(ScriptNode_& lhs, ScriptNode_& rhs, double eps) {
        auto expr = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = std::make_unique<NodeEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(expr);
        top->eps_ = eps;
        return top;
    }

    ScriptNode_ Parser_::BuildDifferent(ScriptNode_& lhs, ScriptNode_& rhs, double eps) {
        auto eq = BuildEqual(lhs, rhs, eps);
        auto top = std::make_unique<NodeNot_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        return top;
    }

    ScriptNode_ Parser_::BuildSuperior(ScriptNode_& lhs, ScriptNode_& rhs, double eps) {
        auto eq = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = std::make_unique<NodeSuperior_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }

    ScriptNode_ Parser_::BuildSupEqual(ScriptNode_& lhs, ScriptNode_& rhs, double eps) {
        auto eq = BuildBinary<NodeMinus_>(lhs, rhs);
        auto top = std::make_unique<NodeSupEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }

    ScriptNode_ Parser_::ParseStatement(TokIt_& cur, const TokIt_& end) {
        if (*cur == "IF")
            return ParseIf(cur, end);
        auto lhs = ParseVar(cur);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        if (*cur == "=")
            return ParseAssign(cur, end, lhs);
        else if (*cur == "PAYS") return ParsePays(cur, end, lhs);
        THROW2("statement without an instruction", ScriptError_);
    }

    Vector_<String_> Tokenize(const String_& str) {
        static const std::regex r(R"([\w.]+|[/-]|,|;|:|[\(\)\+\*\^]|!=|>=|<=|[<>=])");
        Vector_<String_> v;
        for (std::regex_iterator<String_::const_iterator> it(str.begin(), str.end(), r), end; it != end; ++it)
            v.push_back(String_((*it)[0]));
        return v;
    }

    Vector_<ScriptNode_> Parse(const String_& event) {
        Vector_<ScriptNode_> e;
        auto tokens = Tokenize(event);
        Vector_<String_>::const_iterator it = tokens.begin();
        while (it != tokens.end())
            e.push_back(Parser_::ParseStatement(it, tokens.end()));
        return e;
    }
} // namespace Dal::Script
