//
// Created by Cheng Li on 2018/1/13.
//

#pragma once

#include <type_traits>

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
        // one template over the iterator type keeps the const/mutable views in step
        // without derived-class shadowing
        template <class It_> struct RowView_ {
            It_ begin_;
            It_ end_;

            using value_type = E_;
            using iterator = It_;
            using const_iterator = CI_;

            RowView_(It_ begin, It_ end) : begin_(begin), end_(end) {}
            RowView_(It_ begin, int size) : begin_(begin), end_(begin + size) {}
            template <class OtherIt_, class = std::enable_if_t<std::is_convertible_v<OtherIt_, It_>>>
            RowView_(const RowView_<OtherIt_>& src) : begin_(src.begin_), end_(src.end_) {}

            It_ begin() { return begin_; }
            CI_ begin() const { return begin_; }
            It_ end() { return end_; }
            CI_ end() const { return end_; }
            [[nodiscard]] int size() const { return static_cast<int>(end_ - begin_); }
            const E_& operator[](int col) const { return *(begin_ + col); }
            template <class It2_ = It_, class = std::enable_if_t<std::is_same_v<It2_, I_>>> E_& operator[](int col) { return *(begin_ + col); }
            const E_& front() const { return *begin_; }
            const E_& back() const { return *(end_ - 1); }
            operator Vector_<E_>() const { return Vector_<E_>(begin_, end_); }
        };
        using ConstRow_ = RowView_<CI_>;
        using Row_ = RowView_<I_>;

        ConstRow_ Row(int iRow) const { return ConstRow_(vals_.cbegin() + Offset(iRow, 0), cols_); }
        ConstRow_ operator[](int iRow) const { return Row(iRow); }
        Row_ Row(int iRow) { return Row_(vals_.begin() + Offset(iRow, 0), cols_); }
        Row_ operator[](int iRow) { return Row(iRow); }

        // Iteration through columns is less efficient
        template <class It_> class ColView_ {
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

            Iterator_<It_> begin_;
            size_t size_;

            using value_type = E_;
            using iterator = Iterator_<It_>;
            using const_iterator = Iterator_<CI_>;

            ColView_(It_ begin, size_t size, size_t stride) : begin_(begin, stride), size_(size) {}
            template <class OtherIt_, class = std::enable_if_t<std::is_convertible_v<OtherIt_, It_>>>
            ColView_(const ColView_<OtherIt_>& src) : begin_(src.begin_.val_, src.begin_.stride_), size_(src.size_) {}

            Iterator_<It_> begin() const { return begin_; }
            Iterator_<It_> end() const { return Iterator_<It_>(begin_.val_ + size_ * begin_.stride_, begin_.stride_); }
            [[nodiscard]] size_t size() const { return size_; }
            const E_& operator[](int row) const { return *(begin_.val_ + row * begin_.stride_); }
            template <class It2_ = It_, class = std::enable_if_t<std::is_same_v<It2_, I_>>> E_& operator[](int row) {
                return *(begin_.val_ + row * begin_.stride_);
            }
            operator Vector_<E_>() const { return Vector_<E_>(begin(), end()); }
        };
        using ConstCol_ = ColView_<CI_>;
        using Col_ = ColView_<I_>;

        ConstCol_ Col(int iCol) const { return ConstCol_(vals_.cbegin() + iCol, static_cast<size_t>(rows_), static_cast<size_t>(cols_)); }
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
