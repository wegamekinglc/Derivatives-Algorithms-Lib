//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <dal/script/node.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {

    class ScriptError_: public Exception_ {
    public:
        ScriptError_(const std::string& file, long line, const std::string& functionName, const char* msg)
            : Exception_(file, line, functionName, msg) {}
        ScriptError_(const std::string& file, long line, const std::string& functionName, const std::string& msg)
            : ScriptError_(file, line, functionName, msg.c_str()) {}
        ScriptError_(const std::string& file, long line, const std::string& functionName, const String_& msg)
            : ScriptError_(file, line, functionName, msg.c_str()) {}
    };

    class Parser_ {
        using TokIt_ = typename Vector_<String_>::const_iterator;

        // Helpers

        // Find matching closing char, for example matching ) for a (, skipping through nested pairs
        // does not change the cur iterator, assumed to be on the opening,
        // and returns an iterator on the closing match
        template <char OpChar, char ClChar> static TokIt_ FindMatch(TokIt_ cur, const TokIt_& end) {
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
        typedef Expression_ (*ParseFunc)(TokIt_&, const TokIt_&);

        template <ParseFunc FuncOnMatch, ParseFunc FuncOnNoMatch>
        static Expression_ ParseParentheses( TokIt_& cur, const TokIt_& end) {
            Expression_ tree;

            // Do we have an opening '('?
            if( *cur == "(") {
                // Find match
                auto closeIt = FindMatch<'(',')'>(cur, end);

                // Parse the parenthesed condition/expression, including nested parentheses,
                // by recursively calling the parent parseCond/parseExpr
                tree = FuncOnMatch(++cur, closeIt);

                // Advance cur after matching )
                cur = ++closeIt;
            }
            else {
                // No (, so leftmost we move one level up
                tree = FuncOnNoMatch(cur, end);
            }
            return tree;
        }

        static void ParseCondOptionals(TokIt_& cur, const TokIt_& end, double& eps);

        // Expressions
        static Statement_ ParseAssign(TokIt_& cur, const TokIt_& end, Expression_& lhs);
        static Statement_ ParsePays(TokIt_& cur, const TokIt_& end, Expression_& lhs);

        // Parent, Level1, '+' and '-'
        static Expression_ ParseExpr(TokIt_& cur, const TokIt_& end);
        // Level2, '*' and '/'
        static Expression_ ParseExprL2(TokIt_& cur, const TokIt_& end);
        // Level3, '^'
        static Expression_ ParseExprL3(TokIt_& cur, const TokIt_& end);
        // Level 4, unaries
        static Expression_ ParseExprL4(TokIt_& cur, const TokIt_& end);

        // Level 6, variables, constants, functions
        static Expression_ ParseVarConstFunc(TokIt_& cur, const TokIt_& end);
        static Expression_ ParseConst(TokIt_& cur);
        static Expression_ ParseVar(TokIt_& cur);
        static Expression_ ParseCond(TokIt_& cur, const TokIt_& end);
        static Expression_ ParseCondL2(TokIt_& cur, const TokIt_& end);
        static Expression_ ParseCondElem(TokIt_& cur, const TokIt_& end);
        static Vector_<Expression_> ParseFuncArg(TokIt_& cur, const TokIt_& end);

        static Statement_ ParseIf(TokIt_& cur, const TokIt_& end);

        static Expression_ BuildEqual(Expression_& lhs, Expression_& rhs, double eps);
        static Expression_ BuildDifferent(Expression_& lhs, Expression_& rhs, double eps);
        static Expression_ BuildSuperior(Expression_& lhs, Expression_& rhs, double eps);
        static Expression_ BuildSupEqual(Expression_& lhs, Expression_& rhs, double eps);

    public:
        static Statement_ ParseStatement(TokIt_& cur, const TokIt_& end);
    };

    Vector_<String_> Tokenize(const String_& str);
    Event_ Parse(const String_& event);
}
