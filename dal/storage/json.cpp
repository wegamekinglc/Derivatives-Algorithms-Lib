//
// Created by wegam on 2023/1/21.
//

#include <fstream>
#include <sstream>
#include <regex>
#include <dal/utilities/rapidjson/document.h>
#include <dal/utilities/rapidjson/writer.h>
#include <dal/utilities/rapidjson/prettywriter.h>
#include <dal/utilities/rapidjson/filereadstream.h>
#include <dal/utilities/rapidjson/filewritestream.h>

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
        rapidjson::GenericStringRef<char> LendToJSON(const String_& s) {
            return rapidjson::GenericStringRef<char>(s.c_str(), static_cast<int>(s.size()));
        }
        // WRITE interface
        // given up on rapidjson, think it is simple enough to just stream out
        // POSTPONED -- add a template parameter determining the checking strategy for funny characters inside strings
        // this simple implementation just assumes there are none (fastest)
        // could have one that wraps, one that errors

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
            const char* Prep() { return empty_ ? "{\n" : ",\n"; }
            void StoreRefTag(const String_& tag) {
                assert(empty_); // this should always be the first thing written
                dst_ << Prep() << "\"" << TAG_LABEL << "\": \"" << tag << "\"";
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
                // assert(!empty_);	// should have a tag -- unless it is the toplevel!
                dst_ << Prep() << "\"" << TYPE_LABEL << "\": \"" << type << "\"";
                empty_ = false;
            }
            void Done() override { dst_ << '}'; }
            Store_& Child(const String_& name) override {
                std::shared_ptr<XDocStore_>& retval = children_[name];
                if (!retval) {
                    // assume we are going to write the child immediately
                    dst_ << Prep() << "\"" << name << "\": ";
                    retval.reset(new XDocStore_(dst_, sharedTags_, this, name));
                }
                return *retval;
            }

            // store atoms
            XDocStore_& operator=(double val) override {
                dst_ << String::FromDouble(val);
                return *this;
            }

            XDocStore_& operator=(int val) {
                dst_ << String::FromDouble(val);
                return *this;
            }
            XDocStore_& operator=(bool val) {
                dst_ << String::FromBool(val);
                return *this;
            }
            XDocStore_& operator=(const String_& val) override { dst_ << "\"" << val << "\""; return *this; }
            XDocStore_& operator=(const Date_& val) override { return operator=(Date::ToString(val)); }
            XDocStore_& operator=(const DateTime_& val) override { return operator=(DateTime::ToString(val)); }
            XDocStore_& operator=(const Cell_& c) {
                if (Cell::IsBool(c))
                    return operator=(Cell::ToBool(c));
                else if (Cell::IsDate(c))
                    return operator=(Cell::ToDate(c));
                else if (Cell::IsDateTime(c))
                    return operator=(Cell::ToDateTime(c));
                else if (Cell::IsString(c))
                    return operator=(Cell::ToString(c));
                else if (Cell::IsDouble(c))
                    return operator=(Cell::ToDouble(c));
                else if (Cell::IsInt(c))
                    return operator=(Cell::ToInt(c));
                else if (Cell::IsEmpty(c))
                    return operator=(String_());
                else
                    THROW("Internal error -- unhandled cell type");
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
            XDocStore_& operator=(const Dictionary_& val) override { return operator=(Dictionary::ToString(val)); }
        };

        // READ interface
        template <class E_>
        auto AsVector(element_t& doc, const E_& extract) -> typename vector_of<decltype(extract(doc))>::type {
            REQUIRE(doc.IsArray(), "Can't get a vector value");
            const int n = doc.Size();
            typename vector_of<decltype(extract(doc))>::type ret_val(n);
            for (int ii = 0; ii < n; ++ii)
                ret_val[ii] = extract(doc[ii]);
            return ret_val;
        }
        // create matrix from vector of values
        template <class E_> Matrix_<E_> AsMatrix(int cols, const Vector_<E_>& vals) {
            REQUIRE(cols > 0 && !(vals.size() % cols), "Invalid number of matrix columns");
            const int rows = vals.size() / cols;
            Matrix_<E_> ret_val(rows, cols);
            for (int ir = 0; ir < rows; ++ir)
                std::copy(vals.begin() + ir * cols, vals.begin() + (ir + 1) * cols, ret_val.Row(ir).begin());
            return ret_val;
        }

        // need checked access at every level -- support with "E" element-extractors
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
            return String_(doc.GetString());
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
            static const std::regex DATE_PATTERN("\\d{4}-\\d{2}-\\d{2}");
            static const std::regex DATE_TIME_PATTERN("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}");
            if (doc.IsDouble())
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
                return Cell_();
            THROW("Invalid cell type");
        }

        // JSON reader based on RapidJSON DOM interface
        struct XDocView_ : Archive::View_ {
            element_t& doc_; // may be shared
            mutable std::map<String_, Handle_<XDocView_>> children_;
            XDocView_(element_t& doc) : doc_(doc) {}

            // shared-object part of Archive::View_ interface
            // we will do this by having a Tag() which is empty except for shared objects (same as Splat)
            String_ AfterPrefix(char prefix) const {
                if (doc_.IsString() && doc_.GetStringLength() > 1 && doc_.GetString()[0] == prefix) {
                    return String_(doc_.GetString()).substr(1);
                }
                return String_();
            }
            String_ Tag() const {
                if (doc_.HasMember(TAG_LABEL))
                    return EString(doc_[TAG_LABEL]);
                return String_();
            }
            Handle_<Storable_>& Known(Archive::Built_& built) const override // returns a reference within 'built'
            {
                return built.known_[Tag()];
            }

            // direct storage of atoms
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

            // matrix storage is:
            //		the matrix has a child "cols" that gives the number of columns
            //		and a child "vals" that gives all the values, scanning each row in turn
            Matrix_<> AsDoubleMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], EDouble));
            }
            Matrix_<String_> AsStringMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], EString));
            }
            Matrix_<Cell_> AsCellMatrix() const override {
                return AsMatrix(EInt(doc_[COLS_LABEL]), AsVector(doc_[VALS_LABEL], ECell));
            }

            // object query interface
            String_ Type() const override {
                if (doc_.HasMember(TYPE_LABEL))
                    return EString(doc_[TYPE_LABEL]);
                return String_();
            }
            element_t& XChild(const String_& name) const { return doc_[name.c_str()]; }
            bool HasChild(const String_& name) const override { return doc_.HasMember(name.c_str()); }
            const View_& Child(const String_& name) const override {
                Handle_<XDocView_>& ret_val = children_[name];
                if (ret_val.IsEmpty())
                    ret_val.reset(new XDocView_(XChild(name)));
                return *ret_val;
            }

            void Unexpected(const String_&) const override {} // ignore extra children
        };
    }	// leave local

    Handle_<Storable_> JSON::ReadString(const String_& src, bool quiet) {
        NOTE("Extracting object from JSON string");
        rapidjson::Document doc;
        doc.Parse<rapidjson::kParseDefaultFlags>(src.c_str());
        XDocView_ task(doc);
        Archive::Built_ built;
        return Archive::Extract(task, built);
    }

    Handle_<Storable_> JSON::ReadFile(const String_& filename, bool quiet) {
        FILE* fp = fopen(filename.c_str(), "rb"); // POSTPONED -- use "r" on non-Windows platforms
        REQUIRE(fp, "File not found:'" + filename + "'");
        char buffer[8192];
        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
        rapidjson::Document doc;
        doc.ParseStream<rapidjson::kParseDefaultFlags>(is);
        fclose(fp);
        XDocView_ task(doc);
        Archive::Built_ built;
        return Archive::Extract(task, built);
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
}
