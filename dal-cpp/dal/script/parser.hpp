//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <map>
#include <dal/script/node.hpp>
#include <dal/script/lexer.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {

    class Parser_ {
        using TokIt_ = Vector_<Token_>::const_iterator;
        std::map<String_, double> constVariables_;
        std::map<String_, Vector_<double>> numericVectors_;
        std::map<String_, double> loopIndices_;
        String_ preparationError_;
        // One EXERCISE statement per event, and only at the event top level;
        // hasPays_/hasExercise_ let ScriptProduct_ answer payoff queries without re-walking the AST
        bool hasExercise_ = false;
        bool hasPays_ = false;
        size_t ifLevel_ = 0;
        size_t forLevel_ = 0;
        size_t expandedStatements_ = 0;

        // Helpers

        // Find matching closing char, for example matching ) for a (, skipping through nested pairs
        // does not change the cur iterator, assumed to be on the opening,
        // and returns an iterator on the closing match
        template <char OpChar, char ClChar>
        static TokIt_ FindMatch(TokIt_ cur, const TokIt_& end) {
            unsigned opens = 1;
            ++cur;
            while (cur != end && opens > 0) {
                opens += (cur->Text()[0] == OpChar) - (cur->Text()[0] == ClChar);
                ++cur;
            }

            if( cur == end && opens > 0)
                THROW2(String_( "opening ") + OpChar + " has no matching closing " + ClChar, ScriptError_);
            return --cur;
        }

        // Parentheses, Level5
        using ParseFunc = Expression_ (Parser_::*)(TokIt_&, const TokIt_&);

        template <ParseFunc FuncOnMatch_, ParseFunc FuncOnNoMatch_>
        Expression_ ParseParentheses(TokIt_& cur, const TokIt_& end) {
            REQUIRE2(cur != end, "unexpected end of expression", ScriptError_);
            Expression_ tree;

            // Do we have an opening "("?
            if (cur->Text() == "(") {
                // Find match
                auto closeIt = FindMatch<'(',')'>(cur, end);

                // Parse the parentheses condition/expression, including nested parentheses,
                // by recursively calling the parent parseCond/parseExpr
                tree = (this->*FuncOnMatch_)(++cur, closeIt);
                REQUIRE2(cur == closeIt, "unexpected trailing tokens in parentheses", ScriptError_);

                // Advance cur after matching ")"
                cur = ++closeIt;
            } else {
                // No (, so leftmost we move one level up
                tree = (this->*FuncOnNoMatch_)(cur, end);
            }
            return tree;
        }

        void ParseCondOptionals(TokIt_& cur, const TokIt_& end, double& eps);

        // Expressions
        Statement_ ParseAssign(TokIt_& cur, const TokIt_& end, Expression_& lhs);
        Statement_ ParsePays(TokIt_& cur, const TokIt_& end, Expression_& lhs);

        // Parent, Level1, '+' and '-'
        Expression_ ParseExpr(TokIt_& cur, const TokIt_& end);
        // Level2, '*' and '/'
        Expression_ ParseExprL2(TokIt_& cur, const TokIt_& end);
        // Level3, '^'
        Expression_ ParseExprL3(TokIt_& cur, const TokIt_& end);
        // Level 4, unaries
        Expression_ ParseExprL4(TokIt_& cur, const TokIt_& end);

        // Level 6, variables, constants, functions
        Expression_ ParseVarConstFunc(TokIt_& cur, const TokIt_& end);
        Expression_ ParseConst(TokIt_& cur);
        Expression_ ParseVar(TokIt_& cur);
        Expression_ ParseVectorEntry(TokIt_& cur);
        Expression_ ParseVectorReduction(TokIt_& cur, const TokIt_& end);
        Expression_ ParseCond(TokIt_& cur, const TokIt_& end);
        Expression_ ParseCondL2(TokIt_& cur, const TokIt_& end);
        Expression_ ParseCondElem(TokIt_& cur, const TokIt_& end);
        Vector_<Expression_> ParseFuncArg(TokIt_& cur, const TokIt_& end);
        double ParseDCF(TokIt_& cur, const TokIt_& end);
        Expression_ ParseFix(TokIt_& cur, const TokIt_& end);
        Date_ ParseFixingDate(TokIt_& cur, const TokIt_& end, const SourceLocation_& fallback);

        Statement_ ParseIf(TokIt_& cur, const TokIt_& end);
        Statement_ ParseFor(TokIt_& cur, const TokIt_& end);
        Statement_ ParseVectorAppend(TokIt_& cur, const TokIt_& end);
        Statement_ ParseExercise(TokIt_& cur, const TokIt_& end);
        Expression_ ParseExerciseCondition(TokIt_& cur, const TokIt_& end, const SourceLocation_& source);
        [[nodiscard]] static bool IsBareName(const Token_& token);
        [[nodiscard]] static bool IsVectorIdentifier(const String_& name);
        [[nodiscard]] bool IsFreshLoopIndex(const Token_& token) const;
        [[nodiscard]] bool CanExercise() const { return ifLevel_ == 0 && forLevel_ == 0; }
        [[nodiscard]] double NumericConstant(const String_& key, const String_& error) const;
        [[nodiscard]] static size_t NonnegativeInteger(double value, const String_& error);
        String_ ParseVectorName(TokIt_& cur, const TokIt_& end, const SourceLocation_& source, const String_& operation);
        int ParseForBound(TokIt_& cur, const TokIt_& end, const String_& context);
        struct ForHeader_ {
            String_ indexName_;
            int first_;
            int last_;
        };
        ForHeader_ ParseForHeader(TokIt_& cur, const TokIt_& end, const String_& context);
        TokIt_ ParseForIteration(TokIt_ body, const TokIt_& end, bool emit, NodeCollect_* collected, const String_& context);

        Expression_ BuildEqual(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildDifferent(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildSuperior(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildSupEqual(Expression_& lhs, Expression_& rhs, double eps);

    public:
        explicit Parser_(const std::map<String_, double>& constVariables = {}, const std::map<String_, Vector_<double>>& numericVectors = {})
            : constVariables_(constVariables), numericVectors_(numericVectors) {}
        Statement_ ParseStatement(TokIt_& cur, const TokIt_& end);
        Event_ Parse(const String_& event, const Vector_<SourceOrigin_>& origins = {});
        [[nodiscard]] const String_& PreparationError() const { return preparationError_; }
        // Whether the last Parse built a PAYS or EXERCISE statement (at any nesting depth for PAYS)
        [[nodiscard]] bool HasPays() const { return hasPays_; }
        [[nodiscard]] bool HasExercise() const { return hasExercise_; }
    };
} // namespace Dal::Script
