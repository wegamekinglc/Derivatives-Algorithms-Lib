//
// Created by wegam on 2026/5/30.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal::Script {
    // Shared lexer for the script engine.
    //
    // Tokenize is the single tokenization primitive used by both the definition
    // front-end (Preprocessor_) and the payoff back-end (Parser_), so it lives in
    // its own translation unit instead of being owned by either component.
    Vector_<String_> Tokenize(const String_& str);
}
