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
        bool IsWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.'; }
        bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

        size_t WordEnd(const String_& str, size_t start) {
            while (start < str.size() && IsWord(str[start]))
                ++start;
            return start;
        }

        void AdvanceSource(const String_& str, size_t offset, SourceLocation_* source) {
            while (source->offset_ < offset) {
                if (str[source->offset_++] == '\n') {
                    ++source->line_;
                    source->column_ = 1;
                } else
                    ++source->column_;
            }
        }

        String_ IndexContext(const String_& str, const SourceLocation_& source) {
            return "; " + source.Describe() + "; input=" + str.substr(source.offset_);
        }

        size_t IndexBodyEnd(const String_& str, size_t start, const SourceLocation_& source) {
            const auto close = str.find(']', start);
            REQUIRE2(close != String_::npos, "InvalidIndex: missing closing ']'" + IndexContext(str, source), ScriptError_);
            REQUIRE2(str.find_first_of("[\"'", start) >= close, "InvalidIndex: malformed index literal" + IndexContext(str, source), ScriptError_);
            return close + 1;
        }

        size_t IndexSuffixEnd(const String_& str, size_t start, const SourceLocation_& source) {
            auto end = str.find_first_of(",)", start);
            if (end == String_::npos)
                end = str.size();
            REQUIRE2(str.find_first_of("[]\"'", start) >= end, "InvalidIndex: malformed index suffix" + IndexContext(str, source), ScriptError_);
            while (end > start && IsSpace(str[end - 1]))
                --end;
            return end;
        }

        size_t IndexLiteralEnd(const String_& str, size_t start, const SourceLocation_& source) {
            const auto open = WordEnd(str, start);
            if (open == start || open == str.size() || str[open] != '[')
                return start;
            return IndexSuffixEnd(str, IndexBodyEnd(str, open + 1, source), source);
        }

        void ApplyOrigins(const Vector_<SourceOrigin_>& origins, size_t pos, size_t* origin, SourceLocation_* source) {
            while (*origin < origins.size() && origins[*origin].offset_ <= pos) {
                source->row_ = origins[*origin].row_;
                source->eventDate_ = origins[*origin].eventDate_;
                ++*origin;
            }
        }

        size_t ScriptTokenEnd(const String_& str, size_t pos, const SourceLocation_& source) {
            if (IsWord(str[pos]))
                return WordEnd(str, pos);
            const size_t end = pos + 1;
            if (String_("!<>").find(str[pos]) != String_::npos && end < str.size() && str[end] == '=')
                return end + 1;
            REQUIRE2(String_("/-,;:()+*^<>=").find(str[pos]) != String_::npos,
                     "InvalidIndex: unexpected character '" + String_(1, str[pos]) + "'; " + source.Describe(), ScriptError_);
            return end;
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
        if (str.find('[') == String_::npos)
            return {};
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
            ApplyOrigins(origins, pos, &origin, &source);
            const auto literalEnd = IndexLiteralEnd(str, pos, source);
            if (literalEnd != pos) {
                result.push_back({source, IndexLiteral_{str.substr(pos, literalEnd - pos)}});
                pos = literalEnd;
                continue;
            }
            const auto end = ScriptTokenEnd(str, pos, source);
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
