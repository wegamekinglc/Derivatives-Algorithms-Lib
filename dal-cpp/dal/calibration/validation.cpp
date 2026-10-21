#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <charconv>
#include <string_view>

#include <dal/calibration/validation_internal.hpp>

namespace Dal::CalibrationInternal {
    namespace {
        bool Upper(char c) { return c >= 'A' && c <= 'Z'; }
        bool Digit(char c) { return c >= '0' && c <= '9'; }
        bool Alnum(char c) { return Upper(c) || (c >= 'a' && c <= 'z') || Digit(c); }
        bool KeyInitial(char c) { return Upper(c) || c == '_'; }
        String_ SafeText(const String_& value) {
            if (!CanonicalJson::ValidUtf8(Bytes(value)))
                return "<invalid UTF-8>";
            String_ result;
            for (char c : value)
                result += c == '\0' ? String_("\\u0000") : String_(1, c);
            return result;
        }
    } // namespace
    [[noreturn]] void
    Fail(const String_& code, const String_& field, const String_& expected, const String_& actual, CalibrationDiagnostic_ context) {
        context.code_ = SafeText(code);
        context.field_ = field;
        context.expected_ = expected;
        context.actual_ = actual;
        for (auto* text : {&context.field_, &context.expected_, &context.actual_, &context.observationId_, &context.instrumentId_, &context.quoteId_,
                           &context.measureId_, &context.parameterKey_})
            if (*text)
                *text = SafeText(**text);
        context.message_ = context.code_ + ": " + *context.field_ + "; expected " + *context.expected_;
        if (!context.actual_->empty())
            context.message_ += "; actual " + *context.actual_;
        const Handle_<CalibrationReport_> report(new CalibrationReport_(context.stage_, {}, {context}));
        throw CalibrationError_(__FILE__, __LINE__, __func__, std::make_shared<const CalibrationFailure_>(CalibrationFailure_{context, report, {}}));
    }
    bool ValidId(const String_& value) {
        if (value.empty() || value.size() > 128)
            return false;
        return Alnum(value.front()) && std::all_of(value.begin(), value.end(),
                                                   [](char c) { return Alnum(c) || std::string_view("_.:/-").find(c) != std::string_view::npos; });
    }
    bool ValidKey(const String_& value) {
        if (value.empty() || !KeyInitial(value.front()))
            return false;
        return std::all_of(value.begin(), value.end(), [](char c) { return KeyInitial(c) || Digit(c); });
    }
    bool ValidFingerprint(const String_& value) {
        const auto s = Bytes(value);
        return s.size() == 71 && s.substr(0, 7) == "sha256:" &&
               std::all_of(s.begin() + 7, s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
    }
    String_ NumberText(double value) {
        if (std::isnan(value))
            return "NaN";
        if (std::isinf(value))
            return value > 0. ? "Infinity" : "-Infinity";
        return String_(CanonicalJson::JcsNumber(value));
    }
    void Text(const String_& value, const String_& code, const String_& field, const CalibrationDiagnostic_& context) {
        Check(value.find('\0') == String_::npos && CanonicalJson::ValidUtf8(Bytes(value)), code, field, "UTF-8 text without NUL", "", context);
    }
    Date_ ParseDate(const String_& value, const String_& code, const String_& field, const CalibrationDiagnostic_& context) {
        const std::string text = Bytes(value);
        Check(text.size() == 10 && text[4] == '-' && text[7] == '-', code, field, "YYYY-MM-DD in the core Date range", value, context);
        int parts[3] = {};
        const int starts[] = {0, 5, 8}, ends[] = {4, 7, 10};
        for (int i = 0; i < 3; ++i) {
            const auto parsed = std::from_chars(text.data() + starts[i], text.data() + ends[i], parts[i]);
            Check(parsed.ec == std::errc() && parsed.ptr == text.data() + ends[i], code, field, "YYYY-MM-DD", value, context);
        }
        try {
            Date_ result(parts[0], parts[1], parts[2]);
            Check(result.IsValid() && Bytes(Date::ToString(result)) == text, code, field, "a valid core date", value, context);
            return result;
        } catch (const CalibrationError_&) {
            throw;
        } catch (const Exception_&) {
            Fail(code, field, "a valid core date", value, context);
        }
    }
    namespace {
        String_ PointerKey(const std::string& key) {
            String_ result;
            for (char c : key)
                result += c == '~' ? "~0" : c == '/' ? "~1" : String_(1, c);
            return result;
        }
        Json_ Decode(const rapidjson::Value& value, const String_& path) {
            if (value.IsObject()) {
                Json_ result = Json_::Object();
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                    const std::string key(member->name.GetString(), member->name.GetStringLength());
                    const String_ child = path + "/" + PointerKey(key);
                    CalibrationDiagnostic_ context;
                    context.stage_ = CalibrationStage_::Value_::Load;
                    Text(String_(key), "InvalidCalibrationArchive", path, context);
                    Check(result.object_.find(key) == result.object_.end(), "InvalidCalibrationArchive", child, "unique JSON property", "", context);
                    result.object_.emplace(key, Decode(member->value, child));
                }
                return result;
            }
            if (value.IsArray()) {
                Json_ result = Json_::Array();
                for (const auto& child : value.GetArray())
                    result.array_.push_back(Decode(child, path + "/" + OrdinalText(result.array_.size())));
                return result;
            }
            return CanonicalJson::FromRapidJson(value);
        }
        CalibrationDiagnostic_ LoadContext() {
            CalibrationDiagnostic_ result;
            result.stage_ = CalibrationStage_::Value_::Load;
            return result;
        }
    } // namespace
    Json_ Parse(const String_& input) {
        Text(input, "InvalidCalibrationArchive", "", LoadContext());
        Check(input.size() <= 50U * 1024U * 1024U, "InvalidCalibrationArchive", "", "at most 50 MiB", "", LoadContext());
        rapidjson::Document document;
        document.Parse<rapidjson::kParseFullPrecisionFlag | rapidjson::kParseValidateEncodingFlag>(input.data(), input.size());
        Check(!document.HasParseError(), "InvalidCalibrationArchive", "", "valid JSON", "", LoadContext());
        return Decode(document, "");
    }
    void Fields(const Json_& value, const String_& path, std::initializer_list<const char*> names) {
        Check(value.kind_ == JsonKind_::OBJECT, "InvalidCalibrationArchive", path, "object", "", LoadContext());
        for (const auto& item : value.object_) {
            const bool known = std::any_of(names.begin(), names.end(), [&](const char* name) { return item.first == name; });
            Check(known, "InvalidCalibrationArchive", path + "/" + PointerKey(item.first), "a schema field", "", LoadContext());
        }
        for (const auto* name : names)
            Check(value.object_.count(name) != 0, "InvalidCalibrationArchive", path + "/" + name, "required field", "", LoadContext());
    }
    const Json_& Field(const Json_& value, const char* name) { return value.object_.at(name); }
    String_ ReadString(const Json_& value, const String_& path) {
        Check(value.kind_ == JsonKind_::STRING, "InvalidCalibrationArchive", path, "string", "", LoadContext());
        const String_ result(value.string_);
        Text(result, "InvalidCalibrationArchive", path, LoadContext());
        return result;
    }
    double ReadNumber(const Json_& value, const String_& path) {
        Check(value.kind_ == JsonKind_::NUMBER && std::isfinite(value.number_), "InvalidCalibrationArchive", path, "finite number", "",
              LoadContext());
        return Normalize(value.number_);
    }
    bool ReadBool(const Json_& value, const String_& path) {
        Check(value.kind_ == JsonKind_::BOOLEAN, "InvalidCalibrationArchive", path, "boolean", "", LoadContext());
        return value.boolean_;
    }
    void CheckArray(const Json_& value, const String_& path) {
        Check(value.kind_ == JsonKind_::ARRAY && value.array_.size() <= INT_MAX, "InvalidCalibrationArchive", path, "array", "", LoadContext());
    }
} // namespace Dal::CalibrationInternal
