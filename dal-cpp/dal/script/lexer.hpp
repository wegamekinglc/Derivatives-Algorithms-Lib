//
// Created by wegam on 2026/5/30.
//

#pragma once

#include <optional>
#include <variant>

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>

namespace Dal::Script {
    struct SourceLocation_ {
        size_t offset_ = 0;
        size_t line_ = 1;
        size_t column_ = 1;
        size_t row_ = 0;
        std::optional<Date_> eventDate_;

        [[nodiscard]] String_ Describe() const;
    };

    struct SourceOrigin_ {
        size_t offset_;
        size_t row_;
        Date_ eventDate_;
    };

    struct IndexLiteral_ {
        String_ raw_;
    };

    struct Token_ {
        SourceLocation_ source_;
        std::variant<String_, IndexLiteral_> value_;

        [[nodiscard]] const String_& Text() const {
            if (const auto* literal = std::get_if<IndexLiteral_>(&value_))
                return literal->raw_;
            return std::get<String_>(value_);
        }
    };

    Vector_<Token_> Lex(const String_& str, const Vector_<SourceOrigin_>& origins = {});
    Vector_<std::pair<size_t, size_t>> IndexLiteralRanges(const String_& str);
    // Shared tokenizer for the preprocessor and parser; see docs/methodology/script_engine.md §"Lexer".
    Vector_<String_> Tokenize(const String_& str);
} // namespace Dal::Script
