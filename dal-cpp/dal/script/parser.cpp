//
// Created by wegam on 2022/4/4.
//

#include <algorithm>
#include <cmath>
#include <set>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/vectorops.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/daybasis.hpp>

namespace {
    const std::set<Dal::String_> RESERVED_KEY_WORDS = {"IF",  "END", "THEN", "ELSE", "DCF", "PAYS",     "AND", "OR",     "SPOT", "MAX",
                                                       "MIN", "LOG", "SQRT", "EXP",  "FIX", "EXERCISE", "FOR", "APPEND", "SUM",  "AVERAGE"};
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
            top->arguments_.Resize(1);
            top->arguments_[0] = std::move(rhs);
            return top;
        }
        return ParseParentheses<&Parser_::ParseExpr, &Parser_::ParseVarConstFunc>(cur, end);
    }

    Expression_ Parser_::ParseVarConstFunc(TokIt_& cur, const TokIt_& end) {
        REQUIRE2(cur != end, "unexpected end of expression", ScriptError_);
        if (cur->Text() == "FIX")
            return ParseFix(cur, end);
        if (std::holds_alternative<IndexLiteral_>(cur->value_))
            return ParseVectorEntry(cur);
        if (cur->Text() == "SUM" || cur->Text() == "AVERAGE" || cur->Text() == "MIN" || cur->Text() == "MAX") {
            auto argument = cur;
            ++argument;
            if (argument != end && argument->Text() == "(") {
                ++argument;
                if (argument != end && !std::holds_alternative<IndexLiteral_>(argument->value_)) {
                    ++argument;
                    if (argument != end && argument->Text() == ")")
                        return ParseVectorReduction(cur, end);
                }
            }
        }
        if (cur->Text()[0] == '.' || (cur->Text()[0] >= '0' && cur->Text()[0] <= '9'))
            return ParseConst(cur);

        Expression_ top;
        bool empty = true;
        unsigned minArg, maxArg;
        if (cur->Text() == "SPOT") {
            auto spot = MakeNode<NodeSpot_>();
            spot->source_ = cur->source_;
            top = std::move(spot);
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
            } else {
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
        if (std::holds_alternative<IndexLiteral_>(cur->value_))
            return ParseVectorEntry(cur);
        REQUIRE2(cur->Text() != "FIX", "ReservedIdentifier: FIX is a function; rename the variable; " + cur->source_.Describe(), ScriptError_);
        REQUIRE2(cur->Text() != "EXERCISE",
                 "ReservedIdentifier: EXERCISE is a statement; rename the variable; " + cur->source_.Describe(), ScriptError_);
        REQUIRE2(cur->Text()[0] >= 'A' && cur->Text()[0] <= 'z', String_("Variable name ") + cur->Text() + " is invalid", ScriptError_);
        REQUIRE2(RESERVED_KEY_WORDS.find(cur->Text()) == RESERVED_KEY_WORDS.end(),
                 String_("Variable name ") + cur->Text() + " is conflicted with an existing key word", ScriptError_);
        String_ name(cur->Text());
        REQUIRE2(!numericVectors_.count(name), "ImmutableVector: predefined vector requires indexed access; " + cur->source_.Describe(),
                 ScriptError_);
        Expression_ top;
        if (const auto loopIndex = loopIndices_.find(name); loopIndex != loopIndices_.end())
            top = MakeNode<NodeConst_>(loopIndex->second);
        else if (constVariables_.find(name) == constVariables_.end())
            top = MakeNode<NodeVar_>(String_(cur->Text()));
        else
            top = MakeNode<NodeConstVar_>(String_(cur->Text()), constVariables_[name]);
        ++cur;
        return std::move(top);
    }

    bool Parser_::IsBareName(const Token_& token) {
        return !std::holds_alternative<IndexLiteral_>(token.value_) && IsVectorIdentifier(token.Text()) &&
               RESERVED_KEY_WORDS.find(token.Text()) == RESERVED_KEY_WORDS.end();
    }

    bool Parser_::IsVectorIdentifier(const String_& name) {
        static const String_ LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static const String_ NAME_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.";
        return !name.empty() && LETTERS.find(name.front()) != String_::npos && name.find_first_not_of(NAME_CHARS) == String_::npos;
    }

    bool Parser_::IsFreshLoopIndex(const Token_& token) const {
        return IsBareName(token) && !constVariables_.count(token.Text()) && !numericVectors_.count(token.Text()) && !loopIndices_.count(token.Text());
    }

    double Parser_::NumericConstant(const String_& key, const String_& error) const {
        if (String::IsNumber(key))
            return String::ToDouble(key);
        if (const auto constant = constVariables_.find(key); constant != constVariables_.end())
            return constant->second;
        if (const auto loopIndex = loopIndices_.find(key); loopIndex != loopIndices_.end())
            return loopIndex->second;
        THROW2(error, ScriptError_);
    }

    size_t Parser_::NonnegativeInteger(double value, const String_& error) {
        if (!std::isfinite(value) || value < 0.0 || value > 1000000.0 || std::floor(value) != value)
            THROW2(error, ScriptError_);
        return static_cast<size_t>(value);
    }

    String_ Parser_::ParseVectorName(TokIt_& cur, const TokIt_& end, const SourceLocation_& source, const String_& operation) {
        REQUIRE2(cur != end && IsBareName(*cur), operation + ": expected a vector name; " + source.Describe(), ScriptError_);
        const String_ name = cur->Text();
        REQUIRE2(!loopIndices_.count(name), "InvalidFor: loop index cannot name a vector; " + source.Describe(), ScriptError_);
        REQUIRE2(!constVariables_.count(name), operation + ": scalar constant is not a vector; " + source.Describe(), ScriptError_);
        ++cur;
        return name;
    }

    Expression_ Parser_::ParseVectorEntry(TokIt_& cur) {
        const String_ raw = cur->Text();
        const auto source = cur->source_;
        const auto open = raw.find('[');
        const auto close = raw.find(']');
        REQUIRE2(open != String_::npos && open > 0 && close == raw.size() - 1,
                 "InvalidVectorEntry: expected name[constant-index]; " + source.Describe(), ScriptError_);
        const String_ name(raw.substr(0, open));
        const String_ key(raw.substr(open + 1, close - open - 1));
        REQUIRE2(IsVectorIdentifier(name), "InvalidVectorEntry: invalid vector name; " + source.Describe(), ScriptError_);
        REQUIRE2(!loopIndices_.count(name), "InvalidFor: loop index cannot name a vector; " + source.Describe(), ScriptError_);
        REQUIRE2(RESERVED_KEY_WORDS.find(name) == RESERVED_KEY_WORDS.end(), "InvalidVectorEntry: reserved vector name; " + source.Describe(),
                 ScriptError_);
        REQUIRE2(!constVariables_.count(name), "InvalidVectorEntry: scalar constant is not a vector; " + source.Describe(), ScriptError_);
        const double value = NumericConstant(key, "InvalidVectorEntry: index must be an integer constant; " + source.Describe());
        const size_t entry =
            NonnegativeInteger(value, "InvalidVectorEntry: index must be a nonnegative integer at most 1000000; " + source.Describe());
        ++cur;
        if (const auto predefined = numericVectors_.find(name); predefined != numericVectors_.end())
            return MakeNode<NodeConst_>(ReadVectorEntry(predefined->second, entry, name + "; " + source.Describe()));
        return MakeNode<NodeVectorEntry_>(name, entry, source);
    }

    Expression_ Parser_::ParseVectorReduction(TokIt_& cur, const TokIt_& end) {
        const String_ function = cur->Text();
        const auto source = cur->source_;
        ++cur;
        REQUIRE2(cur != end && cur->Text() == "(", "InvalidVectorReduction: expected '('; " + source.Describe(), ScriptError_);
        ++cur;
        const String_ name = ParseVectorName(cur, end, source, "InvalidVectorReduction");
        REQUIRE2(cur != end && cur->Text() == ")", "InvalidVectorReduction: expected ')'; " + source.Describe(), ScriptError_);
        ++cur;
        const auto kind = function == "SUM"       ? NodeVectorReduce_::Kind_::Sum
                          : function == "AVERAGE" ? NodeVectorReduce_::Kind_::Average
                          : function == "MIN"     ? NodeVectorReduce_::Kind_::Minimum
                                                  : NodeVectorReduce_::Kind_::Maximum;
        if (const auto predefined = numericVectors_.find(name); predefined != numericVectors_.end())
            return MakeNode<NodeConst_>(ReduceVectorValues(predefined->second, kind, name + "; " + source.Describe()));
        return MakeNode<NodeVectorReduce_>(name, kind, source);
    }

    Statement_ Parser_::ParseVectorAppend(TokIt_& cur, const TokIt_& end) {
        const auto source = cur->source_;
        ++cur;
        REQUIRE2(cur != end && cur->Text() == "(", "InvalidVectorAppend: expected '('; " + source.Describe(), ScriptError_);
        auto close = FindMatch<'(', ')'>(cur, end);
        ++cur;
        auto append = MakeNode<NodeVectorAppend_>(ParseVectorName(cur, close, source, "InvalidVectorAppend"), source);
        REQUIRE2(!numericVectors_.count(append->name_), "ImmutableVector: APPEND cannot modify a predefined vector; " + source.Describe(),
                 ScriptError_);
        REQUIRE2(cur != close && cur->Text() == ",", "InvalidVectorAppend: expected ','; " + source.Describe(), ScriptError_);
        ++cur;
        REQUIRE2(cur != close, "InvalidVectorAppend: expected a value; " + source.Describe(), ScriptError_);
        append->arguments_.Resize(1);
        append->arguments_[0] = ParseExpr(cur, close);
        REQUIRE2(cur == close, "InvalidVectorAppend: unexpected tokens after value; " + source.Describe(), ScriptError_);
        cur = ++close;
        return append;
    }

    int Parser_::ParseForBound(TokIt_& cur, const TokIt_& end, const String_& context) {
        REQUIRE2(cur != end, "InvalidFor: missing loop bound" + context, ScriptError_);
        int sign = 1;
        if (cur->Text() == "-" || cur->Text() == "+") {
            sign = cur->Text() == "-" ? -1 : 1;
            ++cur;
        }
        REQUIRE2(cur != end, "InvalidFor: missing loop bound" + context, ScriptError_);
        const double value = sign * NumericConstant(cur->Text(), "InvalidFor: bound must be an integer constant" + context);
        ++cur;
        return static_cast<int>(NonnegativeInteger(value, "InvalidFor: bound must be a nonnegative integer at most 1000000" + context));
    }

    Parser_::ForHeader_ Parser_::ParseForHeader(TokIt_& cur, const TokIt_& end, const String_& context) {
        ++cur;
        REQUIRE2(cur != end && cur->Text() == "(", "InvalidFor: expected '('" + context, ScriptError_);
        ++cur;
        REQUIRE2(cur != end && IsFreshLoopIndex(*cur), "InvalidFor: expected a fresh loop index" + context, ScriptError_);
        const String_ indexName = cur->Text();
        ++cur;
        REQUIRE2(cur != end && cur->Text() == ",", "InvalidFor: expected ',' after loop index" + context, ScriptError_);
        ++cur;
        const int first = ParseForBound(cur, end, context);
        REQUIRE2(cur != end && cur->Text() == ",", "InvalidFor: expected ',' between bounds" + context, ScriptError_);
        ++cur;
        const int last = ParseForBound(cur, end, context);
        REQUIRE2(cur != end && cur->Text() == ")", "InvalidFor: expected ')'" + context, ScriptError_);
        REQUIRE2(last >= first && last - first <= 10000, "InvalidFor: range must contain at most 10000 iterations" + context, ScriptError_);
        ++cur;
        return {indexName, first, last};
    }

    Parser_::TokIt_ Parser_::ParseForIteration(TokIt_ body, const TokIt_& end, bool emit, NodeCollect_* collected, const String_& context) {
        while (body != end && body->Text() != "END") {
            auto statement = ParseStatement(body, end);
            if (emit) {
                REQUIRE2(expandedStatements_ < 100000, "InvalidFor: expanded program exceeds 100000 statements" + context, ScriptError_);
                ++expandedStatements_;
                collected->arguments_.push_back(std::move(statement));
            }
        }
        REQUIRE2(body != end, "InvalidFor: missing END" + context, ScriptError_);
        return body;
    }

    Statement_ Parser_::ParseFor(TokIt_& cur, const TokIt_& end) {
        const String_ context = "; " + cur->source_.Describe();
        const auto header = ParseForHeader(cur, end, context);
        const TokIt_ bodyStart = cur;
        TokIt_ bodyEnd = end;
        auto collected = MakeNode<NodeCollect_>();
        const bool hadPays = hasPays_;
        const bool hadExercise = hasExercise_;
        const String_ previousPreparationError = preparationError_;
        const size_t previousExpandedStatements = expandedStatements_;
        ++forLevel_;
        for (int i = header.first_; i < std::max(header.last_, header.first_ + 1); ++i) {
            loopIndices_[header.indexName_] = static_cast<double>(i);
            const TokIt_ body = ParseForIteration(bodyStart, end, i < header.last_, collected.get(), context);
            if (bodyEnd == end)
                bodyEnd = body;
            else
                REQUIRE2(body == bodyEnd, "InvalidFor: inconsistent loop body" + context, ScriptError_);
        }
        --forLevel_;
        loopIndices_.erase(header.indexName_);
        if (header.first_ == header.last_) {
            hasPays_ = hadPays;
            hasExercise_ = hadExercise;
            preparationError_ = previousPreparationError;
            expandedStatements_ = previousExpandedStatements;
        }
        cur = ++bodyEnd;
        return collected;
    }

    Statement_ Parser_::ParseIf(TokIt_& cur, const TokIt_& end) {
        ++cur;
        REQUIRE2(cur != end, "`if` is not followed by `then`", ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur == end || cur->Text() != "then")
            THROW2("`if` is not followed by `then`", ScriptError_);
        ++cur;
        Vector_<Statement_> stats;
        ++ifLevel_;
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
        --ifLevel_;

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
        hasPays_ = true;
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
            ++cur;
            REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
            eps = String::ToDouble(cur->Text());
            //  a zero width divides by zero in CSpr at the kink; a negative one would
            //  silently read as "unset" against the -1 sentinel
            REQUIRE2(std::isfinite(eps) && eps > 0.0,
                     "InvalidSmoothing: the ;eps option expects a finite positive width, got '" + cur->Text() + "'; " + cur->source_.Describe(),
                     ScriptError_);
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

    Expression_ Parser_::ParseExerciseCondition(TokIt_& cur, const TokIt_& end, const SourceLocation_& source) {
        ++cur; // the IF introducer, greedily bound to the exercise statement
        REQUIRE2(cur != end, "unexpected end of statement; EXERCISE requires a condition after IF; " + source.Describe(), ScriptError_);
        auto cond = ParseCond(cur, end);
        if (cur != end && RESERVED_KEY_WORDS.find(cur->Text()) != RESERVED_KEY_WORDS.end())
            THROW2("InvalidExerciseCondition: EXERCISE condition ends on the statement keyword '" + cur->Text() + "'; " + cur->source_.Describe(),
                   ScriptError_);
        return cond;
    }

    Statement_ Parser_::ParseExercise(TokIt_& cur, const TokIt_& end) {
        const auto source = cur->source_;
        hasExercise_ = true;
        ++cur;
        // Legacy assignment/payment to a variable named `exercise` reads better as a reserved-word conflict
        if (cur != end && (cur->Text() == "=" || cur->Text() == "PAYS"))
            THROW2("ReservedIdentifier: EXERCISE is a statement; rename the variable; " + source.Describe(), ScriptError_);
        REQUIRE2(cur != end, "unexpected end of statement; EXERCISE requires a value expression; " + source.Describe(), ScriptError_);
        auto top = MakeNode<NodeExercise_>();
        top->source_ = source;
        top->arguments_.Resize(1);
        top->arguments_[0] = ParseExpr(cur, end);
        if (cur != end && cur->Text() == "IF") {
            auto cond = ParseExerciseCondition(cur, end, source);
            if (const auto* comparison = FindFirstComparison(*cond))
                top->eps_ = comparison->eps_;
            top->arguments_.Resize(2);
            top->arguments_[1] = std::move(cond);
        }
        return top;
    }

    Statement_ Parser_::ParseStatement(TokIt_& cur, const TokIt_& end) {
        if (cur->Text() == "IF")
            return ParseIf(cur, end);
        if (cur->Text() == "FOR")
            return ParseFor(cur, end);
        if (cur->Text() == "APPEND")
            return ParseVectorAppend(cur, end);
        if (cur->Text() == "EXERCISE") {
            REQUIRE2(CanExercise(), "UnsupportedExerciseNesting: EXERCISE must be a top-level statement outside IF/FOR; " + cur->source_.Describe(),
                     ScriptError_);
            REQUIRE2(!hasExercise_, "DuplicateExercise: an event admits at most one EXERCISE statement; " + cur->source_.Describe(), ScriptError_);
            return ParseExercise(cur, end);
        }
        const auto source = cur->source_;
        auto lhs = ParseVar(cur);
        REQUIRE2(dynamic_cast<NodeVar_*>(lhs.get()) || dynamic_cast<NodeVectorEntry_*>(lhs.get()),
                 "InvalidAssignmentTarget: expected a mutable variable or vector entry; " + source.Describe(), ScriptError_);
        REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
        if (cur->Text() == "=") {
            if (dynamic_cast<NodeVectorEntry_*>(lhs.get())) {
                ++cur;
                REQUIRE2(cur != end, "unexpected end of statement", ScriptError_);
                auto rhs = ParseExpr(cur, end);
                return MakeBaseBinary<NodeVectorAssign_>(lhs, rhs);
            }
            return ParseAssign(cur, end, lhs);
        } else if (cur->Text() == "PAYS") {
            REQUIRE2(dynamic_cast<NodeVar_*>(lhs.get()), "InvalidPaymentTarget: expected a scalar variable; " + source.Describe(), ScriptError_);
            return ParsePays(cur, end, lhs);
        }
        THROW2("statement without an instruction", ScriptError_);
    }

    Event_ Parser_::Parse(const String_& event, const Vector_<SourceOrigin_>& origins) {
        preparationError_.clear();
        hasExercise_ = false;
        hasPays_ = false;
        ifLevel_ = 0;
        forLevel_ = 0;
        expandedStatements_ = 0;
        loopIndices_.clear();
        Event_ e;
        auto tokens = Lex(event, origins);
        Vector_<Token_>::const_iterator it = tokens.begin();
        while (it != tokens.end())
            e.push_back(ParseStatement(it, tokens.end()));
        return e;
    }
} // namespace Dal::Script
