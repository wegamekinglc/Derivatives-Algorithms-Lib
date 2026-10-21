#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>

#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal {
#include <dal/auto/MG_CanonicalJsonKind_enum.inc>
} // namespace Dal

namespace Dal::CanonicalJson {
    namespace {
        bool IsContinuation(unsigned char value) { return value >= 0x80 && value <= 0xBF; }

        bool HasUtf8Bytes(const std::string& value, std::size_t index, std::size_t width) { return value.size() - index >= width; }

        bool ValidUtf8Two(const std::string& value, std::size_t index) {
            if (!HasUtf8Bytes(value, index, 2))
                return false;
            const auto first = static_cast<unsigned char>(value[index]);
            return first >= 0xC2 && IsContinuation(static_cast<unsigned char>(value[index + 1]));
        }

        bool ValidUtf8ThreeSecond(unsigned char first, unsigned char second) {
            if (first == 0xE0)
                return second >= 0xA0 && second <= 0xBF;
            if (first == 0xED)
                return second >= 0x80 && second <= 0x9F;
            return IsContinuation(second);
        }

        bool ValidUtf8Three(const std::string& value, std::size_t index) {
            if (!HasUtf8Bytes(value, index, 3))
                return false;
            const auto first = static_cast<unsigned char>(value[index]);
            const auto second = static_cast<unsigned char>(value[index + 1]);
            if (!ValidUtf8ThreeSecond(first, second))
                return false;
            return IsContinuation(static_cast<unsigned char>(value[index + 2]));
        }

        bool ValidUtf8FourSecond(unsigned char first, unsigned char second) {
            if (first == 0xF0)
                return second >= 0x90 && second <= 0xBF;
            if (first == 0xF4)
                return second >= 0x80 && second <= 0x8F;
            return IsContinuation(second);
        }

        bool ValidUtf8Four(const std::string& value, std::size_t index) {
            if (!HasUtf8Bytes(value, index, 4))
                return false;
            const auto first = static_cast<unsigned char>(value[index]);
            const auto second = static_cast<unsigned char>(value[index + 1]);
            if (!ValidUtf8FourSecond(first, second))
                return false;
            if (!IsContinuation(static_cast<unsigned char>(value[index + 2])))
                return false;
            return IsContinuation(static_cast<unsigned char>(value[index + 3]));
        }

        std::size_t Utf8MultibyteWidth(const std::string& value, std::size_t index) {
            const auto first = static_cast<unsigned char>(value[index]);
            if (first <= 0xDF)
                return ValidUtf8Two(value, index) ? 2 : 0;
            if (first <= 0xEF)
                return ValidUtf8Three(value, index) ? 3 : 0;
            if (first <= 0xF4)
                return ValidUtf8Four(value, index) ? 4 : 0;
            return 0;
        }

        bool ValidUtf8Impl(const std::string& value) {
            std::size_t index = 0;
            while (index < value.size()) {
                const auto first = static_cast<unsigned char>(value[index]);
                const std::size_t width = first <= 0x7F ? 1 : Utf8MultibyteWidth(value, index);
                if (width == 0)
                    return false;
                index += width;
            }
            return true;
        }

        std::vector<std::uint16_t> Utf16Units(const std::string& value) {
            REQUIRE(ValidUtf8Impl(value), "QUOTE_RISK_JCS_INVALID_UTF8");
            std::vector<std::uint16_t> result;
            for (std::size_t i = 0; i < value.size();) {
                const auto first = static_cast<unsigned char>(value[i]);
                std::uint32_t codePoint = 0;
                std::size_t width = 1;
                if (first <= 0x7F) {
                    codePoint = first;
                } else if (first <= 0xDF) {
                    width = 2;
                    codePoint = ((first & 0x1FU) << 6U) | (static_cast<unsigned char>(value[i + 1]) & 0x3FU);
                } else if (first <= 0xEF) {
                    width = 3;
                    codePoint = ((first & 0x0FU) << 12U) | ((static_cast<unsigned char>(value[i + 1]) & 0x3FU) << 6U) |
                                (static_cast<unsigned char>(value[i + 2]) & 0x3FU);
                } else {
                    width = 4;
                    codePoint = ((first & 0x07U) << 18U) | ((static_cast<unsigned char>(value[i + 1]) & 0x3FU) << 12U) |
                                ((static_cast<unsigned char>(value[i + 2]) & 0x3FU) << 6U) | (static_cast<unsigned char>(value[i + 3]) & 0x3FU);
                }
                if (codePoint <= 0xFFFFU) {
                    result.push_back(static_cast<std::uint16_t>(codePoint));
                } else {
                    codePoint -= 0x10000U;
                    result.push_back(static_cast<std::uint16_t>(0xD800U + (codePoint >> 10U)));
                    result.push_back(static_cast<std::uint16_t>(0xDC00U + (codePoint & 0x3FFU)));
                }
                i += width;
            }
            return result;
        }

        bool JcsKeyLess(const std::string& lhs, const std::string& rhs) {
            const auto left = Utf16Units(lhs);
            const auto right = Utf16Units(rhs);
            return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
        }

        std::string ExtractJcsNumberSign(std::string* digits) {
            if (digits->empty())
                return std::string();
            if (digits->front() != '-')
                return std::string();
            digits->erase(digits->begin());
            return "-";
        }

        int ParseJcsExponent(std::string exponentText) {
            REQUIRE(!exponentText.empty(), "QUOTE_RISK_JCS_NUMBER_FORMAT_FAILED");
            int sign = 1;
            if (exponentText.front() == '+' || exponentText.front() == '-') {
                sign = exponentText.front() == '-' ? -1 : 1;
                exponentText.erase(exponentText.begin());
            }
            int exponent = 0;
            const auto parsed = std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);
            REQUIRE(!exponentText.empty(), "QUOTE_RISK_JCS_NUMBER_FORMAT_FAILED");
            REQUIRE(parsed.ec == std::errc(), "QUOTE_RISK_JCS_NUMBER_FORMAT_FAILED");
            REQUIRE(parsed.ptr == exponentText.data() + exponentText.size(), "QUOTE_RISK_JCS_NUMBER_FORMAT_FAILED");
            return sign * exponent;
        }

        int ExtractJcsExponent(std::string* digits) {
            const auto exponentAt = digits->find_first_of("eE");
            if (exponentAt == std::string::npos)
                return 0;
            const int exponent = ParseJcsExponent(digits->substr(exponentAt + 1));
            digits->erase(exponentAt);
            return exponent;
        }

        int RemoveJcsDecimalPoint(std::string* digits) {
            const auto decimalAt = digits->find('.');
            const int integerDigits = decimalAt == std::string::npos ? static_cast<int>(digits->size()) : static_cast<int>(decimalAt);
            if (decimalAt != std::string::npos)
                digits->erase(decimalAt, 1);
            return integerDigits;
        }

        std::string JcsPlainNumber(const std::string& digits, int decimalExponent) {
            if (decimalExponent <= 0)
                return "0." + std::string(static_cast<std::size_t>(-decimalExponent), '0') + digits;
            if (decimalExponent >= static_cast<int>(digits.size()))
                return digits + std::string(static_cast<std::size_t>(decimalExponent - static_cast<int>(digits.size())), '0');
            return digits.substr(0, static_cast<std::size_t>(decimalExponent)) + "." + digits.substr(static_cast<std::size_t>(decimalExponent));
        }

        std::string JcsScientificNumber(const std::string& digits, int scientificExponent) {
            std::string result(1, digits.front());
            if (digits.size() > 1)
                result += "." + digits.substr(1);
            result += scientificExponent >= 0 ? "e+" : "e-";
            return result + std::to_string(std::abs(scientificExponent));
        }

        std::string JcsNumberImpl(double value) {
            REQUIRE(std::isfinite(value), "QUOTE_RISK_NON_FINITE_FINGERPRINT_INPUT");
            if (value == 0.0)
                return "0";

            char buffer[64];
            const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
            REQUIRE(converted.ec == std::errc(), "QUOTE_RISK_JCS_NUMBER_FORMAT_FAILED");
            std::string digits(buffer, converted.ptr);
            const std::string sign = ExtractJcsNumberSign(&digits);
            const int exponent = ExtractJcsExponent(&digits);
            const int decimalExponent = RemoveJcsDecimalPoint(&digits) + exponent;
            const int scientificExponent = decimalExponent - 1;
            const bool plain = scientificExponent >= -6 && scientificExponent < 21;
            return sign + (plain ? JcsPlainNumber(digits, decimalExponent) : JcsScientificNumber(digits, scientificExponent));
        }

        const char* JcsShortEscape(unsigned char byte) {
            static const std::array<std::pair<unsigned char, const char*>, 7> ESCAPES = {
                std::pair<unsigned char, const char*>{'"', "\\\""},
                {'\\', "\\\\"},
                {'\b', "\\b"},
                {'\t', "\\t"},
                {'\n', "\\n"},
                {'\f', "\\f"},
                {'\r', "\\r"},
            };
            for (const auto& escape : ESCAPES)
                if (escape.first == byte)
                    return escape.second;
            return nullptr;
        }

        void AppendJcsByte(char raw, std::string* output) {
            static constexpr char HEX[] = "0123456789abcdef";
            const auto byte = static_cast<unsigned char>(raw);
            if (const char* escape = JcsShortEscape(byte)) {
                *output += escape;
                return;
            }
            if (byte >= 0x20) {
                output->push_back(raw);
                return;
            }
            *output += "\\u00";
            output->push_back(HEX[(byte >> 4U) & 0x0FU]);
            output->push_back(HEX[byte & 0x0FU]);
        }

        void AppendJcsString(const std::string& value, std::string* output) {
            REQUIRE(ValidUtf8Impl(value), "QUOTE_RISK_JCS_INVALID_UTF8");
            output->push_back('"');
            for (const char raw : value)
                AppendJcsByte(raw, output);
            output->push_back('"');
        }

        void AppendJcs(const Json_& value, std::string* output);

        void AppendJcsArray(const Json_& value, std::string* output) {
            output->push_back('[');
            for (std::size_t i = 0; i < value.array_.size(); ++i) {
                if (i)
                    output->push_back(',');
                AppendJcs(value.array_[i], output);
            }
            output->push_back(']');
        }

        void AppendJcsObject(const Json_& value, std::string* output) {
            output->push_back('{');
            std::vector<const std::pair<const std::string, Json_>*> members;
            members.reserve(value.object_.size());
            for (const auto& member : value.object_)
                members.push_back(&member);
            std::sort(members.begin(), members.end(), [](const auto* lhs, const auto* rhs) { return JcsKeyLess(lhs->first, rhs->first); });
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (i)
                    output->push_back(',');
                AppendJcsString(members[i]->first, output);
                output->push_back(':');
                AppendJcs(members[i]->second, output);
            }
            output->push_back('}');
        }

        void AppendJcs(const Json_& value, std::string* output) {
            switch (value.kind_.Switch()) {
            case Json_::Kind_::Value_::NIL:
                *output += "null";
                break;
            case Json_::Kind_::Value_::BOOLEAN:
                *output += value.boolean_ ? "true" : "false";
                break;
            case Json_::Kind_::Value_::NUMBER:
                *output += JcsNumberImpl(value.number_);
                break;
            case Json_::Kind_::Value_::STRING:
                AppendJcsString(value.string_, output);
                break;
            case Json_::Kind_::Value_::ARRAY:
                AppendJcsArray(value, output);
                break;
            case Json_::Kind_::Value_::OBJECT:
                AppendJcsObject(value, output);
                break;
            }
        }

        std::string JcsImpl(const Json_& value) {
            std::string result;
            AppendJcs(value, &result);
            return result;
        }

        constexpr std::array<std::uint32_t, 64> SHA256_K = {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
            0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
            0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
            0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        };

        std::uint32_t RotateRight(std::uint32_t value, int bits) { return (value >> bits) | (value << (32 - bits)); }

        std::array<unsigned char, 32> Sha256(const std::string& value) {
            std::vector<unsigned char> bytes(value.begin(), value.end());
            const std::uint64_t bitLength = static_cast<std::uint64_t>(bytes.size()) * 8U;
            bytes.push_back(0x80U);
            while (bytes.size() % 64U != 56U)
                bytes.push_back(0U);
            for (int shift = 56; shift >= 0; shift -= 8)
                bytes.push_back(static_cast<unsigned char>((bitLength >> shift) & 0xFFU));

            std::array<std::uint32_t, 8> state = {
                0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
            };
            for (std::size_t offset = 0; offset < bytes.size(); offset += 64U) {
                std::array<std::uint32_t, 64> words{};
                for (int i = 0; i < 16; ++i) {
                    const std::size_t at = offset + static_cast<std::size_t>(4 * i);
                    words[i] = (static_cast<std::uint32_t>(bytes[at]) << 24U) | (static_cast<std::uint32_t>(bytes[at + 1]) << 16U) |
                               (static_cast<std::uint32_t>(bytes[at + 2]) << 8U) | static_cast<std::uint32_t>(bytes[at + 3]);
                }
                for (int i = 16; i < 64; ++i) {
                    const std::uint32_t s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3U);
                    const std::uint32_t s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10U);
                    words[i] = words[i - 16] + s0 + words[i - 7] + s1;
                }

                std::uint32_t a = state[0];
                std::uint32_t b = state[1];
                std::uint32_t c = state[2];
                std::uint32_t d = state[3];
                std::uint32_t e = state[4];
                std::uint32_t f = state[5];
                std::uint32_t g = state[6];
                std::uint32_t h = state[7];
                for (int i = 0; i < 64; ++i) {
                    const std::uint32_t upperSigma1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
                    const std::uint32_t choose = (e & f) ^ (~e & g);
                    const std::uint32_t temp1 = h + upperSigma1 + choose + SHA256_K[i] + words[i];
                    const std::uint32_t upperSigma0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temp2 = upperSigma0 + majority;
                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }
                state[0] += a;
                state[1] += b;
                state[2] += c;
                state[3] += d;
                state[4] += e;
                state[5] += f;
                state[6] += g;
                state[7] += h;
            }

            std::array<unsigned char, 32> result{};
            for (int i = 0; i < 8; ++i) {
                result[4 * i] = static_cast<unsigned char>(state[i] >> 24U);
                result[4 * i + 1] = static_cast<unsigned char>(state[i] >> 16U);
                result[4 * i + 2] = static_cast<unsigned char>(state[i] >> 8U);
                result[4 * i + 3] = static_cast<unsigned char>(state[i]);
            }
            return result;
        }

        String_ FingerprintCanonicalImpl(const std::string& canonical) {
            static constexpr char HEX[] = "0123456789abcdef";
            const auto digest = Sha256(canonical);
            std::string result = "sha256:";
            result.reserve(71);
            for (const unsigned char byte : digest) {
                result.push_back(HEX[(byte >> 4U) & 0x0FU]);
                result.push_back(HEX[byte & 0x0FU]);
            }
            return String_(result);
        }

        String_ FingerprintImpl(const Json_& value) { return FingerprintCanonicalImpl(JcsImpl(value)); }

        Json_ FromRapidJsonImpl(const rapidjson::Value& value) {
            if (value.IsNull())
                return Json_();
            if (value.IsBool())
                return Json_::Boolean(value.GetBool());
            if (value.IsNumber())
                return Json_::Number(value.GetDouble());
            if (value.IsString())
                return Json_::String(String_(value.GetString(), value.GetString() + value.GetStringLength()));
            if (value.IsArray()) {
                Json_ result = Json_::Array();
                for (const auto& child : value.GetArray())
                    result.array_.push_back(FromRapidJsonImpl(child));
                return result;
            }
            REQUIRE(value.IsObject(), "QUOTE_RISK_JCS_UNSUPPORTED_VALUE");
            Json_ result = Json_::Object();
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                const std::string name(member->name.GetString(), member->name.GetString() + member->name.GetStringLength());
                REQUIRE(result.object_.emplace(name, FromRapidJsonImpl(member->value)).second, "QUOTE_RISK_JCS_DUPLICATE_KEY");
            }
            return result;
        }

    } // namespace

    bool ValidUtf8(const std::string& value) { return ValidUtf8Impl(value); }
    std::string JcsNumber(double value) { return JcsNumberImpl(value); }
    std::string Jcs(const Json_& value) { return JcsImpl(value); }
    String_ FingerprintCanonical(const std::string& value) { return FingerprintCanonicalImpl(value); }
    String_ Fingerprint(const Json_& value) { return FingerprintImpl(value); }
    Json_ FromRapidJson(const rapidjson::Value& value) { return FromRapidJsonImpl(value); }
} // namespace Dal::CanonicalJson
