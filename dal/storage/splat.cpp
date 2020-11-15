//
// Created by wegam on 2020/11/15.
//

#include <dal/platform/platform.hpp>
#include <map>
#include <dal/storage/splat.hpp>
#include <dal/platform/strict.hpp>

#include <dal/storage/archive.hpp>
#include <dal/math/cell.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/dictionary.hpp>

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
    }
}