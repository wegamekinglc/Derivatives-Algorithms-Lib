//
// Created by Codex on 2026/9/15.
//

#include <cstdint>

#include "__script_test_api.hpp"

namespace Dal::Excel {
    Vector_<String_> ScriptDiagnosticChunks(const String_& json, const String_& function) {
        String_ ascii;
        const auto encodingError = "DiagnosticEncoding: " + function + "; expected valid UTF-8 JSON";
        const auto escape = [&](uint32_t value) {
            static const char HEX[] = "0123456789abcdef";
            ascii += "\\u";
            for (int shift : {12, 8, 4, 0})
                ascii += HEX[(value >> shift) & 15];
        };
        for (size_t i = 0; i < json.size();) {
            const auto first = static_cast<unsigned char>(json[i++]);
            if (first < 128) {
                ascii += static_cast<char>(first);
                continue;
            }
            const int count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
            REQUIRE(count && i + count <= json.size(), encodingError);
            uint32_t code = first & ((1u << (6 - count)) - 1);
            for (int n = 0; n < count; ++n) {
                const auto next = static_cast<unsigned char>(json[i++]);
                REQUIRE((next & 0xc0) == 0x80, encodingError);
                code = (code << 6) | (next & 0x3f);
            }
            const uint32_t minimum = count == 1 ? 0x80 : count == 2 ? 0x800 : 0x10000;
            REQUIRE(code >= minimum && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff), encodingError);
            if (code <= 0xffff) {
                escape(code);
            } else {
                code -= 0x10000;
                escape(0xd800 + (code >> 10));
                escape(0xdc00 + (code & 0x3ff));
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
