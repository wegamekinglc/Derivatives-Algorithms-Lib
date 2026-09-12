//
// Created by wegam on 2026/5/30.
//

#include <cctype>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/lexer.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    namespace {
        bool IsWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }
        bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)); }

        void AdvanceSource(const String_& str, size_t offset, SourceLocation_* source) {
            while (source->offset_ < offset) {
                if (str[source->offset_++] == '\n') {
                    ++source->line_;
                    source->column_ = 1;
                } else
                    ++source->column_;
            }
        }

        size_t IndexLiteralEnd(const String_& str, size_t start, const SourceLocation_& source) {
            size_t cur = start;
            while (cur < str.size() && IsWord(str[cur]))
                ++cur;
            if (cur == start || cur == str.size() || str[cur] != '[')
                return start;
            const auto context = [&] { return "; " + source.Describe() + "; input=" + str.substr(start); };
            ++cur;
            while (cur < str.size() && str[cur] != ']') {
                REQUIRE2(str[cur] != '[' && str[cur] != '\'' && str[cur] != '"' && str[cur] != '(' && str[cur] != ')',
                         "InvalidIndex: malformed index literal" + context(), ScriptError_);
                ++cur;
            }
            REQUIRE2(cur != str.size(), "InvalidIndex: missing closing ']'" + context(), ScriptError_);
            ++cur;
            while (cur < str.size() && str[cur] != ',' && str[cur] != ')') {
                REQUIRE2(str[cur] != '[' && str[cur] != ']' && str[cur] != '\'' && str[cur] != '"',
                         "InvalidIndex: malformed index suffix" + context(), ScriptError_);
                ++cur;
            }
            while (cur > start && IsSpace(str[cur - 1]))
                --cur;
            return cur;
        }
    } // namespace

    String_ SourceLocation_::Describe() const {
        String_ result("line=" + std::to_string(line_) + ", column=" + std::to_string(column_) + ", offset=" + std::to_string(offset_));
        if (row_)
            result += String_(", row=" + std::to_string(row_));
        if (eventDate_)
            result += ", event=" + Date::ToString(*eventDate_);
        return result;
    }

    Vector_<std::pair<size_t, size_t>> IndexLiteralRanges(const String_& str) {
        Vector_<std::pair<size_t, size_t>> result;
        SourceLocation_ source;
        for (size_t pos = 0; pos < str.size();) {
            AdvanceSource(str, pos, &source);
            const auto end = IndexLiteralEnd(str, pos, source);
            if (end != pos) {
                result.emplace_back(pos, end);
                pos = end;
            } else if (IsWord(str[pos])) {
                do {
                    ++pos;
                } while (pos < str.size() && IsWord(str[pos]));
            } else
                ++pos;
        }
        return result;
    }

    Vector_<Token_> Lex(const String_& str, const Vector_<SourceOrigin_>& origins) {
        Vector_<Token_> result;
        SourceLocation_ source;
        size_t origin = 0;
        for (size_t pos = 0; pos < str.size();) {
            if (IsSpace(str[pos])) {
                ++pos;
                continue;
            }
            AdvanceSource(str, pos, &source);
            while (origin < origins.size() && origins[origin].offset_ <= pos) {
                source.row_ = origins[origin].row_;
                source.eventDate_ = origins[origin].eventDate_;
                ++origin;
            }
            const auto literalEnd = IndexLiteralEnd(str, pos, source);
            if (literalEnd != pos) {
                result.push_back({source, IndexLiteral_{str.substr(pos, literalEnd - pos)}});
                pos = literalEnd;
                continue;
            }
            size_t end = pos + 1;
            if (IsWord(str[pos])) {
                while (end < str.size() && IsWord(str[end]))
                    ++end;
            } else if ((str[pos] == '!' || str[pos] == '<' || str[pos] == '>') && end < str.size() && str[end] == '=') {
                ++end;
            } else {
                REQUIRE2(String_("/-,;:()+*^<>=").find(str[pos]) != String_::npos,
                         "InvalidIndex: unexpected character '" + String_(1, str[pos]) + "'; " + source.Describe(), ScriptError_);
            }
            result.push_back({source, String_(str.substr(pos, end - pos))});
            pos = end;
        }
        return result;
    }

    Vector_<String_> Tokenize(const String_& str) {
        Vector_<String_> result;
        for (const auto& token : Lex(str))
            result.push_back(token.Text());
        return result;
    }
} // namespace Dal::Script
