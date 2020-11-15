//
// Created by wegam on 2020/11/15.
//

#include <dal/platform/platform.hpp>
#include <map>
#include <dal/storage/splat.hpp>
#include <dal/platform/strict.hpp>

#include <dal/storage/archive.hpp>
#include <dal/math/cell.hpp>
#include <dal/string/stringutils.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/time/datetimeutils.hpp>
#include <dal/utilities/dictionary.hpp>
#include <dal/utilities/numerics.hpp>

using std::map;
using std::shared_ptr;

namespace Dal {
    namespace {
        const String_ OBJECT_PREFACE("~");
        const String_ TAG_PREFACE("$");

        struct XSplat_ : Archive::Store_ {
            String_ tag_;
            String_ type_;

            map<String_, shared_ptr<XSplat_>> children_;
            map<const Storable_*, String_>& sharedTags_;
            Matrix_<Cell_> val_;

            explicit XSplat_(map<const Storable_*, String_>& shared_tags): sharedTags_(shared_tags) {}

            int Rows() const {
                REQUIRE(type_.empty() == children_.empty(), "children asn type status should be same");
                if (!val_.Empty()) {
                    REQUIRE(type_.empty(), "");
                    return val_.Rows();
                }
                int ret_val = 0;
                for (const auto& c : children_)
                    ret_val += c.second->Rows();
                return ret_val;
            }

            int Cols() const {
                if (!val_.Empty())
                    return val_.Cols();
                int ret_val = 0;
                for (const auto& c : children_)
                    ret_val += Max(ret_val, c.second->Cols());
                return 2 + ret_val;
            }

            void Write(Matrix_<Cell_>& dst, int row_offset, int col_offset) const {
                if (!val_.Empty()) {
                    for(int ir = 0; ir < val_.Rows(); ++ir)
                        copy(val_.Row(ir).begin(),
                             val_.Row(ir).end(),
                             dst.Row(ir + row_offset).begin() + col_offset);
                }
                else {
                    dst(row_offset, col_offset) = tag_ + OBJECT_PREFACE + type_;
                    for (const auto& c : children_) {
                        dst (row_offset, 1 + col_offset) = c.first;
                        c.second->Write(dst, row_offset, 2 + col_offset);
                        row_offset += c.second->Rows();
                    }
                }
            }

            bool StoreRef(const Storable_* object) override {
                auto ot = sharedTags_.find(object);
                if (ot != sharedTags_.end()) {
                    SetScalar(ot->second);
                    return true;
                }
                auto tag = TAG_PREFACE + ToString(1 + static_cast<int>(sharedTags_.size()));
                sharedTags_.insert(make_pair(object, tag));
                SetTag(tag);
                return false;
            }

            void SetType(const String_& type) override { type_ = type;}
            void SetTag(const String_& tag) { tag_ = tag;}
            XSplat_& Child(const String_& name) override {
                shared_ptr<XSplat_>& ret_val = children_[name];
                if (!ret_val.get())
                    ret_val.reset(new XSplat_(sharedTags_));
                return *ret_val;
            }

            template <class E_>
            void SetScalar(const E_& e) {
                val_.Resize(1, 1);
                val_(0, 0) = e;
            }

            template <class E_>
            void SetVector(const Vector_<E_>& v) {
                val_.Resize(1, static_cast<int>(v.size()));
                auto dst = val_.Row(0);
                Copy(v, &dst);
            }

            template <class E_>
            void SetMatrix(const Matrix_<E_>& m) {
                val_.Resize(m.Rows(), m.Cols());
                for (auto ir = 0; ir < m.Rows(); ++ir) {
                    auto dst = val_.Row(ir);
                    Copy(m.Row(ir), &dst);
                }
            }

            void operator=(double d) override { SetScalar(d);}
            void operator=(const Date_& d) override { SetScalar(d);}
            void operator=(const String_& s) override { SetScalar(s); }
            void operator=(const Vector_<>& v) override { SetVector(v); }
            void operator=(const Vector_<int>& v) override { SetVector(v); }
            void operator=(const Vector_<bool>& v) override { SetVector(v); }
            void operator=(const Vector_<String_>& v) override { SetVector(v); }
            void operator=(const Vector_<Date_>& v) override { SetVector(v); }
            void operator=(const Vector_<DateTime_>& v) override { SetVector(v); }
            void operator=(const Matrix_<>& m) override { SetMatrix(m); }
            void operator=(const Matrix_<String_>& m) override { SetMatrix(m); }
            void operator=(const Matrix_<Cell_>& m) override { SetMatrix(m); }
            void operator=(const Dictionary_& d) override {
                val_.Resize(d.Size(), 2);
                int ir = 0;
                for (const auto& k_v : d) {
                    val_(ir, 0) = k_v.first;
                    val_(ir, 1) = k_v.second;
                    ++ir;
                }
            }

        };

        // --------------------------------------
        // helper functions for UnSplat

        double ExtractDouble(const Cell_& src) {
            switch (src.type_) {
            case Cell_::Type_ ::NUMBER:
                return src.d_;
            case Cell::Type_::STRING:
                return String::ToDouble(src.s_);
            }
            THROW("Can't create a number from a non-numeric type");
        }

        int ExtractInt(const Cell_& src) {
            const auto d = ExtractDouble(src);
            int ret_val = static_cast<int>(d);
            REQUIRE(ret_val == d, "Non-integer value not expected");
            return ret_val;
        }

        bool ExtractBool(const Cell_& src) {
            switch (src.type_) {
            case Cell::Type_ ::BOOLEAN:
                return src.b_;
            case Cell::Type_::STRING:
                return String::ToBool(src.s_);
            }
            THROW("Can't construct a boolean flag from a non-boolean value");
        }

        String_ ExtractString(const Cell_& src) {
            switch (src.type_) {
            case Cell::Type_::EMPTY:
                return String_();
            case Cell::Type_::STRING:
                return src.s_;
            }
            THROW("Can't construct a String_ from a non-text value");
        }

        Date_ ExtractDate(const Cell_& src) {
            switch (src.type_) {
            case Cell::Type_::EMPTY:
                return Date_();
            case Cell::Type_::STRING:
                return Date::FromString(src.s_);
            case Cell::Type_::DATE:
                return src.dt_.Date();
            }
            THROW("Can't construct a Date_ from non-date value");
        }

        DateTime_ ExtractDateTime(const Cell_& src) {
            switch (src.type_) {
            case Cell::Type_::EMPTY:
                return DateTime_();
            case Cell::Type_::STRING:
                return DateTime::FromString(src.s_);
            case Cell::Type_::DATE:
            case Cell::Type_::DATETIME:
                return src.dt_;
            }
            THROW("Can't construct a Date_ from a non-date value");
        }

        template <class R_, class T_>
        auto TranslateRange(const R_& range, const T_& translate) {
            Vector_<VALUE_TYPE_OF(translate(Cell_()))> ret_val;
            std::transform(range.first, range.second, std::back_inserter(ret_val), translate);
            return ret_val;
        }

        struct XUnSplat_ : Archive::View_ {
            const Matrix_<Cell_>& data_;
            int rowStart_;
            int rowStop_;
            int colStart_;
            mutable  Vector_<shared_ptr<XUnSplat_>> children_;
            bool quiet_;

            XUnSplat_(const Matrix_<Cell_>& data,
                      int row_start,
                      int row_stop,
                      int col_start,
                      bool quiet)
            :data_(data), rowStart_(row_start), rowStop_(row_stop), colStart_(col_start), quiet_(quiet) {}

            String_ Type() const override {
                const Cell_& c = data_(rowStart_, colStart_);
                if (c.type_ != Cell::Type_::STRING)
                    return String_();
                auto pt = c.s_.find(OBJECT_PREFACE);
                if (pt == String_::npos)
                    return String_();
                return c.s_.substr(pt + OBJECT_PREFACE.size());
            }

            String_ Tag() const {
                const Cell_& c = data_(rowStart_, colStart_);
                if (c.type_ != Cell::Type_::STRING)
                    return String_();
                if (c.s_.substr(0, TAG_PREFACE.size()) != TAG_PREFACE)
                    return String_();
                auto pt = c.s_.find(OBJECT_PREFACE);
                return c.s_.substr(0, pt);
            }

            const View_& Child(const String_& name) const override {
                REQUIRE(!Type().empty(), "Can't be a empty view");
                const auto nameCol = colStart_ + 1;
                REQUIRE(nameCol < data_.Cols(), "Can't have a child");
                for (auto ir = rowStart_; ir < rowStop_; ++ir) {
                    if (data_(ir, nameCol) == name) {
                        auto jr = ir + 1;
                        while (jr < rowStop_ && Cell::IsEmpty(data_(jr, nameCol)))
                            ++jr;
                        children_.emplace_back(new XUnSplat_(data_, ir, jr, nameCol + 1, quiet_));
                        return * children_.back();
                    }
                }
                THROW("Child '" + name + "' not found");
            }

            bool HasChild(const String_& name) const override {
                REQUIRE(!Type().empty(), "Can't be a empty view");
                const auto nameCol = colStart_ + 1;
                REQUIRE(nameCol < data_.Cols(), "Can't have a child");
                for (auto ir = rowStart_; ir < rowStop_; ++ir) {
                    if (data_(ir, nameCol) == name) {
                        return true;
                    }
                }
                return false;
            }

            void Unexpected(const String_& child_name) const override {
                REQUIRE(quiet_, "Unexpected child '" + child_name + "'; aborting");
            }

            const Cell_& GetScalar() const {
                REQUIRE(rowStop_ == rowStart_ + 1, "Can't get a scalar value from a multi-line entry");
                REQUIRE(colStart_ == data_.Cols() -1 || Cell::IsEmpty(data_(rowStart_, colStart_ + 1)),
                        "Can't get a scalar value from a multi-row entry");
                return data_(rowStart_, colStart_);
            }

            int AsInt() const override {
                auto temp = AsDouble();
                int ret_val = Dal::AsInt(temp);
                REQUIRE(ret_val == temp, "Can't get an integer from a non-integer entry");
                return ret_val;
            }
        };
    }

    Matrix_<Cell_> Splat(const Storable_& src)
    {
        map<const Storable_*, String_> tags;
        XSplat_ task(tags);
        src.Write(task);
        Matrix_<Cell_> ret_val(task.Rows(), task.Cols());
        task.Write(ret_val, 0, 0);
        return ret_val;
    }
}