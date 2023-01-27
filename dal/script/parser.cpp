//
// Created by wegam on 2022/4/4.
//

#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>


namespace Dal::Script {
    Expression_ Parser_::ParseExpr(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL2(cur, end);
        while (cur != end && ((*cur)[0] == '+' || (*cur)[0] == '-')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL2(cur, end);
            lhs = op == '+' ? MakeBaseBinary<NodeAdd>(lhs, rhs) : MakeBaseBinary<NodeSub>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL3(cur, end);
        while (cur != end && ((*cur)[0] == '*' || (*cur)[0] == '/')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL3(cur, end);
            lhs = op == '*' ? MakeBaseBinary<NodeMult>(lhs, rhs) : MakeBaseBinary<NodeDiv>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL3(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL4(cur, end);
        while (cur != end && (*cur)[0] == '^') {
            ++cur;
            REQUIRE(cur != end, "unexpected end of statement");
            auto rhs = ParseExprL4(cur, end);
            lhs = MakeBaseBinary<NodePow>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL4(TokIt_& cur, const TokIt_& end) {
        if (cur != end && ((*cur)[0] == '+' || (*cur)[0] == '-')) {
            char op = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL4(cur, end);
            auto top = op == '+' ? MakeBaseNode<NodeUplus>() : MakeBaseNode<NodeUminus>();
            top->arguments.Resize( 1);
            top->arguments[0] = move( rhs);
            return top;
        }
        return ParseParentheses<ParseExpr, ParseVarConstFunc>(cur, end);
    }

    Expression_ Parser_::ParseVarConstFunc(TokIt_& cur, const TokIt_& end) {
        if ((*cur)[0] == '.' || ((*cur)[0] >= '0' && (*cur)[0] <= '9'))
            return ParseConst(cur);

        Expression_ top;
        bool empty = true;
        unsigned minArg, maxArg;
        if(*cur == "SPOT") {
            top = MakeBaseNode<NodeSpot>();
            minArg = maxArg = 0;
        } else if (*cur == "LOG") {
            top = MakeBaseNode<NodeLog>();
            minArg = maxArg = 1;
        } else if (*cur == "SQRT") {
            top = MakeBaseNode<NodeSqrt>();
            minArg = maxArg = 1;
        } else if (*cur == "MIN") {
            top = MakeBaseNode<NodeMin>();
            minArg = 2;
            maxArg = 1000;
        } else if (*cur == "MAX") {
            top = MakeBaseNode<NodeMax>();
            minArg = 2;
            maxArg = 1000;
        } else if(*cur == "SMOOTH") {
            top = MakeBaseNode<NodeSmooth>();
            minArg = 4;
            maxArg = 4;
        }

        if (top) {
            String_ func = *cur;
            ++cur;

            //	Matched a function, parse its arguments and check
            top->arguments = ParseFuncArg(cur, end);
            if( top->arguments.size() < minArg || top->arguments.size() > maxArg)
                THROW2(String_( "Function ") + func + String_(": wrong number of arguments"), ScriptError_);

            //	Return
            return top;
        }

        // When everything else fails, we have a variable
        return ParseVar(cur);
    }

    Expression_ Parser_::ParseConst(TokIt_& cur) {
        double v = String::ToDouble(*cur);
        auto top = MakeNode<NodeConst>(v);
        ++cur;
        return std::move(top);
    }

    Expression_ Parser_::ParseVar(TokIt_& cur) {
        REQUIRE2((*cur)[0] >= 'A' && (*cur)[0] <= 'z', String_("Variable name ") + *cur + " is invalid", ScriptError_);
        auto top = MakeNode<NodeVar>(String_(*cur));
        ++cur;
        return std::move(top);
    }

    Statement_ Parser_::ParseIf(TokIt_& cur, const TokIt_& end) {
        ++cur;
        REQUIRE2(cur != end, "`if` is not followed by `then`", ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur == end || *cur != "then")
            THROW2("`if` is not followed by `then`", ScriptError_);
        ++cur;
        Vector_<Statement_> stats;
        while (cur != end && *cur != "ELSE" && *cur != "ENDIF")
            stats.push_back(ParseStatement(cur, end));

        REQUIRE2(cur != end, "`if/then` is not followed by `else` or `endif`", ScriptError_);
        Vector_<Statement_> elseStats;
        int elseIdx = -1;
        while (*cur == "ELSE") {
            ++cur;
            while (cur != end && *cur != "ENDIF")
                elseStats.push_back(ParseStatement(cur, end));
            REQUIRE2(cur != end, "`if/then/else` is not followed by `endif`", ScriptError_);
            elseIdx = stats.size() + 1;
        }

        auto top = MakeNode<NodeIf>();
        top->arguments.Resize(1 + stats.size() + elseStats.size());
        top->arguments[0] = std::move(cond);
        for (auto i = 0; i < stats.size(); ++i)
            top->arguments[i + 1] = std::move(stats[i]);
        for (auto i = 0; i < elseStats.size(); ++i)
            top->arguments[i + elseIdx] = std::move(elseStats[i]);
        top->firstElse = elseIdx;

        ++cur;
        return std::move(top);
    }

    Statement_ Parser_::ParseAssign(TokIt_& cur, const TokIt_& end, Expression_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return MakeBaseBinary<NodeAssign>(lhs, rhs);
    }

    Statement_ Parser_::ParsePays(TokIt_& cur, const TokIt_& end, Expression_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return MakeBaseBinary<NodePays>(lhs, rhs);
    }

    Expression_ Parser_::ParseCond(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseCondL2(cur, end);
        while (cur != end && *cur == "OR") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseCondL2(cur, end);
            lhs = MakeBaseBinary<NodeOr>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseCondL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
        while (cur != end && *cur == "AND") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseParentheses<ParseCond, ParseCondElem>(cur, end);
            lhs = MakeBaseBinary<NodeAnd>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseCondElem(TokIt_& cur, const TokIt_& end) {
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
        eps = -1.0;
        while (*cur == ";" || *cur == ":") {
            const char c = (*cur)[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            eps = String::ToDouble(*cur);
            ++cur;
        }
    }

    Vector_<Expression_> Parser_::ParseFuncArg(TokIt_& cur, const TokIt_& end) {
        REQUIRE2((*cur)[0] == '(', "No opening ( following function name", ScriptError_);
        TokIt_ closeIt = FindMatch<'(', ')'>(cur, end);

        //	Parse expressions between parentheses
        Vector_<Expression_> args;
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

    Expression_ Parser_::BuildEqual(Expression_& lhs, Expression_& rhs, double eps) {
        auto expr = MakeBaseBinary<NodeSub>(lhs, rhs);
        auto top = MakeNode<NodeEqual>();
        top->arguments.Resize(1);
        top->arguments[0] = std::move(expr);
        top->eps = eps;
        return top;
    }

    Expression_ Parser_::BuildDifferent(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = BuildEqual(lhs, rhs, eps);
        auto top = std::make_unique<NodeNot>();
        top->arguments.Resize(1);
        top->arguments[0] = std::move(eq);
        return top;
    }

    Expression_ Parser_::BuildSuperior(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = MakeBaseBinary<NodeSub>(lhs, rhs);
        auto top = MakeNode<NodeSup>();
        top->arguments.Resize(1);
        top->arguments[0] = std::move(eq);
        top->eps = eps;
        return top;
    }

    Expression_ Parser_::BuildSupEqual(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = MakeBaseBinary<NodeSub>(lhs, rhs);
        auto top = MakeNode<NodeSupEqual>();
        top->arguments.Resize(1);
        top->arguments[0] = std::move(eq);
        top->eps = eps;
        return top;
    }

    Statement_ Parser_::ParseStatement(TokIt_& cur, const TokIt_& end) {
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

    Event_ Parse(const String_& event) {
        Event_ e;
        auto tokens = Tokenize(event);
        Vector_<String_>::const_iterator it = tokens.begin();
        while (it != tokens.end())
            e.push_back(Parser_::ParseStatement(it, tokens.end()));
        return e;
    }
} // namespace Dal::Script
