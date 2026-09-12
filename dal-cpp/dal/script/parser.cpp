//
// Created by wegam on 2022/4/4.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/index.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/daybasis.hpp>

namespace {
    const std::set<Dal::String_> RESERVED_KEY_WORDS = {"IF",   "END", "THEN", "ELSE", "DCF",  "PAYS", "AND", "OR",
                                                       "SPOT", "MAX", "MIN",  "LOG",  "SQRT", "EXP",  "FIX"};
} // namespace

namespace Dal::Script {
    Expression_ Parser_::ParseExpr(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL2(cur, end);
        while (cur != end && (cur->Text()[0] == '+' || cur->Text()[0] == '-')) {
            char op = cur->Text()[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL2(cur, end);
            lhs = op == '+' ? MakeBaseBinary<NodeAdd_>(lhs, rhs) : MakeBaseBinary<NodeSub_>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL3(cur, end);
        while (cur != end && (cur->Text()[0] == '*' || cur->Text()[0] == '/')) {
            char op = cur->Text()[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL3(cur, end);
            lhs = op == '*' ? MakeBaseBinary<NodeMulti_>(lhs, rhs) : MakeBaseBinary<NodeDiv_>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL3(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExprL4(cur, end);
        while (cur != end && cur->Text()[0] == '^') {
            ++cur;
            REQUIRE(cur != end, "unexpected end of statement");
            auto rhs = ParseExprL4(cur, end);
            lhs = MakeBaseBinary<NodePow_>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseExprL4(TokIt_& cur, const TokIt_& end) {
        if (cur != end && (cur->Text()[0] == '+' || cur->Text()[0] == '-')) {
            char op = cur->Text()[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseExprL4(cur, end);
            auto top = op == '+' ? MakeBaseNode<NodeUPlus_>() : MakeBaseNode<NodeUMinus_>();
            top->arguments_.Resize( 1);
            top->arguments_[0] = std::move( rhs);
            return top;
        }
        return ParseParentheses<&Parser_::ParseExpr, &Parser_::ParseVarConstFunc>(cur, end);
    }

    Expression_ Parser_::ParseVarConstFunc(TokIt_& cur, const TokIt_& end) {
        REQUIRE2(cur != end, "unexpected end of expression", ScriptError_);
        if (cur->Text() == "FIX")
            return ParseFix(cur, end);
        if (cur->Text()[0] == '.' || (cur->Text()[0] >= '0' && cur->Text()[0] <= '9'))
            return ParseConst(cur);

        Expression_ top;
        bool empty = true;
        unsigned minArg, maxArg;
        if (cur->Text() == "SPOT") {
            top = MakeBaseNode<NodeSpot_>();
            minArg = maxArg = 0;
        } else if (cur->Text() == "LOG") {
            top = MakeBaseNode<NodeLog_>();
            minArg = maxArg = 1;
        } else if (cur->Text() == "SQRT") {
            top = MakeBaseNode<NodeSqrt_>();
            minArg = maxArg = 1;
        } else if (cur->Text() == "EXP") {
            top = MakeBaseNode<NodeExp_>();
            minArg = maxArg = 1;
        } else if (cur->Text() == "MIN") {
            top = MakeBaseNode<NodeMin_>();
            minArg = 2;
            maxArg = 1000;
        } else if (cur->Text() == "MAX") {
            top = MakeBaseNode<NodeMax_>();
            minArg = 2;
            maxArg = 1000;
        } else if (cur->Text() == "DCF") {
            top = MakeBaseNode<NodeConst_>(0.0);
            minArg = 3;
            maxArg = 3;
        }

        if (top) {
            String_ func = cur->Text();
            ++cur;

            if (func == "DCF") {
                dynamic_cast<NodeConst_*>(top.get())->constVal_ = ParseDCF(cur, end);
            }
            else {
                //	Matched a function, parse its arguments_ and check
                top->arguments_ = ParseFuncArg(cur, end);
                if (top->arguments_.size() < minArg || top->arguments_.size() > maxArg)
                    THROW2(String_("Function ") + func + String_(": wrong number of arguments_"), ScriptError_);
            }
            //	Return
            return top;
        }

        // When everything else fails, we have a variable
        return ParseVar(cur);
    }

    Expression_ Parser_::ParseConst(TokIt_& cur) {
        double v = String::ToDouble(cur->Text());
        auto top = MakeNode<NodeConst_>(v);
        ++cur;
        return std::move(top);
    }

    Expression_ Parser_::ParseVar(TokIt_& cur) {
        REQUIRE2(cur->Text() != "FIX", "ReservedIdentifier: FIX is a function; rename the variable; " + cur->source_.Describe(), ScriptError_);
        REQUIRE2(!std::holds_alternative<IndexLiteral_>(cur->value_),
                 "InvalidIndex: index literal is only allowed inside FIX; " + cur->source_.Describe(), ScriptError_);
        REQUIRE2(cur->Text()[0] >= 'A' && cur->Text()[0] <= 'z', String_("Variable name ") + cur->Text() + " is invalid", ScriptError_);
        REQUIRE2(RESERVED_KEY_WORDS.find(cur->Text()) == RESERVED_KEY_WORDS.end(),
                 String_("Variable name ") + cur->Text() + " is conflicted with an existing key word", ScriptError_);
        String_ name(cur->Text());
        Expression_ top;
        if (constVariables_.find(name) == constVariables_.end())
            top = MakeNode<NodeVar_>(String_(cur->Text()));
        else
            top = MakeNode<NodeConstVar_>(String_(cur->Text()), constVariables_[name]);
        ++cur;
        return std::move(top);
    }

    Statement_ Parser_::ParseIf(TokIt_& cur, const TokIt_& end) {
        ++cur;
        REQUIRE2(cur != end, "`if` is not followed by `then`", ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur == end || cur->Text() != "then")
            THROW2("`if` is not followed by `then`", ScriptError_);
        ++cur;
        Vector_<Statement_> stats;
        while (cur != end && cur->Text() != "ELSE" && cur->Text() != "END")
            stats.push_back(ParseStatement(cur, end));

        REQUIRE2(cur != end, "`if/then` is not followed by `else` or `end`", ScriptError_);
        Vector_<Statement_> elseStats;
        int elseIdx = -1;
        while (cur->Text() == "ELSE") {
            ++cur;
            while (cur != end && cur->Text() != "END")
                elseStats.push_back(ParseStatement(cur, end));
            REQUIRE2(cur != end, "`if/then/else` is not followed by `end`", ScriptError_);
            elseIdx = static_cast<int>(stats.size()) + 1;
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

    Statement_ Parser_::ParseAssign(TokIt_& cur, const TokIt_& end, Expression_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return MakeBaseBinary<NodeAssign_>(lhs, rhs);
    }

    Statement_ Parser_::ParsePays(TokIt_& cur, const TokIt_& end, Expression_& lhs) {
        ++cur;
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        auto rhs = ParseExpr(cur, end);
        return MakeBaseBinary<NodePays_>(lhs, rhs);
    }

    Expression_ Parser_::ParseCond(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseCondL2(cur, end);
        while (cur != end && cur->Text() == "OR") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseCondL2(cur, end);
            lhs = MakeBaseBinary<NodeOr_>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseCondL2(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseParentheses<&Parser_::ParseCond, &Parser_::ParseCondElem>(cur, end);
        while (cur != end && cur->Text() == "AND") {
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            auto rhs = ParseParentheses<&Parser_::ParseCond, &Parser_::ParseCondElem>(cur, end);
            lhs = MakeBaseBinary<NodeAnd_>(lhs, rhs);
        }
        return lhs;
    }

    Expression_ Parser_::ParseCondElem(TokIt_& cur, const TokIt_& end) {
        auto lhs = ParseExpr(cur, end);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        String_ comparator = cur->Text();
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
        while (cur != end && (cur->Text() == ";" || cur->Text() == ":")) {
            const char c = cur->Text()[0];
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            eps = String::ToDouble(cur->Text());
            ++cur;
        }
    }

    double Parser_::ParseDCF(TokIt_& cur, const TokIt_& end) {
        // TODO: we assume `DCF` function won't contain another nested function
        REQUIRE2(cur != end && cur->Text()[0] == '(', "missing opening '(' after `DCF`", ScriptError_);
        auto closeIt = FindMatch<'(', ')'>(cur, end);
        ++cur;

        // Parse basis and dates between parentheses
        REQUIRE2(cur != closeIt, "missing `basis` for `DCF`", ScriptError_);
        String_ day_basis = "";
        while (cur != closeIt && cur->Text()[0] != ',') {
            day_basis += cur->Text();
            ++cur;
        }
        REQUIRE2(cur != closeIt, "missing `start` for `DCF`", ScriptError_);
        ++cur;
        while (cur != closeIt && cur->Text()[0] == ',')
            ++cur;
        REQUIRE2(cur != closeIt, "missing `start` for `DCF`", ScriptError_);

        String_ start_date = "";
        while (cur != closeIt && cur->Text()[0] != ',') {
            start_date += cur->Text();
            ++cur;
        }
        REQUIRE2(cur != closeIt, "missing `end` for `DCF`", ScriptError_);
        ++cur;
        while (cur != closeIt && cur->Text()[0] == ',')
            ++cur;
        REQUIRE2(cur != closeIt, "missing `end` for `DCF`", ScriptError_);

        String_ end_date = "";
        while (cur != closeIt && cur->Text()[0] != ',') {
            end_date += cur->Text();
            ++cur;
        }
        REQUIRE2(cur == closeIt, "too many arguments for `DCF`", ScriptError_);

        cur = ++closeIt;
        // TODO: we only implement normal day count fraction convention and leave `context` as empty
        return DayBasis_(day_basis)(Date::FromString(start_date), Date::FromString(end_date), nullptr);
    }

    Expression_ Parser_::ParseFix(TokIt_& cur, const TokIt_& end) {
        const auto functionSource = cur->source_;
        ++cur;
        REQUIRE2(cur != end && cur->Text() == "(",
                 "ReservedIdentifier: FIX is a function; use FIX(index[,date]) or rename the variable; " + functionSource.Describe(), ScriptError_);
        ++cur;
        REQUIRE2(cur != end && std::holds_alternative<IndexLiteral_>(cur->value_),
                 "InvalidIndex: FIX requires an unquoted index literal; " + functionSource.Describe(), ScriptError_);
        const auto literal = std::get<IndexLiteral_>(cur->value_);
        const auto source = cur->source_;
        Handle_<Index_> index;
        try {
            index = Handle_<Index_>(Index::Parse(literal.raw_));
        } catch (const Exception_& error) {
            THROW2(String_(error.what()) + "; " + source.Describe() + "; input=" + literal.raw_, ScriptError_);
        }
        REQUIRE2(!index.IsEmpty(), "InvalidIndex: parser returned an empty index; " + source.Describe(), ScriptError_);
        ++cur;
        std::optional<Date_> fixingDate;
        if (cur != end && cur->Text() == ",") {
            ++cur;
            fixingDate = ParseFixingDate(cur, end, source);
        }
        REQUIRE2(cur != end && cur->Text() == ")", "InvalidIndex: FIX requires one index and an optional date; " + functionSource.Describe(),
                 ScriptError_);
        ++cur;
        auto node = MakeNode<NodeFix_>(literal, index, fixingDate, source);
        if (preparationError_.empty())
            preparationError_ = node->PreparationError();
        return node;
    }

    Date_ Parser_::ParseFixingDate(TokIt_& cur, const TokIt_& end, const SourceLocation_& fallback) {
        const auto dateSource = cur == end ? fallback : cur->source_;
        String_ date;
        size_t nextOffset = dateSource.offset_;
        bool contiguous = true;
        while (cur != end && cur->Text() != ")") {
            contiguous = contiguous && cur->source_.offset_ == nextOffset;
            date += cur->Text();
            nextOffset = cur->source_.offset_ + cur->Text().size();
            ++cur;
        }
        static const std::regex ISO_DATE("[0-9]{4}-[0-9]{2}-[0-9]{2}");
        REQUIRE2(contiguous && std::regex_match(date, ISO_DATE),
                 "InvalidFixingDate: FIX requires a strict YYYY-MM-DD date literal; input=" + date + "; " + dateSource.Describe(), ScriptError_);
        try {
            const auto fixingDate = Date::FromString(date);
            REQUIRE(fixingDate.IsValid(), "date is outside the supported range");
            return fixingDate;
        } catch (const Exception_& error) {
            THROW2("InvalidFixingDate: " + date + "; " + dateSource.Describe() + "; " + String_(error.what()), ScriptError_);
        }
    }

    Vector_<Expression_> Parser_::ParseFuncArg(TokIt_& cur, const TokIt_& end) {
        REQUIRE2(cur != end && cur->Text()[0] == '(', "No opening ( following function name", ScriptError_);
        auto closeIt = FindMatch<'(', ')'>(cur, end);

        //	Parse expressions between parentheses
        Vector_<Expression_> args;
        ++cur;
        while (cur != closeIt) {
            args.push_back(ParseExpr(cur, end));
            if (cur != end && cur->Text()[0] == ',')
                ++cur;
            else if (cur != closeIt)
                THROW2("Arguments must be separated by commas", ScriptError_);
        }
        cur = ++closeIt;
        return args;
    }

    Expression_ Parser_::BuildEqual(Expression_& lhs, Expression_& rhs, double eps) {
        auto expr = MakeBaseBinary<NodeSub_>(lhs, rhs);
        auto top = MakeNode<NodeEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(expr);
        top->eps_ = eps;
        return top;
    }

    Expression_ Parser_::BuildDifferent(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = BuildEqual(lhs, rhs, eps);
        auto top = std::make_unique<NodeNot_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        return top;
    }

    Expression_ Parser_::BuildSuperior(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = MakeBaseBinary<NodeSub_>(lhs, rhs);
        auto top = MakeNode<NodeSup_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }

    Expression_ Parser_::BuildSupEqual(Expression_& lhs, Expression_& rhs, double eps) {
        auto eq = MakeBaseBinary<NodeSub_>(lhs, rhs);
        auto top = MakeNode<NodeSupEqual_>();
        top->arguments_.Resize(1);
        top->arguments_[0] = std::move(eq);
        top->eps_ = eps;
        return top;
    }

    Statement_ Parser_::ParseStatement(TokIt_& cur, const TokIt_& end) {
        if (cur->Text() == "IF")
            return ParseIf(cur, end);
        auto lhs = ParseVar(cur);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        if (cur->Text() == "=")
            return ParseAssign(cur, end, lhs);
        else if (cur->Text() == "PAYS")
            return ParsePays(cur, end, lhs);
        THROW2("statement without an instruction", ScriptError_);
    }

    Event_ Parser_::Parse(const String_& event, const Vector_<SourceOrigin_>& origins) {
        preparationError_.clear();
        Event_ e;
        auto tokens = Lex(event, origins);
        Vector_<Token_>::const_iterator it = tokens.begin();
        while (it != tokens.end())
            e.push_back(ParseStatement(it, tokens.end()));
        return e;
    }
} // namespace Dal::Script
