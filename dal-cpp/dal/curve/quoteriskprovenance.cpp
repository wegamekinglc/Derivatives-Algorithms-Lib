//
// Created by dal-implementer on 2026/8/31.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <rapidjson/document.h>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        struct Json_ {
            enum class Kind_ { NIL, BOOLEAN, NUMBER, STRING, ARRAY, OBJECT };

            Kind_ kind_ = Kind_::NIL;
            bool boolean_ = false;
            double number_ = 0.0;
            std::string string_;
            std::vector<Json_> array_;
            std::map<std::string, Json_> object_;

            static Json_ Boolean(bool value) {
                Json_ result;
                result.kind_ = Kind_::BOOLEAN;
                result.boolean_ = value;
                return result;
            }

            static Json_ Number(double value) {
                REQUIRE(std::isfinite(value), "QUOTE_RISK_NON_FINITE_FINGERPRINT_INPUT");
                Json_ result;
                result.kind_ = Kind_::NUMBER;
                result.number_ = value;
                return result;
            }

            static Json_ String(const String_& value) {
                Json_ result;
                result.kind_ = Kind_::STRING;
                result.string_.assign(value.data(), value.size());
                return result;
            }

            static Json_ String(const char* value) { return String(String_(value ? value : "")); }

            static Json_ Array() {
                Json_ result;
                result.kind_ = Kind_::ARRAY;
                return result;
            }

            static Json_ Object() {
                Json_ result;
                result.kind_ = Kind_::OBJECT;
                return result;
            }
        };

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

        bool ValidUtf8(const std::string& value) {
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
            REQUIRE(ValidUtf8(value), "QUOTE_RISK_JCS_INVALID_UTF8");
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

        std::string JcsNumber(double value) {
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
            REQUIRE(ValidUtf8(value), "QUOTE_RISK_JCS_INVALID_UTF8");
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
            switch (value.kind_) {
            case Json_::Kind_::NIL:
                *output += "null";
                break;
            case Json_::Kind_::BOOLEAN:
                *output += value.boolean_ ? "true" : "false";
                break;
            case Json_::Kind_::NUMBER:
                *output += JcsNumber(value.number_);
                break;
            case Json_::Kind_::STRING:
                AppendJcsString(value.string_, output);
                break;
            case Json_::Kind_::ARRAY:
                AppendJcsArray(value, output);
                break;
            case Json_::Kind_::OBJECT:
                AppendJcsObject(value, output);
                break;
            }
        }

        std::string Jcs(const Json_& value) {
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

        String_ Fingerprint(const Json_& value) {
            static constexpr char HEX[] = "0123456789abcdef";
            const auto digest = Sha256(Jcs(value));
            std::string result = "sha256:";
            result.reserve(71);
            for (const unsigned char byte : digest) {
                result.push_back(HEX[(byte >> 4U) & 0x0FU]);
                result.push_back(HEX[byte & 0x0FU]);
            }
            return String_(result);
        }

        Json_ FromRapidJson(const rapidjson::Value& value) {
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
                    result.array_.push_back(FromRapidJson(child));
                return result;
            }
            REQUIRE(value.IsObject(), "QUOTE_RISK_JCS_UNSUPPORTED_VALUE");
            Json_ result = Json_::Object();
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                const std::string name(member->name.GetString(), member->name.GetString() + member->name.GetStringLength());
                REQUIRE(result.object_.emplace(name, FromRapidJson(member->value)).second, "QUOTE_RISK_JCS_DUPLICATE_KEY");
            }
            return result;
        }

        std::string ArchiveTag(const rapidjson::Value& value) {
            const auto tag = value.FindMember("$tag");
            if (tag == value.MemberEnd())
                return std::string();
            if (!tag->value.IsString())
                return std::string();
            return std::string(tag->value.GetString(), tag->value.GetString() + tag->value.GetStringLength());
        }

        bool RecordArchiveTag(const std::string& ownTag,
                              const std::string& parentTag,
                              bool definition,
                              std::map<std::string, std::set<std::string>>* edges,
                              std::set<std::string>* definitions) {
            if (ownTag.empty())
                return false;
            if (!parentTag.empty())
                (*edges)[parentTag].insert(ownTag);
            if (!definition)
                return true;
            definitions->insert(ownTag);
            return false;
        }

        void CollectArchiveGraph(const rapidjson::Value& value,
                                 const std::string& parentTag,
                                 std::map<std::string, std::set<std::string>>* edges,
                                 std::set<std::string>* definitions);

        void CollectArchiveArray(const rapidjson::Value& value,
                                 const std::string& parentTag,
                                 std::map<std::string, std::set<std::string>>* edges,
                                 std::set<std::string>* definitions) {
            for (const auto& child : value.GetArray())
                CollectArchiveGraph(child, parentTag, edges, definitions);
        }

        void CollectArchiveObject(const rapidjson::Value& value,
                                  const std::string& parentTag,
                                  std::map<std::string, std::set<std::string>>* edges,
                                  std::set<std::string>* definitions) {
            const std::string ownTag = ArchiveTag(value);
            const bool definition = !ownTag.empty() && value.MemberCount() > 1;
            if (RecordArchiveTag(ownTag, parentTag, definition, edges, definitions))
                return;

            const std::string current = definition ? ownTag : parentTag;
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                if (std::strcmp(member->name.GetString(), "$tag") != 0)
                    CollectArchiveGraph(member->value, current, edges, definitions);
            }
        }

        void CollectArchiveGraph(const rapidjson::Value& value,
                                 const std::string& parentTag,
                                 std::map<std::string, std::set<std::string>>* edges,
                                 std::set<std::string>* definitions) {
            if (value.IsArray()) {
                CollectArchiveArray(value, parentTag, edges, definitions);
                return;
            }
            if (value.IsObject())
                CollectArchiveObject(value, parentTag, edges, definitions);
        }

        bool HasArchiveCycle(const std::string& tag,
                             const std::map<std::string, std::set<std::string>>& edges,
                             std::set<std::string>* active,
                             std::set<std::string>* complete) {
            if (active->find(tag) != active->end())
                return true;
            if (complete->find(tag) != complete->end())
                return false;
            active->insert(tag);
            const auto children = edges.find(tag);
            if (children != edges.end()) {
                for (const auto& child : children->second)
                    if (HasArchiveCycle(child, edges, active, complete))
                        return true;
            }
            active->erase(tag);
            complete->insert(tag);
            return false;
        }

        Json_ StorableJson(const Storable_& value) {
            const String_ archive = JSON::WriteString(value);
            rapidjson::Document document;
            document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseFullPrecisionFlag>(archive.data(), archive.size());
            REQUIRE(!document.HasParseError(), "QUOTE_RISK_ARCHIVE_CANONICALIZATION_FAILED");
            std::map<std::string, std::set<std::string>> edges;
            std::set<std::string> definitions;
            CollectArchiveGraph(document, std::string(), &edges, &definitions);
            std::set<std::string> active;
            std::set<std::string> complete;
            for (const auto& tag : definitions)
                REQUIRE(!HasArchiveCycle(tag, edges, &active, &complete), "QUOTE_RISK_CYCLIC_BASE_GRAPH");
            return FromRapidJson(document);
        }

        Json_ DateJson(const Date_& value) { return Json_::String(Date::ToString(value)); }
        Json_ DateTimeJson(const DateTime_& value) { return Json_::String(DateTime::ToString(value)); }

        Json_ DoubleVectorJson(const Vector_<>& values) {
            Json_ result = Json_::Array();
            for (const double value : values)
                result.array_.push_back(Json_::Number(value));
            return result;
        }

        Json_ DateVectorJson(const Vector_<Date_>& values) {
            Json_ result = Json_::Array();
            for (const auto& value : values)
                result.array_.push_back(DateJson(value));
            return result;
        }

        Json_ StringVectorJson(const Vector_<String_>& values) {
            Json_ result = Json_::Array();
            for (const auto& value : values)
                result.array_.push_back(Json_::String(value));
            return result;
        }

        Json_ MatrixJson(const Matrix_<>& values) {
            Json_ result = Json_::Array();
            for (int row = 0; row < values.Rows(); ++row) {
                Json_ rowValue = Json_::Array();
                for (int column = 0; column < values.Cols(); ++column)
                    rowValue.array_.push_back(Json_::Number(values(row, column)));
                result.array_.push_back(std::move(rowValue));
            }
            return result;
        }

        Json_ RateIndexJson(const RateIndexConvention_& value) {
            Json_ result = Json_::Object();
            result.object_["accrualHolidays"] = Json_::String(value.accrualHolidays_.String());
            result.object_["businessDayConvention"] = Json_::String(value.businessDayConvention_.String());
            result.object_["collateral"] = Json_::String(value.collateral_.String());
            result.object_["dayBasis"] = Json_::String(value.dayBasis_.String());
            result.object_["endOfMonth"] = Json_::Boolean(value.endOfMonth_);
            result.object_["fixingHolidays"] = Json_::String(value.fixingHolidays_.String());
            result.object_["fixingLag"] = Json_::Number(value.fixingLag_);
            result.object_["forecastTenor"] = Json_::String(value.forecastTenor_.String());
            result.object_["spotLag"] = Json_::Number(value.spotLag_);
            result.object_["useProjectionCurve"] = Json_::Boolean(value.useProjectionCurve_);
            return result;
        }

        Json_ RateLegJson(const RateLegConvention_& value) {
            Json_ result = Json_::Object();
            result.object_["accrualHolidays"] = Json_::String(value.accrualHolidays_.String());
            result.object_["businessDayConvention"] = Json_::String(value.businessDayConvention_.String());
            result.object_["dayBasis"] = Json_::String(value.dayBasis_.String());
            result.object_["endOfMonth"] = Json_::Boolean(value.endOfMonth_);
            result.object_["paymentConvention"] = Json_::String(value.paymentConvention_.String());
            result.object_["paymentFrequency"] = Json_::String(value.paymentFrequency_.String());
            result.object_["paymentHolidays"] = Json_::String(value.paymentHolidays_.String());
            result.object_["paymentLag"] = Json_::Number(value.paymentLag_);
            return result;
        }

        Json_ CalibrationInstrumentJson(const YCInstrument_& value) {
            Json_ result = Json_::Object();
            result.object_["displayName"] = Json_::String(value.Name());
            result.object_["marketRate"] = Json_::Number(value.MarketRate());
            result.object_["tradeDate"] = DateJson(value.TradeDate());
            const auto span = value.TimeSpan();
            result.object_["startDate"] = DateJson(span.first);
            result.object_["maturityDate"] = DateJson(span.second);

            if (const auto* future = dynamic_cast<const Future_*>(&value)) {
                result.object_["convexityAdjustment"] = Json_::Number(future->ConvexityAdjustment());
                result.object_["index"] = RateIndexJson(future->FloatConvention());
            } else if (const auto* deposit = dynamic_cast<const Deposit_*>(&value)) {
                result.object_["index"] = RateIndexJson(deposit->FloatConvention());
            } else if (const auto* fra = dynamic_cast<const FRA_*>(&value)) {
                result.object_["index"] = RateIndexJson(fra->FloatConvention());
            } else if (const auto* swap = dynamic_cast<const Swap_*>(&value)) {
                result.object_["fixedLeg"] = RateLegJson(swap->FixedLegConvention());
                result.object_["floatIndex"] = RateIndexJson(swap->FloatConvention());
                result.object_["floatLeg"] = RateLegJson(swap->FloatLegConvention());
            } else if (const auto* basis = dynamic_cast<const BasisSwap_*>(&value)) {
                result.object_["referenceIndex"] = RateIndexJson(basis->ReferenceIndexConvention());
                result.object_["referenceLeg"] = RateLegJson(basis->ReferenceLegConvention());
                result.object_["spreadIndex"] = RateIndexJson(basis->SpreadIndexConvention());
                result.object_["spreadLeg"] = RateLegJson(basis->SpreadLegConvention());
            } else {
                REQUIRE(false, "QUOTE_RISK_UNSUPPORTED_CALIBRATION_INSTRUMENT");
            }
            return result;
        }

        Json_ CalibrationInstrumentsJson(const Vector_<Handle_<YCInstrument_>>& values) {
            Json_ result = Json_::Array();
            for (int i = 0; i < static_cast<int>(values.size()); ++i) {
                REQUIRE(values[i], "QUOTE_RISK_EMPTY_CALIBRATION_INSTRUMENT");
                Json_ instrument = CalibrationInstrumentJson(*values[i]);
                instrument.object_["ordinal"] = Json_::Number(i);
                result.array_.push_back(std::move(instrument));
            }
            return result;
        }

        Json_ FixingsJson(const Handle_<MarketFixingSnapshot_>& fixings) {
            Json_ result = Json_::Array();
            if (!fixings)
                return result;
            for (const auto& history : fixings->Values()) {
                for (const auto& fixing : history.second) {
                    Json_ entry = Json_::Object();
                    entry.object_["fixingTime"] = DateTimeJson(fixing.first);
                    entry.object_["indexName"] = Json_::String(history.first);
                    entry.object_["value"] = Json_::Number(fixing.second);
                    result.array_.push_back(std::move(entry));
                }
            }
            return result;
        }

        Json_ CurveBlockJson(const CurveBlock_& block) {
            Json_ result = Json_::Object();
            Json_ discounts = Json_::Object();
            for (const auto& curve : block.DiscountCurves()) {
                REQUIRE(curve.second, "QUOTE_RISK_CURVE_BLOCK_CURVE_EMPTY");
                discounts.object_[std::string(curve.first.String())] = StorableJson(*curve.second);
            }
            if (discounts.object_.empty())
                discounts.object_["ROUTED_DEFAULT"] = StorableJson(block.Discount(CollateralType_(CollateralType_::Value_::OIS)));
            result.object_["discountCurves"] = std::move(discounts);
            Json_ forwards = Json_::Object();
            for (const auto& curve : block.ForwardCurves()) {
                REQUIRE(curve.second, "QUOTE_RISK_CURVE_BLOCK_CURVE_EMPTY");
                forwards.object_[std::string(curve.first.String())] = StorableJson(*curve.second);
            }
            result.object_["forwardCurves"] = std::move(forwards);
            result.object_["liborBasis"] = Json_::String(block.LiborBasis().String());
            return result;
        }

        Json_ XccyMarketJson(const RatePricingMarket_& market) {
            if (market.xccyMarket_) {
                Json_ xccy = Json_::Object();
                xccy.object_["basisCurve"] = market.xccyMarket_->BasisCurve() ? StorableJson(*market.xccyMarket_->BasisCurve()) : Json_();
                xccy.object_["collateralCurrency"] = Json_::String(market.xccyMarket_->CollateralCurrency().String());
                xccy.object_["domesticBlock"] = CurveBlockJson(market.xccyMarket_->DomesticBlock());
                xccy.object_["domesticCurrency"] = Json_::String(market.xccyMarket_->DomesticCcy().String());
                xccy.object_["fixings"] = FixingsJson(market.xccyMarket_->Fixings());
                xccy.object_["foreignBlock"] = CurveBlockJson(market.xccyMarket_->ForeignBlock());
                xccy.object_["foreignCurrency"] = Json_::String(market.xccyMarket_->ForeignCcy().String());
                xccy.object_["fxSpot"] = Json_::Number(market.xccyMarket_->FxSpot());
                xccy.object_["valuationTime"] = DateTimeJson(market.xccyMarket_->ValuationTime());
                return xccy;
            }
            return Json_();
        }

        Json_ MarketJson(const RatePricingMarket_& market, const std::map<String_, String_>& bindings) {
            REQUIRE(market.valuationTime_.IsValid(), "QUOTE_RISK_BOUND_MARKET_VALUATION_TIME_INVALID");
            Json_ result = Json_::Object();
            Json_ components = Json_::Object();
            for (const auto& binding : bindings) {
                const auto component = market.curveComponents_.find(binding.second);
                REQUIRE(component != market.curveComponents_.end() && component->second, "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
                components.object_[std::string(component->first.data(), component->first.size())] = StorableJson(*component->second);
            }
            result.object_["curveComponents"] = std::move(components);
            result.object_["fixings"] = FixingsJson(market.fixings_);
            result.object_["resultCurrency"] = Json_::String(market.resultCurrency_.String());
            result.object_["valuationTime"] = DateTimeJson(market.valuationTime_);
            result.object_["xccy"] = XccyMarketJson(market);
            return result;
        }

        Json_ SingleSpecJson(const CurveCalibrationSpec_& spec) {
            Json_ result = Json_::Object();
            result.object_["baseCurve"] = spec.baseCurve_ ? StorableJson(*spec.baseCurve_) : Json_();
            result.object_["calibrateDiscountCurve"] = Json_::Boolean(spec.calibrateDiscountCurve_);
            result.object_["currency"] = Json_::String(spec.ccy_);
            result.object_["curveName"] = Json_::String(spec.curveName_);
            Json_ discountCurves = Json_::Object();
            for (const auto& curve : spec.discountCurves_) {
                REQUIRE(curve.second, "QUOTE_RISK_SPEC_CURVE_EMPTY");
                discountCurves.object_[std::string(curve.first.String())] = StorableJson(*curve.second);
            }
            result.object_["discountCurves"] = std::move(discountCurves);
            result.object_["fitTolerance"] = Json_::Number(spec.fitTolerance_);
            Json_ forwardCurves = Json_::Object();
            for (const auto& curve : spec.forwardCurves_) {
                REQUIRE(curve.second, "QUOTE_RISK_SPEC_CURVE_EMPTY");
                forwardCurves.object_[std::string(curve.first.String())] = StorableJson(*curve.second);
            }
            result.object_["forwardCurves"] = std::move(forwardCurves);
            result.object_["initialGuess"] = Json_::Number(spec.initialGuess_);
            result.object_["initialGuessPerNode"] = DoubleVectorJson(spec.initialGuessPerNode_);
            result.object_["instruments"] = CalibrationInstrumentsJson(spec.instruments_);
            result.object_["knotDates"] = DateVectorJson(spec.knotDates_);
            result.object_["knotPolicy"] = Json_::String(spec.knotPolicy_.String());
            result.object_["liborBasis"] = Json_::String(spec.liborBasis_.String());
            result.object_["logDfScheme"] = Json_::String(spec.logDfScheme_.String());
            result.object_["maxEvaluations"] = Json_::Number(spec.maxEvaluations_);
            result.object_["maxRestarts"] = Json_::Number(spec.maxRestarts_);
            result.object_["parameterization"] = Json_::String(spec.parameterization_.String());
            result.object_["smoothingWeight"] = Json_::Number(spec.smoothingWeight_);
            result.object_["solveMode"] = Json_::String(spec.solveMode_.String());
            result.object_["targetCollateral"] = Json_::String(spec.targetCollateral_.String());
            result.object_["targetTenor"] = Json_::String(spec.targetTenor_.String());
            result.object_["today"] = DateJson(spec.today_);
            result.object_["tolerance"] = Json_::Number(spec.tolerance_);
            return result;
        }

        Json_ SingleOptionsJson(const CurveCalibrationOptions_& options) {
            Json_ result = Json_::Object();
            result.object_["computeEffJacobianInverse"] = Json_::Boolean(options.computeEffJacobianInverse_);
            result.object_["computeForwardJacobian"] = Json_::Boolean(options.computeForwardJacobian_);
            result.object_["jacobianMode"] = Json_::String(options.jacobianMode_.String());
            return result;
        }

        Json_ SingleResultJson(const CurveCalibrationResult_& result) {
            REQUIRE(result.curve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            Json_ value = Json_::Object();
            value.object_["curve"] = StorableJson(*result.curve_);
            Json_ diagnostics = Json_::Object();
            diagnostics.object_["curveName"] = Json_::String(result.diagnostics_.curveName_);
            diagnostics.object_["effectiveInverse"] = MatrixJson(result.diagnostics_.effJacobianInverse_);
            diagnostics.object_["instrumentNames"] = StringVectorJson(result.diagnostics_.instrumentNames_);
            diagnostics.object_["marketRates"] = DoubleVectorJson(result.diagnostics_.marketRates_);
            diagnostics.object_["maxAbsResidual"] = Json_::Number(result.diagnostics_.maxAbsResidual_);
            diagnostics.object_["modelRates"] = DoubleVectorJson(result.diagnostics_.modelRates_);
            diagnostics.object_["residuals"] = DoubleVectorJson(result.diagnostics_.residuals_);
            diagnostics.object_["rmsResidual"] = Json_::Number(result.diagnostics_.rmsResidual_);
            diagnostics.object_["usedApproximateFit"] = Json_::Boolean(result.diagnostics_.usedApproximateFit_);
            value.object_["diagnostics"] = std::move(diagnostics);
            return value;
        }

        Json_ CurrencyPairJson(const CurrencyPair_& pair) {
            Json_ result = Json_::Object();
            result.object_["domestic"] = Json_::String(pair.domestic_.String());
            result.object_["foreign"] = Json_::String(pair.foreign_.String());
            return result;
        }

        Json_ FixingIdentityJson(const FixingIdentity_& fixing) {
            Json_ result = Json_::Object();
            result.object_["fixingHour"] = Json_::Number(fixing.fixingHour_);
            result.object_["fixingMinute"] = Json_::Number(fixing.fixingMinute_);
            result.object_["indexName"] = Json_::String(fixing.indexName_);
            return result;
        }

        Json_ XccyConfigJson(const CrossCurrencySwapConfig_& config) {
            Json_ result = Json_::Object();
            result.object_["domesticNotional"] = Json_::Number(config.domesticNotional_);
            result.object_["domesticRateFixing"] = FixingIdentityJson(config.domesticRateFixing_);
            result.object_["finalNotionalExchange"] = Json_::Boolean(config.convention_.finalNotionalExchange_);
            result.object_["foreignNotional"] = Json_::Number(config.foreignNotional_);
            result.object_["foreignRateFixing"] = FixingIdentityJson(config.foreignRateFixing_);
            Json_ reset = Json_::Object();
            reset.object_["fixingConvention"] = Json_::String(config.fxReset_.fixingConvention_.String());
            reset.object_["fixingHolidays"] = Json_::String(config.fxReset_.fixingHolidays_.String());
            reset.object_["fixingHour"] = Json_::Number(config.fxReset_.fixingHour_);
            reset.object_["fixingLag"] = Json_::Number(config.fxReset_.fixingLag_);
            reset.object_["fixingMinute"] = Json_::Number(config.fxReset_.fixingMinute_);
            result.object_["fxReset"] = std::move(reset);
            result.object_["initialNotionalExchange"] = Json_::Boolean(config.convention_.initialNotionalExchange_);
            result.object_["notionalMode"] = Json_::String(config.notionalMode_.String());
            result.object_["pair"] = CurrencyPairJson(config.pair_);
            result.object_["spreadOnForeignLeg"] = Json_::Boolean(config.convention_.spreadOnForeignLeg_);
            result.object_["domesticIndex"] = RateIndexJson(config.convention_.domesticIndex_);
            result.object_["domesticLeg"] = RateLegJson(config.convention_.domesticLeg_);
            result.object_["foreignIndex"] = RateIndexJson(config.convention_.foreignIndex_);
            result.object_["foreignLeg"] = RateLegJson(config.convention_.foreignLeg_);
            return result;
        }

        Json_ XccyInstrumentJson(const CrossCurrencySwap_& instrument) {
            Json_ result = Json_::Object();
            result.object_["config"] = XccyConfigJson(instrument.Config());
            result.object_["displayName"] = Json_::String(instrument.Name());
            result.object_["marketRate"] = Json_::Number(instrument.MarketRate());
            const auto span = instrument.TimeSpan();
            result.object_["maturityDate"] = DateJson(span.second);
            result.object_["startDate"] = DateJson(span.first);
            result.object_["tradeDate"] = DateJson(instrument.TradeDate());
            return result;
        }

        Json_ XccyInstrumentsJson(const Vector_<Handle_<CrossCurrencySwap_>>& instruments) {
            Json_ result = Json_::Array();
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                REQUIRE(instruments[i], "QUOTE_RISK_EMPTY_XCCY_CALIBRATION_INSTRUMENT");
                Json_ instrument = XccyInstrumentJson(*instruments[i]);
                instrument.object_["ordinal"] = Json_::Number(i);
                result.array_.push_back(std::move(instrument));
            }
            return result;
        }

        Json_ JointDeclarationJson(const JointCurveDeclaration_& declaration) {
            Json_ result = Json_::Object();
            result.object_["baseLayeredOverDiscount"] = Json_::Boolean(declaration.baseLayeredOverDiscount_);
            result.object_["calibrateDiscountCurve"] = Json_::Boolean(declaration.calibrateDiscountCurve_);
            result.object_["curveName"] = Json_::String(declaration.curveName_);
            result.object_["initialGuessPerNode"] = DoubleVectorJson(declaration.initialGuessPerNode_);
            result.object_["instruments"] = CalibrationInstrumentsJson(declaration.instruments_);
            result.object_["knotDates"] = DateVectorJson(declaration.knotDates_);
            result.object_["logDfScheme"] = Json_::String(declaration.logDfScheme_.String());
            result.object_["parameterization"] = Json_::String(declaration.parameterization_.String());
            result.object_["smoothingWeight"] = Json_::Number(declaration.smoothingWeight_);
            result.object_["targetCollateral"] = Json_::String(declaration.targetCollateral_.String());
            result.object_["targetTenor"] = Json_::String(declaration.targetTenor_.String());
            return result;
        }

        Json_ JointCurrencyJson(const JointCurrencyCurveSpec_& currency) {
            Json_ result = Json_::Object();
            result.object_["currency"] = Json_::String(currency.ccy_.String());
            result.object_["liborBasis"] = Json_::String(currency.liborBasis_.String());
            Json_ curves = Json_::Array();
            for (const auto& declaration : currency.curves_)
                curves.array_.push_back(JointDeclarationJson(declaration));
            result.object_["curves"] = std::move(curves);
            return result;
        }

        Json_ JointSpecJson(const JointXccyCalibrationSpec_& spec) {
            Json_ result = Json_::Object();
            Json_ basis = Json_::Object();
            basis.object_["curveName"] = Json_::String(spec.basis_.curveName_);
            basis.object_["initialGuessPerNode"] = DoubleVectorJson(spec.basis_.initialGuessPerNode_);
            basis.object_["instruments"] = XccyInstrumentsJson(spec.basis_.instruments_);
            basis.object_["knotDates"] = DateVectorJson(spec.basis_.knotDates_);
            basis.object_["logDfScheme"] = Json_::String(spec.basis_.logDfScheme_.String());
            basis.object_["parameterization"] = Json_::String(spec.basis_.parameterization_.String());
            basis.object_["smoothingWeight"] = Json_::Number(spec.basis_.smoothingWeight_);
            result.object_["basis"] = std::move(basis);
            result.object_["collateralCurrency"] = Json_::String(spec.collateralCurrency_.String());
            result.object_["domestic"] = JointCurrencyJson(spec.domestic_);
            result.object_["fitTolerance"] = Json_::Number(spec.fitTolerance_);
            result.object_["fixings"] = FixingsJson(spec.fixings_);
            result.object_["foreign"] = JointCurrencyJson(spec.foreign_);
            result.object_["fxSpot"] = Json_::Number(spec.fxSpot_);
            result.object_["initialGuess"] = Json_::Number(spec.initialGuess_);
            result.object_["maxEvaluations"] = Json_::Number(spec.maxEvaluations_);
            result.object_["maxRestarts"] = Json_::Number(spec.maxRestarts_);
            result.object_["pair"] = CurrencyPairJson(spec.pair_);
            result.object_["solveMode"] = Json_::String(spec.solveMode_.String());
            result.object_["tolerance"] = Json_::Number(spec.tolerance_);
            result.object_["valuationTime"] = DateTimeJson(spec.valuationTime_);
            return result;
        }

        Json_ JointOptionsJson(const JointXccyCalibrationOptions_& options) {
            Json_ result = Json_::Object();
            result.object_["computeEffJacobianInverse"] = Json_::Boolean(options.computeEffJacobianInverse_);
            result.object_["computeForwardJacobian"] = Json_::Boolean(options.computeForwardJacobian_);
            result.object_["jacobianMode"] = Json_::String(options.jacobianMode_.String());
            return result;
        }

        Json_ CalibrationRangesJson(const Vector_<CalibrationBlockRange_>& ranges) {
            Json_ result = Json_::Array();
            for (const auto& range : ranges) {
                Json_ entry = Json_::Object();
                entry.object_["name"] = Json_::String(range.name_);
                entry.object_["offset"] = Json_::Number(range.offset_);
                entry.object_["size"] = Json_::Number(range.size_);
                result.array_.push_back(std::move(entry));
            }
            return result;
        }

        Json_ FxForwardJson(const CrossCurrencyFxForwardCurve_& curve) {
            Json_ result = Json_::Object();
            result.object_["dates"] = DateVectorJson(curve.dates_);
            result.object_["forwards"] = DoubleVectorJson(curve.forwards_);
            result.object_["pair"] = CurrencyPairJson(curve.pair_);
            return result;
        }

        Json_ JointDiagnosticsJson(const JointCurveCalibrationDiagnostics_& diagnostics) {
            Json_ result = Json_::Object();
            result.object_["curveIndex"] = Json_::Number(diagnostics.curveIndex_);
            result.object_["curveName"] = Json_::String(diagnostics.curveName_);
            result.object_["instrumentNames"] = StringVectorJson(diagnostics.instrumentNames_);
            result.object_["marketRates"] = DoubleVectorJson(diagnostics.marketRates_);
            result.object_["maxAbsResidual"] = Json_::Number(diagnostics.maxAbsResidual_);
            result.object_["modelRates"] = DoubleVectorJson(diagnostics.modelRates_);
            result.object_["residuals"] = DoubleVectorJson(diagnostics.residuals_);
            result.object_["rmsResidual"] = Json_::Number(diagnostics.rmsResidual_);
            result.object_["usedApproximateFit"] = Json_::Boolean(diagnostics.usedApproximateFit_);
            return result;
        }

        Json_ XccyDiagnosticsJson(const CrossCurrencyCalibrationDiagnostics_& diagnostics) {
            Json_ result = Json_::Object();
            result.object_["effectiveInverse"] = MatrixJson(diagnostics.effJacobianInverse_);
            result.object_["effectiveInverseAvailability"] = Json_::String(diagnostics.effJacobianInverseAvailability_);
            result.object_["effectiveInverseScaling"] = Json_::String(diagnostics.effJacobianInverseScaling_);
            result.object_["instrumentNames"] = StringVectorJson(diagnostics.instrumentNames_);
            result.object_["jacobian"] = MatrixJson(diagnostics.jacobian_);
            result.object_["jacobianAvailability"] = Json_::String(diagnostics.jacobianAvailability_);
            result.object_["jacobianScaling"] = Json_::String(diagnostics.jacobianScaling_);
            result.object_["marketRates"] = DoubleVectorJson(diagnostics.marketRates_);
            result.object_["maxAbsResidual"] = Json_::Number(diagnostics.maxAbsResidual_);
            result.object_["modelRates"] = DoubleVectorJson(diagnostics.modelRates_);
            result.object_["parameterKnotDates"] = DateVectorJson(diagnostics.parameterKnotDates_);
            result.object_["residuals"] = DoubleVectorJson(diagnostics.residuals_);
            result.object_["residualTolerance"] = Json_::Number(diagnostics.residualTolerance_);
            result.object_["rmsResidual"] = Json_::Number(diagnostics.rmsResidual_);
            result.object_["usedApproximateFit"] = Json_::Boolean(diagnostics.usedApproximateFit_);
            return result;
        }

        Json_ JointResultJson(const JointXccyCalibrationResult_& result) {
            REQUIRE(result.domesticCurveBlock_ && result.foreignCurveBlock_ && result.basisCurve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            Json_ value = Json_::Object();
            value.object_["basisCurve"] = StorableJson(*result.basisCurve_);
            value.object_["converged"] = Json_::Boolean(result.converged_);
            Json_ domestic = Json_::Array();
            for (const auto& diagnostics : result.domesticDiagnostics_)
                domestic.array_.push_back(JointDiagnosticsJson(diagnostics));
            value.object_["domesticBlock"] = CurveBlockJson(*result.domesticCurveBlock_);
            value.object_["domesticDiagnostics"] = std::move(domestic);
            value.object_["effectiveInverse"] = MatrixJson(result.effJacobianInverse_);
            Json_ foreign = Json_::Array();
            for (const auto& diagnostics : result.foreignDiagnostics_)
                foreign.array_.push_back(JointDiagnosticsJson(diagnostics));
            value.object_["foreignBlock"] = CurveBlockJson(*result.foreignCurveBlock_);
            value.object_["foreignDiagnostics"] = std::move(foreign);
            value.object_["forwardJacobian"] = MatrixJson(result.jacobianAtSolution_);
            value.object_["fxForwardCurve"] = FxForwardJson(result.fxForwardCurve_);
            value.object_["jointMaxAbsResidual"] = Json_::Number(result.jointMaxAbsResidual_);
            value.object_["jointRmsResidual"] = Json_::Number(result.jointRmsResidual_);
            value.object_["marketRates"] = DoubleVectorJson(result.marketRates_);
            value.object_["modelRates"] = DoubleVectorJson(result.modelRates_);
            value.object_["parameterRanges"] = CalibrationRangesJson(result.parameterRanges_);
            value.object_["residualRanges"] = CalibrationRangesJson(result.residualRanges_);
            value.object_["residuals"] = DoubleVectorJson(result.residuals_);
            value.object_["solverEvaluations"] = Json_::Number(result.solverEvaluations_);
            value.object_["usedApproximateFit"] = Json_::Boolean(result.usedApproximateFit_);
            value.object_["xccyDiagnostics"] = XccyDiagnosticsJson(result.xccyDiagnostics_);
            return value;
        }

        DateTime_ StagedValuationTime(const CrossCurrencyCalibrationSpec_& spec) {
            return spec.valuationTime_.IsValid() ? spec.valuationTime_ : DateTime_(spec.today_);
        }

        Ccy_ StagedCollateralCurrency(const CrossCurrencyCalibrationSpec_& spec) {
            return spec.collateralCurrency_.Switch() == Ccy_::Value_::_NOT_SET ? spec.basisPair_.domestic_ : spec.collateralCurrency_;
        }

        Json_ StagedSpecJson(const CrossCurrencyCalibrationSpec_& spec) {
            Json_ result = Json_::Object();
            result.object_["basisPair"] = CurrencyPairJson(spec.basisPair_);
            result.object_["collateralCurrency"] = Json_::String(StagedCollateralCurrency(spec).String());
            REQUIRE(spec.domesticCurveBlock_ && spec.foreignCurveBlock_, "QUOTE_RISK_SPEC_CURVE_EMPTY");
            result.object_["domesticBlock"] = CurveBlockJson(*spec.domesticCurveBlock_);
            result.object_["fitTolerance"] = Json_::Number(spec.fitTolerance_);
            result.object_["fixings"] = FixingsJson(spec.fixings_);
            result.object_["foreignBlock"] = CurveBlockJson(*spec.foreignCurveBlock_);
            result.object_["fxForwardCollateral"] = Json_::String(spec.fxForwardCollateral_.String());
            result.object_["fxSpot"] = Json_::Number(spec.fxSpot_);
            result.object_["initialGuess"] = Json_::Number(spec.initialGuess_);
            result.object_["initialGuessPerNode"] = DoubleVectorJson(spec.initialGuessPerNode_);
            result.object_["instruments"] = XccyInstrumentsJson(spec.instruments_);
            result.object_["knotDates"] = DateVectorJson(spec.knotDates_);
            result.object_["maxEvaluations"] = Json_::Number(spec.maxEvaluations_);
            result.object_["maxRestarts"] = Json_::Number(spec.maxRestarts_);
            result.object_["smoothingWeight"] = Json_::Number(spec.smoothingWeight_);
            result.object_["solveMode"] = Json_::String(spec.solveMode_.String());
            result.object_["today"] = DateJson(StagedValuationTime(spec).Date());
            result.object_["tolerance"] = Json_::Number(spec.tolerance_);
            result.object_["valuationTime"] = DateTimeJson(StagedValuationTime(spec));
            return result;
        }

        Json_ StagedOptionsJson(const CrossCurrencyCalibrationOptions_& options) {
            Json_ result = Json_::Object();
            result.object_["computeEffJacobianInverse"] = Json_::Boolean(options.computeEffJacobianInverse_);
            result.object_["computeForwardJacobian"] = Json_::Boolean(options.computeForwardJacobian_);
            result.object_["jacobianMode"] = Json_::String(options.jacobianMode_.String());
            return result;
        }

        Json_ StagedResultJson(const CrossCurrencyCalibrationResult_& result) {
            Json_ value = Json_::Object();
            Json_ basisCurves = Json_::Array();
            for (const auto& curve : result.basisCurves_) {
                REQUIRE(curve.second, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
                Json_ entry = Json_::Object();
                entry.object_["curve"] = StorableJson(*curve.second);
                entry.object_["pair"] = CurrencyPairJson(curve.first);
                basisCurves.array_.push_back(std::move(entry));
            }
            value.object_["basisCurves"] = std::move(basisCurves);
            value.object_["diagnostics"] = XccyDiagnosticsJson(result.diagnostics_);
            Json_ market = Json_::Object();
            market.object_["basisCurve"] = result.market_.BasisCurve() ? StorableJson(*result.market_.BasisCurve()) : Json_();
            market.object_["collateralCurrency"] = Json_::String(result.market_.CollateralCurrency().String());
            market.object_["domesticBlock"] = CurveBlockJson(result.market_.DomesticBlock());
            market.object_["domesticCurrency"] = Json_::String(result.market_.DomesticCcy().String());
            market.object_["fixings"] = FixingsJson(result.market_.Fixings());
            market.object_["foreignBlock"] = CurveBlockJson(result.market_.ForeignBlock());
            market.object_["foreignCurrency"] = Json_::String(result.market_.ForeignCcy().String());
            market.object_["fxSpot"] = Json_::Number(result.market_.FxSpot());
            market.object_["valuationTime"] = DateTimeJson(result.market_.ValuationTime());
            value.object_["market"] = std::move(market);
            value.object_["fxForwardCurve"] = FxForwardJson(result.fxForwardCurve_);
            return value;
        }

        Json_ RangesJson(const Vector_<RateQuoteRiskRange_>& ranges) {
            Json_ result = Json_::Array();
            for (const auto& range : ranges) {
                Json_ entry = Json_::Object();
                entry.object_["blockKey"] = Json_::String(range.blockKey_);
                entry.object_["offset"] = Json_::Number(range.offset_);
                entry.object_["size"] = Json_::Number(range.size_);
                result.array_.push_back(std::move(entry));
            }
            return result;
        }

        Json_ AxisJson(const String_& kind, const RateQuoteRiskAxis_& axis) {
            Json_ result = Json_::Object();
            result.object_["kind"] = Json_::String(kind);
            Json_ parameters = Json_::Array();
            for (const auto& parameter : axis.parameters_) {
                Json_ entry = Json_::Object();
                entry.object_["blockKey"] = Json_::String(parameter.blockKey_);
                entry.object_["blockOrdinal"] = Json_::Number(parameter.blockOrdinal_);
                entry.object_["component"] = Json_::String(parameter.component_.String());
                entry.object_["date"] = DateJson(parameter.date_);
                entry.object_["globalOrdinal"] = Json_::Number(parameter.globalOrdinal_);
                parameters.array_.push_back(std::move(entry));
            }
            result.object_["parameterCoordinates"] = std::move(parameters);
            result.object_["parameterRanges"] = RangesJson(axis.parameterRanges_);
            Json_ quotes = Json_::Array();
            for (const auto& quote : axis.quotes_) {
                Json_ entry = Json_::Object();
                entry.object_["blockKey"] = Json_::String(quote.blockKey_);
                entry.object_["blockOrdinal"] = Json_::Number(quote.blockOrdinal_);
                entry.object_["displayName"] = Json_::String(quote.displayName_);
                entry.object_["globalOrdinal"] = Json_::Number(quote.globalOrdinal_);
                entry.object_["unit"] = Json_::String(quote.unit_);
                quotes.array_.push_back(std::move(entry));
            }
            result.object_["quoteCoordinates"] = std::move(quotes);
            result.object_["residualRanges"] = RangesJson(axis.residualRanges_);
            result.object_["scheme"] = Json_::String(axis.scheme_);
            return result;
        }

        void ValidateFiniteMatrix(const Matrix_<>& value, const String_& context) {
            for (int row = 0; row < value.Rows(); ++row)
                for (int column = 0; column < value.Cols(); ++column)
                    REQUIRE(std::isfinite(value(row, column)), context + " contains a non-finite value");
        }

        void ValidateFiniteVector(const Vector_<>& value, const String_& context) {
            for (const double entry : value)
                REQUIRE(std::isfinite(entry), context + " contains a non-finite value");
        }

        int ValidateRangePartition(const Vector_<CalibrationBlockRange_>& ranges, const String_& context) {
            REQUIRE(!ranges.empty(), context + " ranges are empty");
            int offset = 0;
            std::set<String_> names;
            for (const auto& range : ranges) {
                REQUIRE(!range.name_.empty() && range.size_ > 0 && range.offset_ == offset, context + " ranges do not form a complete partition");
                REQUIRE(names.insert(range.name_).second, context + " ranges contain a duplicate block");
                REQUIRE(range.offset_ <= std::numeric_limits<int>::max() - range.size_, context + " range overflows");
                offset += range.size_;
            }
            return offset;
        }

        CurveDefinition_ JointDefinition(const JointCurveDeclaration_& declaration, const JointCurrencyCurveSpec_& currency, const Date_& anchor) {
            return MakeCurveDefinition(declaration.curveName_, currency.ccy_.String(), declaration.parameterization_, declaration.logDfScheme_,
                                       declaration.knotDates_, anchor, currency.liborBasis_);
        }

        void ValidateBase(const Handle_<DiscountCurve_>& actual, const Handle_<DiscountCurve_>& expected) {
            REQUIRE(static_cast<bool>(actual) == static_cast<bool>(expected), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            if (actual)
                REQUIRE(Jcs(StorableJson(*actual)) == Jcs(StorableJson(*expected)), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        template <class Curve_> void ValidateExpectedBase(const Curve_& curve, const Handle_<DiscountCurve_>* expectedBase) {
            if (expectedBase)
                ValidateBase(curve.Base(), *expectedBase);
        }

        void ValidateSolvedPwc(const DiscountCurve_& curve, const CurveDefinition_& definition, const Handle_<DiscountCurve_>* expectedBase) {
            const auto* typed = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve);
            REQUIRE(typed, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->KnotDates() == definition.nodeDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateExpectedBase(*typed, expectedBase);
        }

        void ValidateSolvedPwlf(const DiscountCurve_& curve, const CurveDefinition_& definition, const Handle_<DiscountCurve_>* expectedBase) {
            const auto* typed = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve);
            REQUIRE(typed, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->KnotDates() == definition.nodeDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateExpectedBase(*typed, expectedBase);
        }

        void ValidateSolvedLogDf(const DiscountCurve_& curve, const CurveDefinition_& definition, const Handle_<DiscountCurve_>* expectedBase) {
            const auto* typed = dynamic_cast<const Tape::DiscountLogDF_<double>*>(&curve);
            REQUIRE(typed, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->NodeDates() == definition.nodeDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->Scheme() == definition.logDfScheme_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->DayCount() == definition.dayCount_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateExpectedBase(*typed, expectedBase);
        }

        void ValidateSolvedZeroRate(const DiscountCurve_& curve, const CurveDefinition_& definition, const Handle_<DiscountCurve_>* expectedBase) {
            const auto* typed = dynamic_cast<const Tape::DiscountZeroRate_<double>*>(&curve);
            REQUIRE(typed, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->AnchorDate() == definition.anchorDate_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->NodeDates() == definition.nodeDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->Scheme() == definition.logDfScheme_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(typed->DayCount() == definition.dayCount_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateExpectedBase(*typed, expectedBase);
        }

        void ValidateSolvedCurveDefinition(const DiscountCurve_& curve,
                                           const CurveDefinition_& definition,
                                           const Handle_<DiscountCurve_>* expectedBase = nullptr) {
            REQUIRE(curve.Name() == definition.name_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(curve.ccy_.String() == definition.ccy_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            switch (definition.parameterization_.Switch()) {
            case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
                ValidateSolvedPwc(curve, definition, expectedBase);
                return;
            case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
                ValidateSolvedPwlf(curve, definition, expectedBase);
                return;
            case CurveParameterization_::Value_::LOG_DISCOUNT:
                ValidateSolvedLogDf(curve, definition, expectedBase);
                return;
            case CurveParameterization_::Value_::ZERO_RATE:
                ValidateSolvedZeroRate(curve, definition, expectedBase);
                return;
            default:
                REQUIRE(false, "QUOTE_RISK_UNSUPPORTED_CURVE_PARAMETERIZATION");
            }
        }

        CurveDefinition_ JointBasisDefinition(const JointXccyCalibrationSpec_& spec) {
            return MakeCurveDefinition(spec.basis_.curveName_, spec.pair_.domestic_.String(), spec.basis_.parameterization_, spec.basis_.logDfScheme_,
                                       spec.basis_.knotDates_, spec.valuationTime_.Date(), DayBasis::Act365F());
        }

        void ValidateUnlayeredSolvedCurve(const DiscountCurve_& curve, const CurveDefinition_& definition) {
            const Handle_<DiscountCurve_> noBase;
            ValidateSolvedCurveDefinition(curve, definition, &noBase);
        }

        struct JointCurveSlots_ {
            std::set<CollateralType_> discounts_;
            std::set<PeriodLength_> forwards_;
        };

        JointCurveSlots_ ExpectedJointCurveSlots(const JointCurrencyCurveSpec_& currency) {
            JointCurveSlots_ result;
            for (const auto& declaration : currency.curves_) {
                if (declaration.calibrateDiscountCurve_) {
                    REQUIRE(!declaration.baseLayeredOverDiscount_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                    REQUIRE(result.discounts_.insert(declaration.targetCollateral_).second, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                } else {
                    REQUIRE(result.forwards_.insert(declaration.targetTenor_).second, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                }
            }
            return result;
        }

        template <class Key_> void ValidateExactCurveSlots(const std::map<Key_, Handle_<DiscountCurve_>>& actual, const std::set<Key_>& expected) {
            REQUIRE(actual.size() == expected.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            for (const auto& key : expected)
                REQUIRE(actual.find(key) != actual.end(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        const DiscountCurve_& JointSlotCurve(const JointCurveDeclaration_& declaration, const CurveBlock_& block) {
            if (declaration.calibrateDiscountCurve_) {
                const auto found = block.DiscountCurves().find(declaration.targetCollateral_);
                REQUIRE(found != block.DiscountCurves().end(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                return *found->second;
            }
            const auto found = block.ForwardCurves().find(declaration.targetTenor_);
            REQUIRE(found != block.ForwardCurves().end(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            return *found->second;
        }

        Handle_<DiscountCurve_> ExpectedJointSlotBase(const JointCurveDeclaration_& declaration, const CurveBlock_& block) {
            if (declaration.calibrateDiscountCurve_ || !declaration.baseLayeredOverDiscount_)
                return Handle_<DiscountCurve_>();
            const auto found = block.DiscountCurves().find(declaration.targetCollateral_);
            REQUIRE(found != block.DiscountCurves().end(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            return found->second;
        }

        void ValidateJointCurveBlockTopology(const JointCurrencyCurveSpec_& currency, const CurveBlock_& block, const Date_& anchor) {
            const JointCurveSlots_ expected = ExpectedJointCurveSlots(currency);
            REQUIRE(block.ccy_ == currency.ccy_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(block.LiborBasis() == currency.liborBasis_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateExactCurveSlots(block.DiscountCurves(), expected.discounts_);
            ValidateExactCurveSlots(block.ForwardCurves(), expected.forwards_);
            for (const auto& declaration : currency.curves_) {
                const Handle_<DiscountCurve_> expectedBase = ExpectedJointSlotBase(declaration, block);
                ValidateSolvedCurveDefinition(JointSlotCurve(declaration, block), JointDefinition(declaration, currency, anchor), &expectedBase);
            }
        }

        void ValidateJointResultTopology(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result) {
            const Date_ anchor = spec.valuationTime_.Date();
            ValidateJointCurveBlockTopology(spec.domestic_, *result.domesticCurveBlock_, anchor);
            ValidateJointCurveBlockTopology(spec.foreign_, *result.foreignCurveBlock_, anchor);
            ValidateUnlayeredSolvedCurve(*result.basisCurve_, JointBasisDefinition(spec));
        }

        const DiscountCurve_&
        JointResultCurve(const String_& block, const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result) {
            auto find = [&](const String_& group, const JointCurrencyCurveSpec_& currency, const CurveBlock_& curveBlock) -> const DiscountCurve_* {
                for (const auto& declaration : currency.curves_) {
                    if (block != group + ":" + declaration.curveName_)
                        continue;
                    return declaration.calibrateDiscountCurve_ ? &curveBlock.Discount(declaration.targetCollateral_)
                                                               : &curveBlock.Forward(declaration.targetTenor_, declaration.targetCollateral_);
                }
                return nullptr;
            };
            if (const auto* curve = find("domestic", spec.domestic_, *result.domesticCurveBlock_))
                return *curve;
            if (const auto* curve = find("foreign", spec.foreign_, *result.foreignCurveBlock_))
                return *curve;
            REQUIRE(block == String_("basis:") + spec.basis_.curveName_, "QUOTE_RISK_PARAMETER_RANGE_SPEC_MISMATCH");
            return *result.basisCurve_;
        }

        void ValidateJointAxisCurveRanges(const String_& block,
                                          const JointCurveDeclaration_& declaration,
                                          const Vector_<CurveFreeParameter_>& parameters,
                                          const CalibrationBlockRange_& parameterRange,
                                          const CalibrationBlockRange_& residualRange) {
            REQUIRE(parameterRange.name_ == block, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(residualRange.name_ == block, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(parameterRange.size_ == static_cast<int>(parameters.size()), "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(residualRange.size_ == static_cast<int>(declaration.instruments_.size()), "QUOTE_RISK_RANGE_SPEC_MISMATCH");
        }

        void ValidateJointAxisCurveDiagnostics(const JointCurveDeclaration_& declaration, const JointCurveCalibrationDiagnostics_& diagnostics) {
            REQUIRE(diagnostics.curveName_ == declaration.curveName_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(diagnostics.instrumentNames_.size() == declaration.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(diagnostics.marketRates_.size() == declaration.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(diagnostics.modelRates_.size() == declaration.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(diagnostics.residuals_.size() == declaration.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateOrderedInstrumentPairing(const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<String_>& instrumentNames,
                                              const Vector_<>& marketRates) {
            for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
                REQUIRE(instruments[i], "QUOTE_RISK_EMPTY_CALIBRATION_INSTRUMENT");
                REQUIRE(instrumentNames[i] == instruments[i]->Name(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                REQUIRE(marketRates[i] == instruments[i]->MarketRate(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            }
        }

        void AppendAxisParameters(const String_& block, int offset, const Vector_<CurveFreeParameter_>& parameters, RateQuoteRiskAxis_* axis) {
            for (int i = 0; i < static_cast<int>(parameters.size()); ++i)
                axis->parameters_.push_back({block, i, offset + i, parameters[i].date_, parameters[i].component_});
        }

        void AppendAxisQuotes(const String_& block, int offset, const Vector_<String_>& instrumentNames, RateQuoteRiskAxis_* axis) {
            for (int i = 0; i < static_cast<int>(instrumentNames.size()); ++i)
                axis->quotes_.push_back({block, i, offset + i, instrumentNames[i], "DECIMAL_QUOTE"});
        }

        void AppendJointAxisCurve(const String_& group,
                                  const JointCurrencyCurveSpec_& currency,
                                  const JointCurveDeclaration_& declaration,
                                  const JointCurveCalibrationDiagnostics_& diagnostics,
                                  const CalibrationBlockRange_& parameterRange,
                                  const CalibrationBlockRange_& residualRange,
                                  const Date_& anchor,
                                  RateQuoteRiskAxis_* axis) {
            const String_ block = group + ":" + declaration.curveName_;
            const auto parameters = DescribeCurveFreeParameters(JointDefinition(declaration, currency, anchor));
            ValidateJointAxisCurveRanges(block, declaration, parameters, parameterRange, residualRange);
            ValidateJointAxisCurveDiagnostics(declaration, diagnostics);
            const auto instruments = OrderInstruments(declaration.instruments_);
            ValidateOrderedInstrumentPairing(instruments, diagnostics.instrumentNames_, diagnostics.marketRates_);
            axis->parameterRanges_.push_back({block, parameterRange.offset_, parameterRange.size_});
            axis->residualRanges_.push_back({block, residualRange.offset_, residualRange.size_});
            AppendAxisParameters(block, parameterRange.offset_, parameters, axis);
            AppendAxisQuotes(block, residualRange.offset_, diagnostics.instrumentNames_, axis);
        }

        void ValidateJointAxisShape(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result, int residualCount) {
            const std::size_t currencyRangeCount = spec.domestic_.curves_.size() + spec.foreign_.curves_.size();
            REQUIRE(result.parameterRanges_.size() == currencyRangeCount + 1, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(result.residualRanges_.size() == currencyRangeCount + 1, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(result.domesticDiagnostics_.size() == spec.domestic_.curves_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.foreignDiagnostics_.size() == spec.foreign_.curves_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.marketRates_.size()) == residualCount, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.modelRates_.size()) == residualCount, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.residuals_.size()) == residualCount, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateJointRates(const JointXccyCalibrationResult_& result,
                                const JointCurveCalibrationDiagnostics_& diagnostics,
                                const CalibrationBlockRange_& range) {
            for (int i = 0; i < range.size_; ++i) {
                REQUIRE(result.marketRates_[range.offset_ + i] == diagnostics.marketRates_[i], "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                REQUIRE(result.modelRates_[range.offset_ + i] == diagnostics.modelRates_[i], "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                REQUIRE(result.residuals_[range.offset_ + i] == diagnostics.residuals_[i], "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            }
        }

        int AppendJointCurrencyAxis(const String_& group,
                                    const JointCurrencyCurveSpec_& currency,
                                    const Vector_<JointCurveCalibrationDiagnostics_>& diagnostics,
                                    const JointXccyCalibrationResult_& result,
                                    int range,
                                    const Date_& anchor,
                                    RateQuoteRiskAxis_* axis) {
            for (int i = 0; i < static_cast<int>(currency.curves_.size()); ++i, ++range) {
                AppendJointAxisCurve(group, currency, currency.curves_[i], diagnostics[i], result.parameterRanges_[range],
                                     result.residualRanges_[range], anchor, axis);
                ValidateJointRates(result, diagnostics[i], result.residualRanges_[range]);
            }
            return range;
        }

        void ValidateJointBasisShape(const JointXccyCalibrationSpec_& spec,
                                     const JointXccyCalibrationResult_& result,
                                     const Vector_<CurveFreeParameter_>& parameters,
                                     const String_& parameterBlock,
                                     const String_& residualBlock,
                                     const CalibrationBlockRange_& parameterRange,
                                     const CalibrationBlockRange_& residualRange) {
            REQUIRE(parameterRange.name_ == parameterBlock, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(residualRange.name_ == residualBlock, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(parameterRange.size_ == static_cast<int>(parameters.size()), "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(residualRange.size_ == static_cast<int>(spec.basis_.instruments_.size()), "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.instrumentNames_.size() == spec.basis_.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.marketRates_.size() == spec.basis_.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.modelRates_.size() == spec.basis_.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.residuals_.size() == spec.basis_.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateJointBasisInstrument(const JointXccyCalibrationSpec_& spec,
                                          const JointXccyCalibrationResult_& result,
                                          const CalibrationBlockRange_& residualRange,
                                          int index) {
            REQUIRE(spec.basis_.instruments_[index], "QUOTE_RISK_EMPTY_XCCY_CALIBRATION_INSTRUMENT");
            REQUIRE(result.xccyDiagnostics_.instrumentNames_[index] == spec.basis_.instruments_[index]->Name(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.marketRates_[index] == spec.basis_.instruments_[index]->MarketRate(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.marketRates_[residualRange.offset_ + index] == result.xccyDiagnostics_.marketRates_[index],
                    "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.modelRates_[residualRange.offset_ + index] == result.xccyDiagnostics_.modelRates_[index],
                    "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.residuals_[residualRange.offset_ + index] == result.xccyDiagnostics_.residuals_[index], "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void
        AppendJointBasisAxis(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result, int range, RateQuoteRiskAxis_* axis) {
            const auto parameters = DescribeCurveFreeParameters(JointBasisDefinition(spec));
            const String_ parameterBlock = String_("basis:") + spec.basis_.curveName_;
            const String_ residualBlock = String_("xccy:") + spec.basis_.curveName_;
            const auto& parameterRange = result.parameterRanges_[range];
            const auto& residualRange = result.residualRanges_[range];
            ValidateJointBasisShape(spec, result, parameters, parameterBlock, residualBlock, parameterRange, residualRange);
            axis->parameterRanges_.push_back({parameterBlock, parameterRange.offset_, parameterRange.size_});
            axis->residualRanges_.push_back({residualBlock, residualRange.offset_, residualRange.size_});
            AppendAxisParameters(parameterBlock, parameterRange.offset_, parameters, axis);
            for (int i = 0; i < static_cast<int>(spec.basis_.instruments_.size()); ++i)
                ValidateJointBasisInstrument(spec, result, residualRange, i);
            AppendAxisQuotes(residualBlock, residualRange.offset_, result.xccyDiagnostics_.instrumentNames_, axis);
        }

        RateQuoteRiskAxis_ JointAxis(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result) {
            const int parameterCount = ValidateRangePartition(result.parameterRanges_, "QUOTE_RISK_PARAMETER");
            const int residualCount = ValidateRangePartition(result.residualRanges_, "QUOTE_RISK_RESIDUAL");
            ValidateJointAxisShape(spec, result, residualCount);

            RateQuoteRiskAxis_ axis;
            axis.scheme_ = RateQuoteRiskAxisFingerprintScheme();
            int range = 0;
            range =
                AppendJointCurrencyAxis("domestic", spec.domestic_, result.domesticDiagnostics_, result, range, spec.valuationTime_.Date(), &axis);
            range = AppendJointCurrencyAxis("foreign", spec.foreign_, result.foreignDiagnostics_, result, range, spec.valuationTime_.Date(), &axis);
            AppendJointBasisAxis(spec, result, range, &axis);
            REQUIRE(static_cast<int>(axis.parameters_.size()) == parameterCount, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            REQUIRE(static_cast<int>(axis.quotes_.size()) == residualCount, "QUOTE_RISK_RANGE_SPEC_MISMATCH");
            axis.fingerprint_ = Fingerprint(AxisJson("JOINT_XCCY", axis));
            return axis;
        }

        CurveDefinition_ StagedBasisDefinition(const CrossCurrencyCalibrationSpec_& spec) {
            const Date_ anchor = StagedValuationTime(spec).Date();
            return MakeCurveDefinition(String_("xccy_basis_") + spec.basisPair_.domestic_.String(), spec.basisPair_.domestic_.String(),
                                       CurveParameterization_(CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD),
                                       LogDfScheme_(LogDfScheme_::Value_::LOG_LINEAR), spec.knotDates_, anchor, DayBasis::Act365F());
        }

        void ValidateStagedAxisDiagnostics(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationResult_& result) {
            REQUIRE(result.diagnostics_.parameterKnotDates_ == spec.knotDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.diagnostics_.instrumentNames_.size() == spec.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.diagnostics_.marketRates_.size() == spec.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.diagnostics_.modelRates_.size() == spec.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.diagnostics_.residuals_.size() == spec.instruments_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateStagedAxisInstruments(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationResult_& result) {
            for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
                REQUIRE(spec.instruments_[i], "QUOTE_RISK_EMPTY_XCCY_CALIBRATION_INSTRUMENT");
                REQUIRE(result.diagnostics_.instrumentNames_[i] == spec.instruments_[i]->Name(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                REQUIRE(result.diagnostics_.marketRates_[i] == spec.instruments_[i]->MarketRate(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            }
        }

        RateQuoteRiskAxis_ StagedAxis(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationResult_& result) {
            const auto parameters = DescribeCurveFreeParameters(StagedBasisDefinition(spec));
            ValidateStagedAxisDiagnostics(spec, result);
            ValidateStagedAxisInstruments(spec, result);
            const String_ curveName = String_("xccy_basis_") + spec.basisPair_.domestic_.String();
            const String_ parameterBlock = String_("basis:") + curveName;
            const String_ residualBlock = String_("xccy:") + curveName;
            RateQuoteRiskAxis_ axis;
            axis.scheme_ = RateQuoteRiskAxisFingerprintScheme();
            axis.parameterRanges_.push_back({parameterBlock, 0, static_cast<int>(parameters.size())});
            axis.residualRanges_.push_back({residualBlock, 0, static_cast<int>(spec.instruments_.size())});
            AppendAxisParameters(parameterBlock, 0, parameters, &axis);
            AppendAxisQuotes(residualBlock, 0, result.diagnostics_.instrumentNames_, &axis);
            axis.fingerprint_ = Fingerprint(AxisJson("STAGED_XCCY_BASIS", axis));
            return axis;
        }

        void ValidateEffectiveInverse(const Matrix_<>& inverse, bool mustBeEmpty, int parameters, int residuals) {
            if (mustBeEmpty) {
                REQUIRE(inverse.Empty(), "QUOTE_RISK_OPTIONS_RESULT_MISMATCH");
                return;
            }
            if (inverse.Empty())
                return;
            REQUIRE(inverse.Rows() == parameters, "QUOTE_RISK_EFFECTIVE_INVERSE_SHAPE_INVALID");
            REQUIRE(inverse.Cols() == residuals, "QUOTE_RISK_EFFECTIVE_INVERSE_SHAPE_INVALID");
            ValidateFiniteMatrix(inverse, "QUOTE_RISK_EFFECTIVE_INVERSE");
        }

        void ValidateSingleDiagnosticsShape(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& result) {
            const int instruments = static_cast<int>(spec.instruments_.size());
            REQUIRE(result.diagnostics_.curveName_ == spec.curveName_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.diagnostics_.instrumentNames_.size()) == instruments, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.diagnostics_.marketRates_.size()) == instruments, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.diagnostics_.modelRates_.size()) == instruments, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.diagnostics_.residuals_.size()) == instruments, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateSingleInstruments(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& result) {
            for (int i = 0; i < static_cast<int>(spec.instruments_.size()); ++i) {
                REQUIRE(spec.instruments_[i], "QUOTE_RISK_EMPTY_CALIBRATION_INSTRUMENT");
                REQUIRE(result.diagnostics_.instrumentNames_[i] == spec.instruments_[i]->Name(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
                REQUIRE(result.diagnostics_.marketRates_[i] == spec.instruments_[i]->MarketRate(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            }
        }

        void ValidateSinglePairing(const CurveCalibrationSpec_& spec,
                                   const CurveCalibrationResult_& result,
                                   const CurveCalibrationOptions_& options,
                                   int parameterCount) {
            REQUIRE(result.curve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            const CurveDefinition_ definition = MakeCurveDefinition(spec.curveName_, spec.ccy_, spec.Parameterization(), spec.LogDfScheme(),
                                                                    spec.KnotDates(), spec.Today(), spec.liborBasis_);
            ValidateSolvedCurveDefinition(*result.curve_, definition, &spec.baseCurve_);
            ValidateSingleDiagnosticsShape(spec, result);
            ValidateSingleInstruments(spec, result);
            const bool approximate = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            REQUIRE(result.diagnostics_.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateEffectiveInverse(result.diagnostics_.effJacobianInverse_, !options.computeEffJacobianInverse_ || approximate, parameterCount,
                                     static_cast<int>(spec.instruments_.size()));
        }

        void ValidateConfig(const RateQuoteRiskProvenanceConfig_& config, const Vector_<RateQuoteRiskRange_>& parameterRanges) {
            REQUIRE(!config.calibrationId_.empty(), "QUOTE_RISK_CALIBRATION_ID_EMPTY");
            REQUIRE(config.componentKeyByParameterBlock_.size() == parameterRanges.size(), "QUOTE_RISK_PARAMETER_BLOCK_BINDINGS_INCOMPLETE");
            std::set<String_> components;
            for (const auto& range : parameterRanges) {
                const auto binding = config.componentKeyByParameterBlock_.find(range.blockKey_);
                REQUIRE(binding != config.componentKeyByParameterBlock_.end() && !binding->second.empty(),
                        "QUOTE_RISK_PARAMETER_BLOCK_BINDINGS_INCOMPLETE");
                REQUIRE(components.insert(binding->second).second, "QUOTE_RISK_PARAMETER_BLOCK_BINDING_DUPLICATE");
            }
        }

        String_ AvailabilityReason(const CurveSolveMode_& solveMode, bool inverseRequested, const Matrix_<>& inverse) {
            if (!inverseRequested)
                return "QUOTE_RISK_INVERSE_NOT_REQUESTED";
            if (solveMode != CurveSolveMode_::Value_::EXACT)
                return "QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE";
            if (inverse.Empty())
                return "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE";
            return String_();
        }

        CurveCalibrationSpec_ NormalizeSingleSpec(const CurveCalibrationSpec_& spec) {
            CurveCalibrationSpec_ result = spec;
            result.instruments_ = OrderInstruments(spec.instruments_);
            result.knotDates_ = BuildCurveCalibrationKnots(spec.today_, result.instruments_, spec.knotDates_, spec.knotPolicy_);
            result.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
            return result;
        }

        void ValidateJointResultShape(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result, int residuals) {
            REQUIRE(std::isfinite(spec.tolerance_) && spec.tolerance_ > 0.0, "QUOTE_RISK_TOLERANCE_INVALID");
            REQUIRE(result.domesticCurveBlock_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            REQUIRE(result.foreignCurveBlock_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            REQUIRE(result.basisCurve_, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            REQUIRE(result.converged_, "QUOTE_RISK_CALIBRATION_RESULT_NOT_CONVERGED");
            REQUIRE(static_cast<int>(result.marketRates_.size()) == residuals, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.modelRates_.size()) == residuals, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(static_cast<int>(result.residuals_.size()) == residuals, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateFiniteVector(result.marketRates_, "QUOTE_RISK_MARKET_RATES");
            ValidateFiniteVector(result.modelRates_, "QUOTE_RISK_MODEL_RATES");
            ValidateFiniteVector(result.residuals_, "QUOTE_RISK_RESIDUALS");
        }

        void ValidateJointApproximateFlags(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result) {
            const bool approximate = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            REQUIRE(result.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.xccyDiagnostics_.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            for (const auto& diagnostics : result.domesticDiagnostics_)
                REQUIRE(diagnostics.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            for (const auto& diagnostics : result.foreignDiagnostics_)
                REQUIRE(diagnostics.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateJointFxForwardCurve(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationResult_& result) {
            REQUIRE(result.fxForwardCurve_.pair_ == spec.pair_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.fxForwardCurve_.dates_ == spec.basis_.knotDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.fxForwardCurve_.forwards_.size() == spec.basis_.knotDates_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateFiniteVector(result.fxForwardCurve_.forwards_, "QUOTE_RISK_FX_FORWARDS");
            if (spec.fixings_)
                REQUIRE(Jcs(FixingsJson(result.fixings_)) == Jcs(FixingsJson(spec.fixings_)), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateXccyMarketHeader(const CrossCurrencyMarket_& xccy,
                                      const DateTime_& valuationTime,
                                      const CurrencyPair_& pair,
                                      const Ccy_& collateralCurrency,
                                      double fxSpot,
                                      const char* error) {
            REQUIRE(xccy.ValuationTime() == valuationTime, error);
            REQUIRE(xccy.DomesticCcy() == pair.domestic_, error);
            REQUIRE(xccy.ForeignCcy() == pair.foreign_, error);
            REQUIRE(xccy.CollateralCurrency() == collateralCurrency, error);
            REQUIRE(xccy.FxSpot() == fxSpot, error);
            REQUIRE(xccy.BasisCurve(), error);
        }

        void ValidateJointBoundMarket(const JointXccyCalibrationResult_& result, const CrossCurrencyMarket_& xccy) {
            REQUIRE(Jcs(CurveBlockJson(xccy.DomesticBlock())) == Jcs(CurveBlockJson(*result.domesticCurveBlock_)),
                    "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(CurveBlockJson(xccy.ForeignBlock())) == Jcs(CurveBlockJson(*result.foreignCurveBlock_)),
                    "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(StorableJson(*xccy.BasisCurve())) == Jcs(StorableJson(*result.basisCurve_)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(FixingsJson(xccy.Fixings())) == Jcs(FixingsJson(result.fixings_)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
        }

        void ValidateJointBoundComponents(const JointXccyCalibrationSpec_& spec,
                                          const JointXccyCalibrationResult_& result,
                                          const RateQuoteRiskAxis_& axis,
                                          const RatePricingMarket_& market,
                                          const RateQuoteRiskProvenanceConfig_& config) {
            for (const auto& range : axis.parameterRanges_) {
                const DiscountCurve_& solvedCurve = JointResultCurve(range.blockKey_, spec, result);
                const String_& component = config.componentKeyByParameterBlock_.at(range.blockKey_);
                const auto found = market.curveComponents_.find(component);
                REQUIRE(found != market.curveComponents_.end(), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
                REQUIRE(found->second, "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
                REQUIRE(Jcs(StorableJson(*found->second)) == Jcs(StorableJson(solvedCurve)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            }
        }

        void ValidateJointPairing(const JointXccyCalibrationSpec_& spec,
                                  const JointXccyCalibrationResult_& result,
                                  const JointXccyCalibrationOptions_& options,
                                  const RateQuoteRiskAxis_& axis,
                                  const RatePricingMarket_& market,
                                  const RateQuoteRiskProvenanceConfig_& config) {
            const int parameters = static_cast<int>(axis.parameters_.size());
            const int residuals = static_cast<int>(axis.quotes_.size());
            ValidateJointResultShape(spec, result, residuals);
            ValidateJointResultTopology(spec, result);
            ValidateJointApproximateFlags(spec, result);
            const bool approximate = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            ValidateEffectiveInverse(result.effJacobianInverse_, !options.computeEffJacobianInverse_ || approximate, parameters, residuals);
            ValidateJointFxForwardCurve(spec, result);

            ValidateConfig(config, axis.parameterRanges_);
            REQUIRE(market.valuationTime_ == spec.valuationTime_, "QUOTE_RISK_SPEC_MARKET_MISMATCH");
            REQUIRE(market.xccyMarket_, "QUOTE_RISK_SPEC_MARKET_MISMATCH");
            const auto& xccy = *market.xccyMarket_;
            ValidateXccyMarketHeader(xccy, spec.valuationTime_, spec.pair_, spec.collateralCurrency_, spec.fxSpot_,
                                     "QUOTE_RISK_SPEC_MARKET_MISMATCH");
            ValidateJointBoundMarket(result, xccy);
            ValidateJointBoundComponents(spec, result, axis, market, config);
        }

        const DiscountCurve_& StagedBasisCurve(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationResult_& result) {
            const auto found = result.basisCurves_.find(spec.basisPair_);
            REQUIRE(found != result.basisCurves_.end() && found->second, "QUOTE_RISK_CALIBRATION_RESULT_CURVE_EMPTY");
            REQUIRE(result.basisCurves_.size() == 1, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            return *found->second;
        }

        String_ ExpectedStagedInverseAvailability(const CrossCurrencyCalibrationOptions_& options, bool approximate) {
            if (!options.computeEffJacobianInverse_)
                return "not_requested";
            return approximate ? String_("not_available_for_mode") : String_("available");
        }

        void ValidateStagedInverseMetadata(const CrossCurrencyCalibrationDiagnostics_& diagnostics,
                                           const CrossCurrencyCalibrationOptions_& options,
                                           bool approximate) {
            const String_ expected = ExpectedStagedInverseAvailability(options, approximate);
            REQUIRE(diagnostics.effJacobianInverseAvailability_ == expected, "QUOTE_RISK_EFFECTIVE_INVERSE_AVAILABILITY_MISMATCH");
            REQUIRE(diagnostics.effJacobianInverseScaling_ == "solver_scaled", "QUOTE_RISK_EFFECTIVE_INVERSE_SCALING_MISMATCH");
            REQUIRE((expected == "available") == !diagnostics.effJacobianInverse_.Empty(), "QUOTE_RISK_EFFECTIVE_INVERSE_AVAILABILITY_MISMATCH");
        }

        void ValidateStagedDiagnostics(const CrossCurrencyCalibrationSpec_& spec,
                                       const CrossCurrencyCalibrationResult_& result,
                                       const CrossCurrencyCalibrationOptions_& options,
                                       const RateQuoteRiskAxis_& axis) {
            const bool approximate = spec.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE;
            REQUIRE(result.diagnostics_.usedApproximateFit_ == approximate, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.diagnostics_.residualTolerance_ == spec.tolerance_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateFiniteVector(result.diagnostics_.marketRates_, "QUOTE_RISK_MARKET_RATES");
            ValidateFiniteVector(result.diagnostics_.modelRates_, "QUOTE_RISK_MODEL_RATES");
            ValidateFiniteVector(result.diagnostics_.residuals_, "QUOTE_RISK_RESIDUALS");
            ValidateStagedInverseMetadata(result.diagnostics_, options, approximate);
            ValidateEffectiveInverse(result.diagnostics_.effJacobianInverse_, !options.computeEffJacobianInverse_ || approximate,
                                     static_cast<int>(axis.parameters_.size()), static_cast<int>(axis.quotes_.size()));
        }

        void ValidateStagedFxForwardCurve(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationResult_& result) {
            REQUIRE(result.fxForwardCurve_.pair_ == spec.basisPair_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.fxForwardCurve_.dates_ == spec.knotDates_, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(result.fxForwardCurve_.forwards_.size() == spec.knotDates_.size(), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            ValidateFiniteVector(result.fxForwardCurve_.forwards_, "QUOTE_RISK_FX_FORWARDS");
        }

        void ValidateStagedResultMarket(const CrossCurrencyCalibrationSpec_& spec,
                                        const CrossCurrencyCalibrationResult_& result,
                                        const DiscountCurve_& basis,
                                        const DateTime_& valuationTime,
                                        const Ccy_& collateralCurrency) {
            ValidateXccyMarketHeader(result.market_, valuationTime, spec.basisPair_, collateralCurrency, spec.fxSpot_,
                                     "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(Jcs(CurveBlockJson(result.market_.DomesticBlock())) == Jcs(CurveBlockJson(*spec.domesticCurveBlock_)),
                    "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(Jcs(CurveBlockJson(result.market_.ForeignBlock())) == Jcs(CurveBlockJson(*spec.foreignCurveBlock_)),
                    "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            REQUIRE(Jcs(StorableJson(*result.market_.BasisCurve())) == Jcs(StorableJson(basis)), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
            if (spec.fixings_)
                REQUIRE(Jcs(FixingsJson(result.market_.Fixings())) == Jcs(FixingsJson(spec.fixings_)), "QUOTE_RISK_SPEC_RESULT_MISMATCH");
        }

        void ValidateStagedBoundMarket(const CrossCurrencyCalibrationSpec_& spec,
                                       const CrossCurrencyCalibrationResult_& result,
                                       const DiscountCurve_& basis,
                                       const DateTime_& valuationTime,
                                       const Ccy_& collateralCurrency,
                                       const CrossCurrencyMarket_& xccy) {
            ValidateXccyMarketHeader(xccy, valuationTime, spec.basisPair_, collateralCurrency, spec.fxSpot_,
                                     "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(CurveBlockJson(xccy.DomesticBlock())) == Jcs(CurveBlockJson(*spec.domesticCurveBlock_)),
                    "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(CurveBlockJson(xccy.ForeignBlock())) == Jcs(CurveBlockJson(*spec.foreignCurveBlock_)),
                    "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(StorableJson(*xccy.BasisCurve())) == Jcs(StorableJson(basis)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
            REQUIRE(Jcs(FixingsJson(xccy.Fixings())) == Jcs(FixingsJson(result.market_.Fixings())), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
        }

        void ValidateStagedBoundComponent(const RateQuoteRiskAxis_& axis,
                                          const RatePricingMarket_& market,
                                          const RateQuoteRiskProvenanceConfig_& config,
                                          const DiscountCurve_& basis) {
            const String_& component = config.componentKeyByParameterBlock_.at(axis.parameterRanges_.front().blockKey_);
            const auto found = market.curveComponents_.find(component);
            REQUIRE(found != market.curveComponents_.end(), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
            REQUIRE(found->second, "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
            REQUIRE(Jcs(StorableJson(*found->second)) == Jcs(StorableJson(basis)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");
        }

        void ValidateStagedPairing(const CrossCurrencyCalibrationSpec_& spec,
                                   const CrossCurrencyCalibrationResult_& result,
                                   const CrossCurrencyCalibrationOptions_& options,
                                   const RateQuoteRiskAxis_& axis,
                                   const RatePricingMarket_& market,
                                   const RateQuoteRiskProvenanceConfig_& config) {
            REQUIRE(std::isfinite(spec.tolerance_) && spec.tolerance_ > 0.0, "QUOTE_RISK_TOLERANCE_INVALID");
            REQUIRE(spec.domesticCurveBlock_, "QUOTE_RISK_SPEC_CURVE_EMPTY");
            REQUIRE(spec.foreignCurveBlock_, "QUOTE_RISK_SPEC_CURVE_EMPTY");
            const DiscountCurve_& basis = StagedBasisCurve(spec, result);
            ValidateUnlayeredSolvedCurve(basis, StagedBasisDefinition(spec));
            const DateTime_ valuationTime = StagedValuationTime(spec);
            const Ccy_ collateralCurrency = StagedCollateralCurrency(spec);
            ValidateStagedDiagnostics(spec, result, options, axis);
            ValidateStagedFxForwardCurve(spec, result);
            ValidateStagedResultMarket(spec, result, basis, valuationTime, collateralCurrency);

            ValidateConfig(config, axis.parameterRanges_);
            REQUIRE(market.valuationTime_ == valuationTime, "QUOTE_RISK_SPEC_MARKET_MISMATCH");
            REQUIRE(market.xccyMarket_, "QUOTE_RISK_SPEC_MARKET_MISMATCH");
            const auto& xccy = *market.xccyMarket_;
            ValidateStagedBoundMarket(spec, result, basis, valuationTime, collateralCurrency, xccy);
            ValidateStagedBoundComponent(axis, market, config, basis);
        }

        RateQuoteRiskAxis_ SingleAxis(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& result) {
            const ExecutionSingleKnotIdentity_ identity = InspectCurveCalibrationExecutionIdentity(spec);
            RateQuoteRiskAxis_ axis;
            axis.scheme_ = RateQuoteRiskAxisFingerprintScheme();
            axis.parameterRanges_.push_back({spec.curveName_, 0, static_cast<int>(identity.freeParameters_.size())});
            axis.residualRanges_.push_back({spec.curveName_, 0, static_cast<int>(spec.instruments_.size())});
            for (int i = 0; i < static_cast<int>(identity.freeParameters_.size()); ++i) {
                const auto& parameter = identity.freeParameters_[i];
                axis.parameters_.push_back({spec.curveName_, i, i, parameter.date_, parameter.component_});
            }
            for (int i = 0; i < static_cast<int>(result.diagnostics_.instrumentNames_.size()); ++i)
                axis.quotes_.push_back({spec.curveName_, i, i, result.diagnostics_.instrumentNames_[i], "DECIMAL_QUOTE"});
            axis.fingerprint_ = Fingerprint(AxisJson("SINGLE_CURVE", axis));
            return axis;
        }

        Json_ BindingsJson(const std::map<String_, String_>& bindings) {
            Json_ result = Json_::Array();
            for (const auto& binding : bindings) {
                Json_ entry = Json_::Object();
                entry.object_["componentKey"] = Json_::String(binding.second);
                entry.object_["parameterBlock"] = Json_::String(binding.first);
                result.array_.push_back(std::move(entry));
            }
            return result;
        }

        Json_ StateRecord(const String_& kind,
                          const RateQuoteRiskAxis_& axis,
                          const std::map<String_, String_>& bindings,
                          const String_& calibrationId,
                          const RatePricingMarket_& market,
                          Json_ options,
                          Json_ calibrationResult,
                          const String_& reason,
                          Json_ spec,
                          const Matrix_<>& effectiveInverse,
                          double tolerance) {
            Json_ state = Json_::Object();
            state.object_["axisFingerprint"] = Json_::String(axis.fingerprint_);
            state.object_["bindings"] = BindingsJson(bindings);
            state.object_["calibrationId"] = Json_::String(calibrationId);
            state.object_["effectiveInverse"] = MatrixJson(effectiveInverse);
            state.object_["effectiveInverseScaling"] = Json_::String("solver_scaled");
            state.object_["kind"] = Json_::String(kind);
            state.object_["market"] = MarketJson(market, bindings);
            state.object_["options"] = std::move(options);
            state.object_["parameterRanges"] = RangesJson(axis.parameterRanges_);
            state.object_["reason"] = Json_::String(reason);
            state.object_["residualRanges"] = RangesJson(axis.residualRanges_);
            state.object_["result"] = std::move(calibrationResult);
            state.object_["scheme"] = Json_::String(RateQuoteRiskStateFingerprintScheme());
            state.object_["spec"] = std::move(spec);
            state.object_["tolerance"] = Json_::Number(tolerance);
            return state;
        }

        RateQuoteRiskComponentState_ ComponentState(const String_& componentKey, const DiscountCurve_& curve, const RatePricingMarket_& market) {
            Json_ record = Json_::Object();
            record.object_["componentKey"] = Json_::String(componentKey);
            record.object_["curve"] = StorableJson(curve);
            record.object_["fixings"] = FixingsJson(market.fixings_);
            record.object_["resultCurrency"] = Json_::String(market.resultCurrency_.String());
            record.object_["scheme"] = Json_::String(RateQuoteRiskStateFingerprintScheme());
            record.object_["valuationTime"] = DateTimeJson(market.valuationTime_);
            record.object_["xccyMarket"] = XccyMarketJson(market);
            return {componentKey, Fingerprint(record)};
        }
    } // namespace

    struct RateQuoteRiskProvenance_::Data_ {
        String_ kind_;
        bool available_ = false;
        String_ reason_;
        String_ calibrationId_;
        std::map<String_, String_> bindings_;
        RateQuoteRiskAxis_ axis_;
        RateQuoteRiskState_ state_;
        Matrix_<> effectiveInverse_;
        double tolerance_ = 0.0;
    };

    RateQuoteRiskProvenance_::RateQuoteRiskProvenance_(const std::shared_ptr<const Data_>& data) : data_(data) {
        REQUIRE(data_, "QUOTE_RISK_PROVENANCE_DATA_EMPTY");
    }

    const String_& RateQuoteRiskProvenance_::Kind() const { return data_->kind_; }
    bool RateQuoteRiskProvenance_::Available() const { return data_->available_; }
    const String_& RateQuoteRiskProvenance_::Reason() const { return data_->reason_; }
    const String_& RateQuoteRiskProvenance_::CalibrationId() const { return data_->calibrationId_; }
    const std::map<String_, String_>& RateQuoteRiskProvenance_::ComponentKeyByParameterBlock() const { return data_->bindings_; }
    const RateQuoteRiskAxis_& RateQuoteRiskProvenance_::Axis() const { return data_->axis_; }
    const RateQuoteRiskState_& RateQuoteRiskProvenance_::State() const { return data_->state_; }
    const Matrix_<>& RateQuoteRiskProvenance_::EffectiveInverse() const { return data_->effectiveInverse_; }
    double RateQuoteRiskProvenance_::Tolerance() const { return data_->tolerance_; }

    const String_& RateQuoteRiskAxisFingerprintScheme() {
        static const String_ result("dal.quote-risk-axis/1+jcs+sha256");
        return result;
    }

    const String_& RateQuoteRiskStateFingerprintScheme() {
        static const String_ result("dal.quote-risk-state/1+jcs+sha256");
        return result;
    }

    RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(const CurveCalibrationSpec_& spec,
                                                                 const CurveCalibrationResult_& result,
                                                                 const CurveCalibrationOptions_& options,
                                                                 const RatePricingMarket_& boundMarket,
                                                                 const RateQuoteRiskProvenanceConfig_& config) {
        const CurveCalibrationSpec_ normalizedSpec = NormalizeSingleSpec(spec);
        REQUIRE(std::isfinite(normalizedSpec.tolerance_) && normalizedSpec.tolerance_ > 0.0, "QUOTE_RISK_TOLERANCE_INVALID");
        const RateQuoteRiskAxis_ axis = SingleAxis(normalizedSpec, result);
        ValidateConfig(config, axis.parameterRanges_);
        ValidateSinglePairing(normalizedSpec, result, options, static_cast<int>(axis.parameters_.size()));
        REQUIRE(boundMarket.valuationTime_.Date() == normalizedSpec.today_, "QUOTE_RISK_SPEC_MARKET_MISMATCH");

        const String_ componentKey = config.componentKeyByParameterBlock_.at(normalizedSpec.curveName_);
        const auto bound = boundMarket.curveComponents_.find(componentKey);
        REQUIRE(bound != boundMarket.curveComponents_.end() && bound->second, "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISSING");
        REQUIRE(Jcs(StorableJson(*bound->second)) == Jcs(StorableJson(*result.curve_)), "QUOTE_RISK_BOUND_MARKET_COMPONENT_MISMATCH");

        auto data = std::make_shared<RateQuoteRiskProvenance_::Data_>();
        data->kind_ = "SINGLE_CURVE";
        data->reason_ = AvailabilityReason(normalizedSpec.solveMode_, options.computeEffJacobianInverse_, result.diagnostics_.effJacobianInverse_);
        data->available_ = data->reason_.empty();
        data->calibrationId_ = config.calibrationId_;
        data->bindings_ = config.componentKeyByParameterBlock_;
        data->axis_ = axis;
        data->state_.scheme_ = RateQuoteRiskStateFingerprintScheme();
        data->state_.components_.push_back(ComponentState(componentKey, *bound->second, boundMarket));
        data->tolerance_ = normalizedSpec.tolerance_;
        if (data->available_)
            data->effectiveInverse_ = result.diagnostics_.effJacobianInverse_;

        data->state_.fingerprint_ = Fingerprint(StateRecord(
            data->kind_, data->axis_, data->bindings_, data->calibrationId_, boundMarket, SingleOptionsJson(options), SingleResultJson(result),
            data->reason_, SingleSpecJson(normalizedSpec), result.diagnostics_.effJacobianInverse_, data->tolerance_));
        return RateQuoteRiskProvenance_(data);
    }

    RateQuoteRiskProvenance_ BuildJointXccyQuoteRiskProvenance(const JointXccyCalibrationSpec_& spec,
                                                               const JointXccyCalibrationResult_& result,
                                                               const JointXccyCalibrationOptions_& options,
                                                               const RatePricingMarket_& boundMarket,
                                                               const RateQuoteRiskProvenanceConfig_& config) {
        const RateQuoteRiskAxis_ axis = JointAxis(spec, result);
        ValidateJointPairing(spec, result, options, axis, boundMarket, config);

        auto data = std::make_shared<RateQuoteRiskProvenance_::Data_>();
        data->kind_ = "JOINT_XCCY";
        data->reason_ = AvailabilityReason(spec.solveMode_, options.computeEffJacobianInverse_, result.effJacobianInverse_);
        data->available_ = data->reason_.empty();
        data->calibrationId_ = config.calibrationId_;
        data->bindings_ = config.componentKeyByParameterBlock_;
        data->axis_ = axis;
        data->state_.scheme_ = RateQuoteRiskStateFingerprintScheme();
        for (const auto& range : axis.parameterRanges_) {
            const String_& component = data->bindings_.at(range.blockKey_);
            data->state_.components_.push_back(ComponentState(component, *boundMarket.curveComponents_.at(component), boundMarket));
        }
        data->tolerance_ = spec.tolerance_;
        if (data->available_)
            data->effectiveInverse_ = result.effJacobianInverse_;

        data->state_.fingerprint_ =
            Fingerprint(StateRecord(data->kind_, data->axis_, data->bindings_, data->calibrationId_, boundMarket, JointOptionsJson(options),
                                    JointResultJson(result), data->reason_, JointSpecJson(spec), result.effJacobianInverse_, data->tolerance_));
        return RateQuoteRiskProvenance_(data);
    }

    RateQuoteRiskProvenance_ BuildStagedXccyBasisQuoteRiskProvenance(const CrossCurrencyCalibrationSpec_& spec,
                                                                     const CrossCurrencyCalibrationResult_& result,
                                                                     const CrossCurrencyCalibrationOptions_& options,
                                                                     const RatePricingMarket_& boundMarket,
                                                                     const RateQuoteRiskProvenanceConfig_& config) {
        const RateQuoteRiskAxis_ axis = StagedAxis(spec, result);
        ValidateStagedPairing(spec, result, options, axis, boundMarket, config);

        auto data = std::make_shared<RateQuoteRiskProvenance_::Data_>();
        data->kind_ = "STAGED_XCCY_BASIS";
        data->reason_ = AvailabilityReason(spec.solveMode_, options.computeEffJacobianInverse_, result.diagnostics_.effJacobianInverse_);
        data->available_ = data->reason_.empty();
        data->calibrationId_ = config.calibrationId_;
        data->bindings_ = config.componentKeyByParameterBlock_;
        data->axis_ = axis;
        data->state_.scheme_ = RateQuoteRiskStateFingerprintScheme();
        const String_& component = data->bindings_.at(axis.parameterRanges_.front().blockKey_);
        data->state_.components_.push_back(ComponentState(component, *boundMarket.curveComponents_.at(component), boundMarket));
        data->tolerance_ = spec.tolerance_;
        if (data->available_)
            data->effectiveInverse_ = result.diagnostics_.effJacobianInverse_;

        data->state_.fingerprint_ = Fingerprint(StateRecord(data->kind_, data->axis_, data->bindings_, data->calibrationId_, boundMarket,
                                                            StagedOptionsJson(options), StagedResultJson(result), data->reason_, StagedSpecJson(spec),
                                                            result.diagnostics_.effJacobianInverse_, data->tolerance_));
        return RateQuoteRiskProvenance_(data);
    }
} // namespace Dal
