//
// Created by Cheng Li on 2018/1/13.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    template <class E_> class Matrix_ {
    public:
        using I_ = typename Vector_<E_>::iterator;
        using CI_ = typename Vector_<E_>::const_iterator;
        using R_ = typename Vector_<E_>::reference;
        using CR_ = typename Vector_<E_>::const_reference;

    private:
        // dense row-major storage; element (r, c) lives at r * cols_ + c
        Vector_<E_> vals_;
        int rows_;
        int cols_;
        static size_t Extent(int rows, int cols) { return static_cast<size_t>(rows) * static_cast<size_t>(cols); }
        size_t Offset(int row, int col) const { return static_cast<size_t>(row) * static_cast<size_t>(cols_) + static_cast<size_t>(col); }

    public:
        virtual ~Matrix_() = default;
        Matrix_() : rows_(0), cols_(0) {}
        Matrix_(int rows, int cols, E_ val = E_()) : vals_(Extent(rows, cols)), rows_(rows), cols_(cols) { vals_.Fill(val); }
        Matrix_(const Matrix_& src) : vals_(src.vals_), rows_(src.rows_), cols_(src.cols_) {}

        int Rows() const { return rows_; }
        int Cols() const { return cols_; }
        bool Empty() const { return vals_.empty(); }
        void Clear() {
            vals_.clear();
            rows_ = 0;
            cols_ = 0;
        }
        CI_ First() const { return vals_.begin(); }
        inline CI_ begin() const { return First(); }
        CI_ Last() const { return vals_.end(); }
        inline CI_ end() const { return Last(); }

        CR_ operator()(int row, int col) const { return vals_[Offset(row, col)]; }
        R_ operator()(int row, int col) { return vals_[Offset(row, col)]; }

        // contiguous row-major storage; null when empty
        E_* Data() { return vals_.data(); }
        const E_* Data() const { return vals_.data(); }

        // move operators
        void swap(Matrix_& rhs) noexcept {
            std::swap(vals_, rhs.vals_);
            std::swap(rows_, rhs.rows_);
            std::swap(cols_, rhs.cols_);
        }

        Matrix_(Matrix_&& rhs) noexcept : vals_(std::move(rhs.vals_)), rows_(rhs.rows_), cols_(rhs.cols_) {
            rhs.rows_ = 0;
            rhs.cols_ = 0;
        }

        Matrix_& operator=(Matrix_&& rhs) noexcept {
            if (this != &rhs) {
                Matrix_<E_> temp(std::move(rhs));
                swap(temp);
            }
            return *this;
        }

        Matrix_& operator=(const Matrix_& rhs) {
            if (this != &rhs) {
                if (rows_ == rhs.rows_ && cols_ == rhs.cols_) {
                    // equal extents: vector assignment reuses storage
                    vals_ = rhs.vals_;
                } else {
                    Matrix_<E_> temp(rhs);
                    swap(temp);
                }
            }
            return *this;
        }

        // slices -- ephemeral containers of rows or columns
        class ConstRow_ {
        protected:
            CI_ begin_;
            CI_ end_;

        public:
            using value_type = E_;
            using const_iterator = typename Vector_<E_>::const_iterator;

            ConstRow_(CI_ begin, CI_ end) : begin_(begin), end_(end) {}
            ConstRow_(CI_ begin, int size) : begin_(begin), end_(begin + size) {}

            const_iterator begin() const { return begin_; }
            const_iterator end() const { return end_; }
            [[nodiscard]] int size() const { return static_cast<int>(end_ - begin_); }
            const E_& operator[](int col) const { return *(begin_ + col); }
            const E_& front() const { return *begin_; }
            const E_& back() const { return *(end_ - 1); }
            operator Vector_<E_>() const { return Vector_<E_>(begin_, end_); }
        };

        ConstRow_ Row(int iRow) const { return ConstRow_(vals_.cbegin() + Offset(iRow, 0), cols_); }
        ConstRow_ operator[](int iRow) const { return Row(iRow); }

        struct Row_ : ConstRow_ {
            using iterator = I_;
            using const_iterator = typename ConstRow_::const_iterator;
            // mutable aliases of the base range, kept in step by construction
            I_ wrBegin_;
            I_ wrEnd_;

            Row_(I_ begin, I_ end) : ConstRow_(begin, end), wrBegin_(begin), wrEnd_(end) {}
            Row_(I_ begin, int size) : ConstRow_(begin, size), wrBegin_(begin), wrEnd_(begin + size) {}

            // have to double-implement begin/end, otherwise non-const implementations hide the inherited const
            iterator begin() { return wrBegin_; }
            const_iterator begin() const { return ConstRow_::begin(); }
            iterator end() { return wrEnd_; }
            const_iterator end() const { return ConstRow_::end(); }
            E_& operator[](int col) { return *(wrBegin_ + col); }
            const E_& operator[](int col) const { return *(wrBegin_ + col); }
        };

        Row_ Row(int iRow) { return Row_(vals_.begin() + Offset(iRow, 0), cols_); }
        Row_ operator[](int iRow) { return Row(iRow); }

        // Iteration through columns is less efficient
        class ConstCol_ {
        public:
            template <typename RI_>
            struct Iterator_ // column iterator in terms of row iterator
            {
                RI_ val_;
                size_t stride_;
                Iterator_(RI_ val, size_t stride) : val_(val), stride_(stride) {}
                Iterator_& operator++() {
                    val_ += stride_;
                    return *this;
                }
                Iterator_ operator++(int) {
                    Iterator_ ret(*this);
                    val_ += stride_;
                    return ret;
                }
                Iterator_& operator--() {
                    val_ -= stride_;
                    return *this;
                }
                Iterator_ operator--(int) {
                    Iterator_ ret(*this);
                    val_ -= stride_;
                    return ret;
                }
                Iterator_ operator+(size_t inc) {
                    Iterator_ ret(*this);
                    ret.val_ += inc * stride_;
                    return ret;
                }
                typename RI_::reference operator*() { return *val_; }
                bool operator==(const Iterator_& rhs) const {
                    REQUIRE(stride_ == rhs.stride_, "lhs stride size should be same with rhs");
                    return val_ == rhs.val_;
                }
                bool operator!=(const Iterator_& rhs) const { return !this->operator==(rhs); }
                bool operator<(const Iterator_& rhs) const { return val_ < rhs.val_; }
                typename RI_::difference_type operator-(const Iterator_& rhs) const {
                    REQUIRE(stride_ == rhs.stride_, "lhs stride size should be same with rhs");
                    REQUIRE((val_ - rhs.val_) % stride_ == 0, "lhs and rhs should be in same column");
                    return (val_ - rhs.val_) / stride_;
                }
                using iterator_category = typename std::vector<E_>::iterator::iterator_category;
                using difference_type = typename std::vector<E_>::iterator::difference_type;
                using value_type = E_;
                using reference = const E_&;
                using pointer = const E_*;
            };

            operator Vector_<E_>() const { return Vector_<E_>(begin(), end()); }

        protected:
            Iterator_<CI_> begin_; // const view; Col_ carries the mutable alias
            size_t size_;

        public:
            using value_type = E_;
            using const_iterator = Iterator_<CI_>;
            ConstCol_(CI_ begin, size_t size, size_t stride) : begin_(begin, stride), size_(size) {}

            const_iterator begin() const { return begin_; }
            const_iterator end() const { return const_iterator(begin_.val_ + size_ * begin_.stride_, begin_.stride_); }
            [[nodiscard]] size_t size() const { return size_; }
            const E_& operator[](int row) const { return *(begin_.val_ + row * begin_.stride_); }
        };
        ConstCol_ Col(int iCol) const { return ConstCol_(vals_.cbegin() + iCol, static_cast<size_t>(rows_), static_cast<size_t>(cols_)); }

        class Col_ : public ConstCol_ {
            using ConstCol_::size_;

        public:
            using value_type = E_;
            using iterator = typename ConstCol_::template Iterator_<I_>; // mutable column iterator
            iterator wrBegin_;                                           // mutable alias of the base position

            Col_(I_ begin, size_t size, size_t stride) : ConstCol_(begin, size, stride), wrBegin_(begin, stride) {}

            iterator begin() const { return wrBegin_; }
            iterator end() const { return iterator(wrBegin_.val_ + size_ * wrBegin_.stride_, wrBegin_.stride_); }
            E_& operator[](int row) { return *(wrBegin_.val_ + row * wrBegin_.stride_); }

            using ConstCol_::size;
        };
        Col_ Col(int iCol) { return Col_(vals_.begin() + iCol, static_cast<size_t>(rows_), static_cast<size_t>(cols_)); }

        void Swap(Matrix_<E_>* other) {
            REQUIRE(other != nullptr, "can't swap with null");
            swap(*other);
        }

        void Fill(const E_& val) { vals_.Fill(val); }

        template <class T_> void operator*=(const T_& scale) { vals_ *= scale; }

        void Resize(int rows, int cols) {
            if (cols == cols_ && rows * rows_ > 0) {
                vals_.Resize(Extent(rows, cols));
                rows_ = rows;
            } else {
                const int n_copy = std::min(cols, cols_);
                Vector_<E_> new_vals(Extent(rows, cols));
                for (int ir = 0; ir < rows && ir < rows_; ++ir)
                    copy(vals_.begin() + ir * cols_, vals_.begin() + ir * cols_ + n_copy,
                         new_vals.begin() + static_cast<size_t>(ir) * static_cast<size_t>(cols));
                vals_.Swap(&new_vals);
                rows_ = rows;
                cols_ = cols;
            }
        }
    };
} // namespace Dal
