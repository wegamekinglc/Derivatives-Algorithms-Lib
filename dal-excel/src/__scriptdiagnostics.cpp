//
// Created by Codex on 2026/9/15.
//

#include <cstdint>
#include <utility>

#include "__script_test_api.hpp"

namespace Dal::Excel {
    namespace {
        int Utf8ContinuationCount(unsigned char first) {
            if (first >= 0xc2 && first <= 0xdf)
                return 1;
            if (first >= 0xe0 && first <= 0xef)
                return 2;
            if (first >= 0xf0 && first <= 0xf4)
                return 3;
            return 0;
        }

        std::pair<uint32_t, size_t> DecodeUtf8(const String_& json, size_t offset, const String_& encodingError) {
            const auto first = static_cast<unsigned char>(json[offset]);
            const int count = Utf8ContinuationCount(first);
            REQUIRE(count && json.size() - offset - 1 >= static_cast<size_t>(count), encodingError);
            uint32_t code = first & ((1u << (6 - count)) - 1);
            for (int n = 1; n <= count; ++n) {
                const auto next = static_cast<unsigned char>(json[offset + n]);
                REQUIRE((next & 0xc0) == 0x80, encodingError);
                code = (code << 6) | (next & 0x3f);
            }
            static const uint32_t MINIMUM[] = {0, 0x80, 0x800, 0x10000};
            REQUIRE(code >= MINIMUM[count] && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff), encodingError);
            return {code, static_cast<size_t>(count + 1)};
        }

        String_ EscapeCodeUnit(uint32_t value) {
            static const char HEX[] = "0123456789abcdef";
            String_ escaped = "\\u";
            for (int shift : {12, 8, 4, 0})
                escaped += HEX[(value >> shift) & 15];
            return escaped;
        }

        String_ EscapeCodePoint(uint32_t code) {
            if (code <= 0xffff)
                return EscapeCodeUnit(code);
            const auto supplementary = code - 0x10000;
            return EscapeCodeUnit(0xd800 + (supplementary >> 10)) + EscapeCodeUnit(0xdc00 + (supplementary & 0x3ff));
        }
    } // namespace

    Vector_<String_> ScriptDiagnosticChunks(const String_& json, const String_& function) {
        String_ ascii;
        const auto encodingError = "DiagnosticEncoding: " + function + "; expected valid UTF-8 JSON";
        for (size_t i = 0; i < json.size();) {
            const auto first = static_cast<unsigned char>(json[i]);
            if (first < 128) {
                ascii += static_cast<char>(first);
                ++i;
            } else {
                const auto decoded = DecodeUtf8(json, i, encodingError);
                ascii += EscapeCodePoint(decoded.first);
                i += decoded.second;
            }
        }
        constexpr size_t CHUNK_SIZE = 30000, MAX_ROWS = 1048576;
        const auto rows = (ascii.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        REQUIRE(rows <= MAX_ROWS, "DiagnosticOutputTooLarge: " + function + "; rows=" + String_(std::to_string(rows)) + "; allowed rows=1048576");
        Vector_<String_> chunks;
        for (size_t offset = 0; offset < ascii.size(); offset += CHUNK_SIZE)
            chunks.push_back(ascii.substr(offset, CHUNK_SIZE));
        return chunks;
    }
} // namespace Dal::Excel
