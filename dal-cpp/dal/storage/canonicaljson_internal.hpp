#pragma once

#include <map>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration CanonicalJsonKind
    Internal canonical JSON value tag
switchable
alternative NIL
alternative BOOLEAN
alternative NUMBER
alternative STRING
alternative ARRAY
alternative OBJECT
-IF-------------------------------------------------------------------------*/
namespace Dal {
#include <dal/auto/MG_CanonicalJsonKind_enum.hpp>
} // namespace Dal
namespace Dal::CanonicalJson {
    struct Json_ {
        using Kind_ = CanonicalJsonKind_;

        Kind_ kind_ = Kind_::Value_::NIL;
        bool boolean_ = false;
        double number_ = 0.0;
        std::string string_;
        std::vector<Json_> array_;
        std::map<std::string, Json_> object_;

        static Json_ Boolean(bool value) {
            Json_ result;
            result.kind_ = Kind_::Value_::BOOLEAN;
            result.boolean_ = value;
            return result;
        }

        static Json_ Number(double value) {
            REQUIRE(std::isfinite(value), "QUOTE_RISK_NON_FINITE_FINGERPRINT_INPUT");
            Json_ result;
            result.kind_ = Kind_::Value_::NUMBER;
            result.number_ = value;
            return result;
        }

        static Json_ String(const String_& value) {
            Json_ result;
            result.kind_ = Kind_::Value_::STRING;
            result.string_.assign(value.data(), value.size());
            return result;
        }

        static Json_ String(const char* value) { return String(String_(value ? value : "")); }

        static Json_ Array() {
            Json_ result;
            result.kind_ = Kind_::Value_::ARRAY;
            return result;
        }

        static Json_ Object() {
            Json_ result;
            result.kind_ = Kind_::Value_::OBJECT;
            return result;
        }
    };

    bool ValidUtf8(const std::string& value);
    std::string JcsNumber(double value);
    std::string Jcs(const Json_& value);
    String_ FingerprintCanonical(const std::string& canonical);
    String_ Fingerprint(const Json_& value);
    Json_ FromRapidJson(const rapidjson::Value& value);
} // namespace Dal::CanonicalJson
