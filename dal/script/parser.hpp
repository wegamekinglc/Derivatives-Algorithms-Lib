//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    using namespace Dal::Script::Experimental;

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
        using ParseFunc = ScriptNode_(TokIt_&, const TokIt_&);

        template <ParseFunc FuncOnMatch, ParseFunc FuncOnNoMatch>
        static ScriptNode_ ParseParentheses( TokIt_& cur, const TokIt_& end) {
            ScriptNode_ tree;

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
        static ScriptNode_ ParseAssign(TokIt_& cur, const TokIt_& end, ScriptNode_& lhs);
        static ScriptNode_ ParsePays(TokIt_& cur, const TokIt_& end, ScriptNode_& lhs);

        // Parent, Level1, '+' and '-'
        static ScriptNode_ ParseExpr(TokIt_& cur, const TokIt_& end);
        // Level2, '*' and '/'
        static ScriptNode_ ParseExprL2(TokIt_& cur, const TokIt_& end);
        // Level3, '^'
        static ScriptNode_ ParseExprL3(TokIt_& cur, const TokIt_& end);
        // Level 4, unaries
        static ScriptNode_ ParseExprL4(TokIt_& cur, const TokIt_& end);

        // Level 6, variables, constants, functions
        static ScriptNode_ ParseVarConstFunc(TokIt_& cur, const TokIt_& end);
        static ScriptNode_ ParseConst(TokIt_& cur);
        static ScriptNode_ ParseVar(TokIt_& cur);
        static ScriptNode_ ParseCond(TokIt_& cur, const TokIt_& end);
        static ScriptNode_ ParseCondL2(TokIt_& cur, const TokIt_& end);
        static ScriptNode_ ParseCondElem(TokIt_& cur, const TokIt_& end);
        static Vector_<ScriptNode_> ParseFuncArg(TokIt_& cur, const TokIt_& end);

        static ScriptNode_ ParseIf(TokIt_& cur, const TokIt_& end);

        static ScriptNode_ BuildEqual(ScriptNode_& lhs, ScriptNode_& rhs, double eps);
        static ScriptNode_ BuildDifferent(ScriptNode_& lhs, ScriptNode_& rhs, double eps);
        static ScriptNode_ BuildSuperior(ScriptNode_& lhs, ScriptNode_& rhs, double eps);
        static ScriptNode_ BuildSupEqual(ScriptNode_& lhs, ScriptNode_& rhs, double eps);

    public:
        static ScriptNode_ ParseStatement(TokIt_& cur, const TokIt_& end);
    };

    Vector_<String_> Tokenize(const String_& str);
    Vector_<ScriptNode_> Parse(const String_& event);
}
