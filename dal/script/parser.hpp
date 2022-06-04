//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <regex>
#include <dal/script/event.hpp>
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
        using ParseFunc = std::unique_ptr<ScriptNode_>(TokIt_&, const TokIt_&);

        template <ParseFunc FuncOnMatch, ParseFunc FuncOnNoMatch>
        static std::unique_ptr<ScriptNode_> ParseParentheses( TokIt_& cur, const TokIt_& end) {
            std::unique_ptr<ScriptNode_> tree;

            // Do we have an opening '('?
            if( *cur == "(") {
                // Find match
                auto closeIt = FindMatch<'(',')'>(cur, end);

                // Parse the parenthesed condition/expression, including nested parentheses,
                // by recursively calling the parent parseCond/parseExpr
                tree = FuncOnMatch( ++cur, closeIt);

                // Advance cur after matching )
                cur = ++closeIt;
            }
            else {
                // No (, so leftmost we move one level up
                tree = FuncOnNoMatch( cur, end);
            }
            return tree;
        }

        // Expressions

        // Parent, Level1, '+' and '-'
        static std::unique_ptr<ScriptNode_> ParseExpr(TokIt_& cur, const TokIt_& end);
        // Level2, '*' and '/'
        static std::unique_ptr<ScriptNode_> ParseExprL2(TokIt_& cur, const TokIt_& end);
        // Level3, '^'
        static std::unique_ptr<ScriptNode_> ParseExprL3(TokIt_& cur, const TokIt_& end);
        // Level 4, unaries
        static std::unique_ptr<ScriptNode_> ParseExprL4(TokIt_& cur, const TokIt_& end);

        // Level 6, variables, constants, functions
        static std::unique_ptr<ScriptNode_> ParseVarConstFunc(TokIt_& cur, const TokIt_& end);
        static std::unique_ptr<ScriptNode_> ParseConst(TokIt_& cur);
        static std::unique_ptr<ScriptNode_> ParseVar(TokIt_& cur);
        static Vector_<std::unique_ptr<ScriptNode_>> ParseFuncArg(TokIt_& cur, const TokIt_& end);
    };
}
