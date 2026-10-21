#pragma once

#include <algorithm>
#include <climits>
#include <dal/calibration/protocol.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal::CalibrationInternal {
    using CanonicalJson::Json_;
    using JsonKind_ = CanonicalJsonKind_::Value_;
    using CanonicalJson::Fingerprint;
    using CanonicalJson::Jcs;

    [[noreturn]] void
    Fail(const String_& code, const String_& field, const String_& expected, const String_& actual = "", CalibrationDiagnostic_ context = {});
    inline void Check(bool valid,
                      const String_& code,
                      const String_& field,
                      const String_& expected,
                      const String_& actual = "",
                      const CalibrationDiagnostic_& context = {}) {
        if (!valid)
            Fail(code, field, expected, actual, context);
    }
    inline std::string Bytes(const String_& value) { return std::string(value.data(), value.size()); }
    inline String_ OrdinalText(size_t index) { return String_(std::to_string(index)); }
    bool ValidId(const String_& value);
    bool ValidKey(const String_& value);
    bool ValidFingerprint(const String_& value);
    String_ NumberText(double value);
    void Text(const String_& value, const String_& code, const String_& field, const CalibrationDiagnostic_& context = {});
    Date_ ParseDate(const String_& value, const String_& code, const String_& field, const CalibrationDiagnostic_& context = {});
    inline double Normalize(double value) { return value == 0. ? 0. : value; }

    template <class E_> String_ Token(const E_& value, const String_& code, const String_& field, const CalibrationDiagnostic_& context = {}) {
        const char* token = value.String();
        Check(token != nullptr, code, field, "a supported enum value", "", context);
        return String_(token);
    }
    template <class E_> E_ Enum(const String_& token, const String_& code, const String_& field, const CalibrationDiagnostic_& context = {}) {
        try {
            E_ result(token);
            Check(Bytes(Token(result, code, field, context)) == Bytes(token), code, field, "an exact enum token", token, context);
            return result;
        } catch (const CalibrationError_&) {
            throw;
        } catch (const Exception_&) {
            Fail(code, field, "a supported enum token", token, context);
        }
    }
    inline Json_ Str(const String_& value) { return Json_::String(value); }
    inline Json_ Num(double value) { return Json_::Number(value); }
    inline Json_ Bool(bool value) { return Json_::Boolean(value); }
    inline Json_ Obj(std::initializer_list<std::pair<const std::string, Json_>> fields) {
        Json_ result = Json_::Object();
        result.object_ = fields;
        return result;
    }
    template <class T_, class F_> Json_ Array(const Vector_<T_>& values, const F_& convert) {
        Json_ result = Json_::Array();
        for (const auto& value : values)
            result.array_.push_back(convert(value));
        return result;
    }
    template <class T_, class F_> Json_ Optional(const std::optional<T_>& value, const F_& convert) { return value ? convert(*value) : Json_(); }
    Json_ Parse(const String_& input);
    void Fields(const Json_& value, const String_& field, std::initializer_list<const char*> names);
    const Json_& Field(const Json_& value, const char* name);
    String_ ReadString(const Json_& value, const String_& path);
    double ReadNumber(const Json_& value, const String_& path);
    bool ReadBool(const Json_& value, const String_& path);
    const std::vector<Json_>& ReadArray(const Json_& value, const String_& path);
    template <class T_, class F_> std::optional<T_> ReadOptional(const Json_& value, const String_& path, const F_& convert) {
        return value.kind_ == JsonKind_::NIL ? std::nullopt : std::optional<T_>(convert(value, path));
    }
    Json_ QuoteJson(const CalibrationMarketQuote_& quote);
} // namespace Dal::CalibrationInternal
