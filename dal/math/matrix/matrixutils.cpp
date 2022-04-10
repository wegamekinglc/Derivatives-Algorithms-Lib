//
// Created by wegam on 2022/4/3.
//

#include <dal/math/matrix/matrixutils.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/math/cell.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/math/matrix/matrixs.hpp>

namespace Dal {
    namespace {
        typedef Matrix_<Cell_> Table_;

        struct WriterView_ {
            mutable Table_* dst_;
            int rowOffset_;
            int colOffset_;
            // linear mapping from input coordinate to coordinate in dst_
            int r2r_, r2c_, c2r_, c2c_;

            WriterView_(Table_* dst) : dst_(dst), rowOffset_(0), colOffset_(0), r2r_(1), r2c_(0), c2r_(0), c2c_(1) {}

            void Write(int i_row, int i_col, const Cell_& val) const {
                const int dstRow = rowOffset_ + r2r_ * i_row + c2r_ * i_col;
                const int dstCol = colOffset_ + r2c_ * i_row + c2c_ * i_col;
                (*dst_)(dstRow, dstCol) = val;
            }

            WriterView_ Transpose() const {
                WriterView_ ret_val(*this);
                std::swap(ret_val.r2r_, ret_val.c2r_);
                std::swap(ret_val.r2c_, ret_val.c2c_);
                return ret_val;
            }
            WriterView_ Invert(int n_rows, int n_cols) const {
                WriterView_ ret_val(*this);
                ret_val.rowOffset_ += r2r_ * (n_rows - 1) + c2r_ * (n_cols - 1);
                ret_val.colOffset_ += r2c_ * (n_rows - 1) + c2c_ * (n_cols - 1);
                ret_val.r2r_ *= -1;
                ret_val.r2c_ *= -1;
                ret_val.c2r_ *= -1;
                ret_val.c2c_ *= -1;
                return ret_val;
            }
            WriterView_ Shift(int row_offset, int col_offset) const {
                WriterView_ ret_val(*this);
                ret_val.rowOffset_ += r2r_ * row_offset + c2r_ * col_offset;
                ret_val.colOffset_ += r2c_ * row_offset + c2c_ * col_offset;
                return ret_val;
            }
            WriterView_ Flatten(int n_cols) const {
                WriterView_ ret_val(*this);
                ret_val.r2r_ = n_cols * ret_val.c2r_;
                ret_val.r2c_ = n_cols * ret_val.c2c_;
                return ret_val;
            }
        };

        struct Writer_ : noncopyable {
            virtual ~Writer_() {}
            virtual int Rows(const Vector_<const Table_*>& args) const = 0;
            virtual int Cols(const Vector_<const Table_*>& args) const = 0;
            virtual void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const = 0;
        };

        struct TransposedWriter_ : Writer_ {
            scoped_ptr<Writer_> base_;
            TransposedWriter_(Writer_* base) : base_(base) {}
            int Rows(const Vector_<const Table_*>& args) const { return base_->Cols(args); }
            int Cols(const Vector_<const Table_*>& args) const { return base_->Rows(args); }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                base_->Write(dst.Transpose(), args);
            }
        };

        struct InvertedWriter_ : Writer_ {
            scoped_ptr<Writer_> base_;
            InvertedWriter_(Writer_* base) : base_(base) {}
            int Rows(const Vector_<const Table_*>& args) const { return base_->Rows(args); }
            int Cols(const Vector_<const Table_*>& args) const { return base_->Cols(args); }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                base_->Write(dst.Invert(Rows(args), Cols(args)), args);
            }
        };

        struct LinearWriter_ : Writer_ // puts everything in a single row
        {
            scoped_ptr<Writer_> base_;
            LinearWriter_(Writer_* base) : base_(base) {}
            int Rows(const Vector_<const Table_*>&) const { return 1; }
            int Cols(const Vector_<const Table_*>& args) const { return base_->Rows(args) * base_->Cols(args); }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                base_->Write(dst.Flatten(base_->Cols(args)), args);
            }
        };

        struct HorizontalWriter_
            : Writer_ // writes args left-to-right, justifies to top; transpose to make a vertical writer
        {
            bool justifyBottom_;
            Vector_<Handle_<Writer_>> elements_;
            HorizontalWriter_(bool justify_bottom = false) : justifyBottom_(justify_bottom) {}

            int Rows(const Vector_<const Table_*>& args) const {
                int ret_val = 0;
                for (const auto& e : elements_)
                    ret_val = Max(ret_val, e->Rows(args));
                return ret_val;
            }
            int Cols(const Vector_<const Table_*>& args) const {
                int ret_val = 0;
                for (const auto& e : elements_)
                    ret_val += e->Cols(args);
                return ret_val;
            }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                WriterView_ temp(dst);
                const int height = justifyBottom_ ? Rows(args) : 0;
                for (const auto& e : elements_) {
                    const int vShift = justifyBottom_ ? height - e->Rows(args) : 0;
                    temp = temp.Shift(vShift, 0);
                    e->Write(temp, args);
                    temp = temp.Shift(-vShift, e->Cols(args));
                }
            }
        };

        struct VerticalWriter_
            : Writer_ // writes args left-to-right, justifies to top; transpose to make a horizontal writer
        {
            bool justifyRight_;
            Vector_<Handle_<Writer_>> elements_;
            VerticalWriter_(bool justify_right) : justifyRight_(justify_right) {}

            int Cols(const Vector_<const Table_*>& args) const {
                int ret_val = 0;
                for (const auto& e : elements_)
                    ret_val = Max(ret_val, e->Cols(args));
                return ret_val;
            }
            int Rows(const Vector_<const Table_*>& args) const {
                int ret_val = 0;
                for (const auto& e : elements_)
                    ret_val += e->Rows(args);
                return ret_val;
            }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                WriterView_ temp(dst);
                const int width = justifyRight_ ? Cols(args) : 0;
                for (const auto& e : elements_) {
                    const int hShift = justifyRight_ ? width - e->Cols(args) : 0;
                    temp = temp.Shift(0, hShift);
                    e->Write(temp, args);
                    temp = temp.Shift(e->Rows(args), -hShift);
                }
            }
        };

        struct EmptyCell_ : Writer_ {
            int Rows(const Vector_<const Table_*>&) const { return 1; }
            int Cols(const Vector_<const Table_*>&) const { return 1; }
            void Write(const WriterView_& dst, const Vector_<const Table_*>&) const { dst.Write(0, 0, Cell_()); }
        };

        struct ArgWriter_ : Writer_ {
            int whichArg_;
            ArgWriter_(int which) : whichArg_(which) {}
            int Rows(const Vector_<const Table_*>& args) const {
                return whichArg_ < args.size() ? args[whichArg_]->Rows() : 0;
            }
            int Cols(const Vector_<const Table_*>& args) const {
                return whichArg_ < args.size() ? args[whichArg_]->Cols() : 0;
            }
            void Write(const WriterView_& dst, const Vector_<const Table_*>& args) const {
                if (whichArg_ >= args.size())
                    return;
                const Table_& src = *args[whichArg_];
                for (int ir = 0; ir < src.Rows(); ++ir)
                    for (int ic = 0; ic < src.Cols(); ++ic)
                        dst.Write(ir, ic, src(ir, ic));
            }
        };

        Vector_<String_> Split(const String_& src, char sep) {
            Vector_<String_> ret_val(1, String_());
            int depth = 0;
            for (const auto& s : src) {
                if (s == sep && depth == 0)
                    ret_val.push_back(String_());
                else {
                    ret_val.back().push_back(s);
                    if (s == '(')
                        ++depth;
                    if (s == ')')
                        --depth;
                }
            }
            return ret_val;
        }

        String_ Strip(const String_& src) // remove parentheses around the whole thing
        {
            auto ps = src.begin();
            for (int depth = 0; ps != src.end(); ++ps) {
                if (*ps == '(')
                    ++depth;
                else if (*ps == ')')
                    --depth;
                if (depth == 0)
                    break;
            }
            return (ps == src.end() - 1 && *ps == ')') // parentheses wrap the whole thing
                       ? Strip(src.substr(1, src.size() - 2))
                       : src;
        }

        Writer_* NewWriter(const String_& format);
        template <class M_> Writer_* MultipleWriter(const String_& format, char separator, M_ make_multiple) {
            typedef typename std::remove_reference<decltype(*make_multiple())>::type multiple_t;
            Vector_<String_> subs = Split(format, separator);
            if (subs.size() <= 1)
                return nullptr;
            std::unique_ptr<multiple_t> ret_val(make_multiple());
            for (const auto& s : subs)
                ret_val->elements_.emplace_back(NewWriter(s));
            return ret_val.release();
        }

        Writer_* XNewWriter(const String_& format) {
            // POSTPONED -- should ':' and ';' have the same precedence?
            if (auto vr = MultipleWriter(format, ':', []() { return new VerticalWriter_(true); }))
                return vr;
            if (auto vl = MultipleWriter(format, ';', []() { return new VerticalWriter_(false); }))
                return vl;
            // ok, no unparenthesized semicolons
            if (auto hr = MultipleWriter(format, '.', []() { return new HorizontalWriter_(true); }))
                return hr;
            if (auto hl = MultipleWriter(format, ',', []() { return new HorizontalWriter_(false); }))
                return hl;

            // no commas:  just one element
            if (toupper(format.back()) == 'T')
                return new TransposedWriter_(NewWriter(format.substr(0, format.size() - 1)));
            else if (toupper(format.back()) == 'I')
                return new InvertedWriter_(NewWriter(format.substr(0, format.size() - 1)));
            else if (format.back() == '*')
                return new LinearWriter_(NewWriter(format.substr(0, format.size() - 1)));
            else {
                REQUIRE(format.size() == 1 && format.front() >= '0' && format.front() <= '9',
                        "Can't recognize format element -- expected argument index (format = '" + format + "')");
                return format.front() == '0'
                           ? (Writer_*)new EmptyCell_
                           : new ArgWriter_(format.front() - '1'); // implements 1-offset count of args
            }
        }
        Writer_* NewWriter(const String_& format) { return XNewWriter(Strip(format)); }
    } // namespace

    Matrix_<Cell_> Matrix::Format(const Vector_<const Table_*>& args, const String_& format) {
        scoped_ptr<Writer_> writer(NewWriter(format));
        Matrix_<Cell_> ret_val(writer->Rows(args), writer->Cols(args));
        REQUIRE(ret_val.Rows() * ret_val.Cols() > 0, "Nothing to output");
        writer->Write(WriterView_(&ret_val), args);
        return ret_val;
    }
} // namespace Dal
