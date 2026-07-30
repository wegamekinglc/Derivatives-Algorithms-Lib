//
// Created by wegam on 2023/1/21.
//

#include <algorithm>
#include <cstdio>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <regex>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/writer.h>
#include <rapidjson/filereadstream.h>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/json.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/storage/archive.hpp>
#include <utility>

namespace Dal {
    namespace {
        const char* TYPE_LABEL = "~type";
        const char* TAG_LABEL = "$tag";
        const char* COLS_LABEL = "cols";
        const char* VALS_LABEL = "vals";
        using element_t = rapidjson::GenericValue<rapidjson::UTF8<>>;
        using allocator_t = rapidjson::GenericDocument<rapidjson::UTF8<>>::AllocatorType ;
        using rapidjson::Value;

        bool IsContinuation(unsigned char value) {
            return value >= 0x80 && value <= 0xBF;
        }

        // #lizard forgives -- the UTF-8 decoder intentionally enumerates every byte-class boundary.
        bool ValidUtf8(const char* src, std::size_t length, std::size_t* badOffset) {
            std::size_t index = 0;
            while (index < length) {
                const auto first = static_cast<unsigned char>(src[index]);
                if (first <= 0x7F) {
                    ++index;
                    continue;
                }
                if (first >= 0xC2 && first <= 0xDF && index + 1 < length &&
                    IsContinuation(static_cast<unsigned char>(src[index + 1]))) {
                    index += 2;
                    continue;
                }
                if (first >= 0xE0 && first <= 0xEF && index + 2 < length) {
                    const auto second = static_cast<unsigned char>(src[index + 1]);
                    const auto third = static_cast<unsigned char>(src[index + 2]);
                    const bool validSecond =
                        (first == 0xE0 && second >= 0xA0 && second <= 0xBF) ||
                        (first == 0xED && second >= 0x80 && second <= 0x9F) ||
                        ((first >= 0xE1 && first <= 0xEC) || (first >= 0xEE && first <= 0xEF)) &&
                            IsContinuation(second);
                    if (validSecond && IsContinuation(third)) {
                        index += 3;
                        continue;
                    }
                }
                if (first >= 0xF0 && first <= 0xF4 && index + 3 < length) {
                    const auto second = static_cast<unsigned char>(src[index + 1]);
                    const auto third = static_cast<unsigned char>(src[index + 2]);
                    const auto fourth = static_cast<unsigned char>(src[index + 3]);
                    const bool validSecond =
                        (first == 0xF0 && second >= 0x90 && second <= 0xBF) ||
                        (first == 0xF4 && second >= 0x80 && second <= 0x8F) ||
                        (first >= 0xF1 && first <= 0xF3 && IsContinuation(second));
                    if (validSecond && IsContinuation(third) && IsContinuation(fourth)) {
                        index += 4;
                        continue;
                    }
                }
                *badOffset = index;
                return false;
            }
            return true;
        }

        void ValidateStringBytes(const String_& value) {
            const auto nul = std::find(value.begin(), value.end(), '\0');
            REQUIRE(nul == value.end(), "ARCHIVE_STRING_NUL");
            std::size_t badOffset = 0;
            REQUIRE(ValidUtf8(value.data(), value.size(), &badOffset),
                    "ARCHIVE_STRING_INVALID_UTF8 at byte " + ToString(static_cast<int>(badOffset)));
        }

        void ValidateDecodedString(const element_t& value) {
            const auto* begin = value.GetString();
            const auto length = static_cast<std::size_t>(value.GetStringLength());
            REQUIRE(std::find(begin, begin + length, '\0') == begin + length, "ARCHIVE_STRING_NUL");
            std::size_t badOffset = 0;
            REQUIRE(
                ValidUtf8(begin, length, &badOffset),
                "ARCHIVE_STRING_INVALID_UNICODE at byte " + ToString(static_cast<int>(badOffset)));
        }

        void ValidateDecodedStrings(const element_t& value) {
            if (value.IsString()) {
                ValidateDecodedString(value);
                return;
            }
            if (value.IsArray()) {
                for (const auto& child : value.GetArray())
                    ValidateDecodedStrings(child);
                return;
            }
            if (value.IsObject()) {
                for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member) {
                    ValidateDecodedString(member->name);
                    ValidateDecodedStrings(member->value);
                }
            }
        }

        // #lizard forgives -- JSON escaping is an exhaustive character-class encoder.
        void WriteJsonString(std::ostream& dst, const String_& value) {
            ValidateStringBytes(value);
            static constexpr char HEX[] = "0123456789ABCDEF";
            dst.put('"');
            for (const auto raw : value) {
                const auto byte = static_cast<unsigned char>(raw);
                switch (byte) {
                case '"':
                    dst << "\\\"";
                    break;
                case '\\':
                    dst << "\\\\";
                    break;
                case '\b':
                    dst << "\\b";
                    break;
                case '\t':
                    dst << "\\t";
                    break;
                case '\n':
                    dst << "\\n";
                    break;
                case '\f':
                    dst << "\\f";
                    break;
                case '\r':
                    dst << "\\r";
                    break;
                default:
                    if (byte < 0x20)
                        dst << "\\u00" << HEX[(byte >> 4U) & 0x0FU] << HEX[byte & 0x0FU];
                    else
                        dst.put(raw);
                    break;
                }
            }
            dst.put('"');
        }

        void WriteJsonDouble(std::ostream& dst, double value) {
            REQUIRE(std::isfinite(value), "ARCHIVE_NUMBER_NON_FINITE");
            if (value == 0.0) {
                dst.put('0');
                return;
            }
            char buffer[64];
            const auto result =
                std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
                              std::numeric_limits<double>::max_digits10);
            REQUIRE(result.ec == std::errc(), "ARCHIVE_NUMBER_FORMAT_FAILED");
            dst.write(buffer, result.ptr - buffer);
        }

        rapidjson::GenericStringRef<char> LendToJSON(const String_& s) {
            return {s.c_str(), static_cast<rapidjson::SizeType>(s.size())};
        }
        struct XDocStore_ : Archive::Store_ {
            std::ostream& dst_;
            std::map<const Storable_*, String_>& sharedTags_;
            std::map<String_, std::shared_ptr<XDocStore_>> children_;
            String_ ownName_;
            bool empty_;

            XDocStore_(std::ostream& dst,
                       std::map<const Storable_*, String_>& tags,
                       XDocStore_* parent,
                       String_  own_name)
                : dst_(dst), sharedTags_(tags), ownName_(std::move(own_name)), empty_(true) {}

            // looks like tag has to be the toplevel attribute of an object node
            const char* Prep() const { return empty_ ? "{\n" : ",\n"; }
            void StoreRefTag(const String_& tag) {
                assert(empty_); // this should always be the first thing written
                dst_ << Prep();
                WriteJsonString(dst_, String_(TAG_LABEL));
                dst_ << ": ";
                WriteJsonString(dst_, tag);
                empty_ = false;
            }

            bool StoreRef(const Storable_* object) override {
                auto ot = sharedTags_.find(object);
                if (ot != sharedTags_.end()) {
                    StoreRefTag(ot->second);
                    return true;
                }
                auto tag = ToString(1 + static_cast<int>(sharedTags_.size()));
                sharedTags_.insert(make_pair(object, tag));
                StoreRefTag(tag);
                return false;
            }
            void SetType(const String_& type) override {
                dst_ << Prep();
                WriteJsonString(dst_, String_(TYPE_LABEL));
                dst_ << ": ";
                WriteJsonString(dst_, type);
                empty_ = false;
            }
            void Done() override { dst_ << '}'; }
            Store_& Child(const String_& name) override {
                std::shared_ptr<XDocStore_>& retval = children_[name];
                if (!retval) {
                    dst_ << Prep();
                    WriteJsonString(dst_, name);
                    dst_ << ": ";
                    retval = std::make_shared<XDocStore_>(dst_, sharedTags_, this, name);
                    empty_ = false;
                }
                return *retval;
            }

            XDocStore_& operator=(const double val) override {
                WriteJsonDouble(dst_, val);
                return *this;
            }

            XDocStore_& operator=(const int val) {
                dst_ << val;
                return *this;
            }
            XDocStore_& operator=(const bool val) {
                dst_ << String::FromBool(val);
                return *this;
            }
            XDocStore_& operator=(const String_& val) override {
                WriteJsonString(dst_, val);
                return *this;
            }
            XDocStore_& operator=(const Date_& val) override { operator=(Date::ToString(val)); return *this; }
            XDocStore_& operator=(const DateTime_& val) override { operator=(DateTime::ToString(val)); return *this; }
            XDocStore_& operator=(const Cell_& c) {
                if (Cell::IsBool(c))
                    operator=(Cell::ToBool(c));
                else if (Cell::IsDate(c))
                    operator=(Cell::ToDate(c));
                else if (Cell::IsDateTime(c))
                    operator=(Cell::ToDateTime(c));
                else if (Cell::IsString(c))
                    operator=(Cell::ToString(c));
                else if (Cell::IsDouble(c))
                    operator=(Cell::ToDouble(c));
                else if (Cell::IsInt(c))
                    operator=(Cell::ToInt(c));
                else if (Cell::IsEmpty(c))
                    operator=(String_());
                else
                    THROW("Internal error -- unhandled cell type");
                return *this;
            }

            template <class E_> void SetArray(const Vector_<E_>& val) {
                dst_ << "[";
                bool first = true;
                for (const auto& v : val) {
                    if (!first)
                        dst_ << ",";
                    first = false;
                    operator=(v);
                }
                dst_ << "]";
            }
            XDocStore_& operator=(const Vector_<>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<int>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<bool>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<String_>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<Date_>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<DateTime_>& val) override { SetArray(val); return *this; }
            XDocStore_& operator=(const Vector_<Cell_>& val) override { SetArray(val); return *this; }

            template <class E_> void SetMatrix(const Matrix_<E_>& val) {
                dst_ << "{ \"rows\": " << val.Rows() << ",\n\"cols\": " << val.Cols() << ",\n\"vals\": [";
                bool first = true;
                for (int ir = 0; ir < val.Rows(); ++ir)
                    for (const auto& v : val.Row(ir)) {
                        if (!first)
                            dst_ << ", ";
                        first = false;
                        operator=(v);
                    }
                dst_ << "]}";
            }
            XDocStore_& operator=(const Matrix_<>& val) override { SetMatrix(val); return *this; }
            XDocStore_& operator=(const Matrix_<String_>& val) override { SetMatrix(val); return *this; }
            XDocStore_& operator=(const Matrix_<Cell_>& val) override { SetMatrix(val); return *this; }
            XDocStore_& operator=(const Dictionary_& val) override { operator=(Dictionary::ToString(val)); return *this; }
        };

        template <class E_>
        auto AsVector(element_t& doc, const E_& extract) -> typename vector_of<decltype(extract(doc))>::type {
            REQUIRE(doc.IsArray(), "Can't get a vector value");
            const auto n = doc.Size();
            typename vector_of<decltype(extract(doc))>::type ret_val(n);
            for (auto ii = 0; ii < n; ++ii)
                ret_val[ii] = extract(doc[ii]);
            return ret_val;
        }
        template <class E_> Matrix_<E_> AsMatrix(int cols, const Vector_<E_>& vals) {
            REQUIRE(cols > 0 && !(vals.size() % cols), "Invalid number of matrix columns");
            const int rows = vals.size() / cols;
            Matrix_<E_> ret_val(rows, cols);
            for (int ir = 0; ir < rows; ++ir)
                std::copy(vals.begin() + ir * cols, vals.begin() + (ir + 1) * cols, ret_val.Row(ir).begin());
            return ret_val;
        }

        double EDouble(const element_t& doc) {
            REQUIRE(doc.IsDouble() || doc.IsInt(), "Can't get a numeric value");
            return doc.GetDouble();
        }
        int EInt(const element_t& doc) {
            REQUIRE(doc.IsInt(), "Can't get an integer value");
            return doc.GetInt();
        }
        bool EBool(const element_t& doc) {
            REQUIRE(doc.IsBool(), "Can't get a boolean value");
            return doc.GetBool();
        }
        String_ EString(const element_t& doc) {
            REQUIRE(doc.IsString(), "Can't get a string value");
            return String_(doc.GetString(), doc.GetString() + doc.GetStringLength());
        }
        Date_ EDate(const element_t& doc) { // worrying about efficiency, so storing dates as Excel-compatible integers
            if (doc.IsInt())
                return Date::FromExcel(doc.GetInt());
            if (doc.IsString())
                return Date::FromString(doc.GetString());
            THROW("Can't get a date value");
        }
        DateTime_
        EDateTime(const element_t& doc) { // worrying about efficiency, so storing dates as Excel-compatible doubles
            if (doc.IsDouble()) {
                double d = doc.GetDouble();
                int i = AsInt(d);
                return DateTime_(Date::FromExcel(i), d - i);
            }
            if (doc.IsString())
                return DateTime::FromString(doc.GetString());
            THROW("Can't get a datetime value");
        }
        Cell_ ECell(const element_t& doc) {
            static const std::regex DATE_PATTERN(R"(\d{4}-\d{2}-\d{2})");
            static const std::regex DATE_TIME_PATTERN(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
            if (doc.IsDouble() || doc.IsInt())
                return Cell_(doc.GetDouble());
            if (doc.IsBool())
                return Cell_(doc.GetBool());
            if (doc.IsString()) {
                const std::string src = doc.GetString();
                if (std::regex_match(src, DATE_PATTERN))
                    return Cell_(Date::FromString(String_(src)));
                else if (std::regex_match(src, DATE_TIME_PATTERN))
                    return Cell_(DateTime::FromString(String_(src)));
                else
                    return Cell_(String_(src));
            }
            if (doc.IsNull())
                return {};
            THROW("Invalid cell type");
        }

        struct XDocView_ : Archive::View_ {
            element_t& doc_;
            std::map<String_, Handle_<XDocView_>> children_;
            explicit XDocView_(element_t& doc) : doc_(doc) {
                if (doc_.IsObject()) {
                    for (auto it = doc_.MemberBegin(); it != doc_.MemberEnd(); ++it)
                        children_.emplace(
                            String_(it->name.GetString(), it->name.GetString() + it->name.GetStringLength()),
                            Handle_<XDocView_>(new XDocView_(it->value)));
                }
            }

            String_ AfterPrefix(char prefix) const {
                if (doc_.IsString() && doc_.GetStringLength() > 1 && doc_.GetString()[0] == prefix) {
                    return String_(doc_.GetString() + 1, doc_.GetString() + doc_.GetStringLength());
                }
                return {};
            }
            String_ Tag() const {
                if (doc_.HasMember(TAG_LABEL))
                    return EString(doc_[TAG_LABEL]);
                return {};
            }
            Handle_<Storable_>& Known(Archive::Built_& built) const override {
                return built.known_[Tag()];
            }

            double AsDouble() const override { return EDouble(doc_); }
            int AsInt() const override { return EInt(doc_); }
            bool AsBool() const override { return EBool(doc_); }
            Date_ AsDate() const override { return EDate(doc_); }
            String_ AsString() const override { return EString(doc_); }
            Dictionary_ AsDictionary() const override { return Dictionary::FromString(AsString()); }

            Vector_<> AsDoubleVector() const override { return AsVector(doc_, EDouble); }
            Vector_<int> AsIntVector() const override { return AsVector(doc_, EInt); }
            Vector_<bool> AsBoolVector() const override { return AsVector(doc_, EBool); }
            Vector_<String_> AsStringVector() const override { return AsVector(doc_, EString); }
            Vector_<Date_> AsDateVector() const override { return AsVector(doc_, EDate); }
            Vector_<DateTime_> AsDateTimeVector() const override { return AsVector(doc_, EDateTime); }
            Vector_<Cell_> AsCellVector() const override { return AsVector(doc_, ECell); }

            // Matrix stored as child "cols" + child "vals" (row-major)
            Matrix_<> AsDoubleMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], EDouble));
            }
            Matrix_<String_> AsStringMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], EString));
            }
            Matrix_<Cell_> AsCellMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], ECell));
            }

            String_ Type() const override {
                if (doc_.HasMember(TYPE_LABEL))
                    return EString(doc_[TYPE_LABEL]);
                return {};
            }
            bool HasChild(const String_& name) const override { return children_.find(name) != children_.end(); }
            const View_& Child(const String_& name) const override {
                const auto it = children_.find(name);
                REQUIRE(it != children_.end(), "Child '" + name + "' not found");
                return *it->second;
            }

            void Unexpected(const String_&) const override {}
        };
    } // namespace

    Handle_<Storable_> JSON::ReadString(const char* src, std::size_t length, const JSONReadOptions_& options) {
        NOTE("Extracting object from JSON string");
        REQUIRE(src, "ARCHIVE_PAYLOAD_NULL");
        REQUIRE(length <= options.maxInputBytes_, "ARCHIVE_PAYLOAD_TOO_LARGE");
        const auto nul = std::find(src, src + length, '\0');
        REQUIRE(nul == src + length, "ARCHIVE_PAYLOAD_NUL at byte " + ToString(static_cast<int>(nul - src)));
        std::size_t badOffset = 0;
        REQUIRE(ValidUtf8(src, length, &badOffset),
                "ARCHIVE_PAYLOAD_INVALID_UTF8 at byte " + ToString(static_cast<int>(badOffset)));
        rapidjson::Document doc;
        doc.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseFullPrecisionFlag>(src, length);
        REQUIRE(
            !doc.HasParseError() ||
                doc.GetParseError() != rapidjson::kParseErrorStringUnicodeSurrogateInvalid,
            "ARCHIVE_STRING_INVALID_UNICODE");
        REQUIRE(!doc.HasParseError(), "JSON parse error in input string");
        ValidateDecodedStrings(doc);
        const XDocView_ task(doc);
        Archive::Built_ built;
        return Archive::Extract(task, built);
    }

    Handle_<Storable_> JSON::ReadString(const String_& src, bool quiet) {
        static_cast<void>(quiet);
        return ReadString(src.data(), src.size(), JSONReadOptions_());
    }

    Handle_<Storable_> JSON::ReadFile(const String_& filename, bool quiet) {
        static_cast<void>(quiet);
        std::ifstream source(filename.c_str(), std::ios::binary);
        REQUIRE(source, "File not found:'" + filename + "'");
        const std::string payload((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
        return ReadString(payload.data(), payload.size(), JSONReadOptions_());
    }

    void JSON::WriteFile(const Storable_& object, const String_& filename) {
        std::ofstream dst(filename.c_str());
        std::map<const Storable_*, String_> tags;
        XDocStore_ task(dst, tags, nullptr, String_());
        object.Write(task);
    }

    String_ JSON::WriteString(const Storable_& object) {
        std::stringstream ret_val;
        std::map<const Storable_*, String_> tags;
        XDocStore_ task(ret_val, tags, nullptr, String_());
        object.Write(task);
        return String_(ret_val.str());
    }
} // namespace Dal
