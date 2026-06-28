//
// Created by wegam on 2026/5/30.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal::Script {
    // Shared tokenizer for the preprocessor and parser; see docs/methodology/script_engine.md §"Lexer".
    Vector_<String_> Tokenize(const String_& str);
} // namespace Dal::Script
