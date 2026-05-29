//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <map>
#include <dal/script/node.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {

    class Parser_ {
        using TokIt_ = Vector_<String_>::const_iterator;
        std::map<String_, double> constVariables_;

        // Helpers

        // Find matching closing char, for example matching ) for a (, skipping through nested pairs
        // does not change the cur iterator, assumed to be on the opening,
        // and returns an iterator on the closing match
        template <char OpChar, char ClChar>
        static TokIt_ FindMatch(TokIt_ cur, const TokIt_& end) {
            unsigned opens = 1;
            ++cur;
            while (cur != end && opens > 0) {
                opens += ((*cur)[0] == OpChar) - ((*cur)[0] == ClChar);
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
            Expression_ tree;

            // Do we have an opening "("?
            if( *cur == "(") {
                // Find match
                auto closeIt = FindMatch<'(',')'>(cur, end);

                // Parse the parentheses condition/expression, including nested parentheses,
                // by recursively calling the parent parseCond/parseExpr
                tree = (this->*FuncOnMatch_)(++cur, closeIt);

                // Advance cur after matching ")"
                cur = ++closeIt;
            }
            else {
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
        Expression_ ParseCond(TokIt_& cur, const TokIt_& end);
        Expression_ ParseCondL2(TokIt_& cur, const TokIt_& end);
        Expression_ ParseCondElem(TokIt_& cur, const TokIt_& end);
        Vector_<Expression_> ParseFuncArg(TokIt_& cur, const TokIt_& end);
        double ParseDCF(TokIt_& cur, const TokIt_& end);

        Statement_ ParseIf(TokIt_& cur, const TokIt_& end);

        Expression_ BuildEqual(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildDifferent(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildSuperior(Expression_& lhs, Expression_& rhs, double eps);
        Expression_ BuildSupEqual(Expression_& lhs, Expression_& rhs, double eps);

    public:
        explicit Parser_(const std::map<String_, double>& const_variables = std::map<String_, double>()): constVariables_(const_variables) {}
        Statement_ ParseStatement(TokIt_& cur, const TokIt_& end);
        Event_ Parse(const String_& event);
    };

    Vector_<String_> Tokenize(const String_& str);
}
