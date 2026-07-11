//
// Created by wegam on 2022/2/21.
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    template <class E_, size_t DefaultSize> class Stack_ {
        E_* data_ = nullptr;
        int size_ = 0;
        int sp_ = 0;

        void Grow() {
            REQUIRE(size_ <= std::numeric_limits<int>::max() / 2, "stack capacity overflow");
            const int defaultSize = DefaultSize > 0 ? static_cast<int>(DefaultSize) : 1;
            const int newSize = size_ > 0 ? size_ * 2 : defaultSize;
            E_* newData = new E_[newSize];
            if (sp_ > 0)
                std::move(data_, data_ + sp_, newData);
            delete[] data_;
            data_ = newData;
            size_ = newSize;
        }

    public:
        explicit Stack_(int chunkSize = static_cast<int>(DefaultSize)) : size_(chunkSize) {
            static_assert(DefaultSize <= static_cast<size_t>(std::numeric_limits<int>::max()), "Default stack size is too large");
            REQUIRE(chunkSize >= 0, "stack capacity cannot be negative");
            if (size_ > 0)
                data_ = new E_[size_];
        }

        virtual ~Stack_() { delete[] data_; }

        Stack_(const Stack_& rhs) : size_(rhs.size_), sp_(rhs.sp_) {
            if (size_ > 0)
                data_ = new E_[size_];
            if (sp_ > 0)
                std::copy(rhs.data_, rhs.data_ + sp_, data_);
        }

        Stack_& operator=(const Stack_& rhs) {
            if (this == &rhs)
                return *this;
            if (size_ < rhs.size_) {
                E_* newData = rhs.size_ > 0 ? new E_[rhs.size_] : nullptr;
                delete[] data_;
                data_ = newData;
            }
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            if (sp_ > 0)
                std::copy(rhs.data_, rhs.data_ + sp_, data_);
            return *this;
        }

        Stack_(Stack_&& rhs) noexcept : data_(rhs.data_), size_(rhs.size_), sp_(rhs.sp_) {
            rhs.data_ = nullptr;
            rhs.size_ = rhs.sp_ = 0;
        }

        Stack_& operator=(Stack_&& rhs) noexcept {
            if (this == &rhs)
                return *this;
            delete[] data_;
            data_ = rhs.data_;
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            rhs.data_ = nullptr;
            rhs.size_ = rhs.sp_ = 0;
            return *this;
        }

        using iterator = std::reverse_iterator<E_*>;
        using const_iterator = std::reverse_iterator<const E_*>;

        FORCE_INLINE iterator begin() { return iterator(data_ ? data_ + sp_ : data_); }
        FORCE_INLINE const_iterator begin() const { return const_iterator(data_ ? data_ + sp_ : data_); }
        FORCE_INLINE iterator end() { return iterator(data_); }
        FORCE_INLINE const_iterator end() const { return const_iterator(data_); }

        template <typename T_> void Push(T_&& value) {
            if (sp_ == size_) {
                E_ materialized(std::forward<T_>(value));
                Grow();
                data_[sp_++] = std::move(materialized);
            } else {
                data_[sp_++] = std::forward<T_>(value);
            }
        }

        FORCE_INLINE E_& Top() {
            REQUIRE(sp_ > 0, "cannot read the top of an empty stack");
            return data_[sp_ - 1];
        }
        FORCE_INLINE const E_& Top() const {
            REQUIRE(sp_ > 0, "cannot read the top of an empty stack");
            return data_[sp_ - 1];
        }
        FORCE_INLINE E_& operator[](const size_t i) {
            REQUIRE(i < static_cast<size_t>(sp_), "stack index is out of bounds");
            return data_[sp_ - 1 - i];
        }
        FORCE_INLINE const E_& operator[](const size_t i) const {
            REQUIRE(i < static_cast<size_t>(sp_), "stack index is out of bounds");
            return data_[sp_ - 1 - i];
        }

        FORCE_INLINE E_ TopAndPop() {
            REQUIRE(sp_ > 0, "cannot pop an empty stack");
            return std::move(data_[--sp_]);
        }

        FORCE_INLINE void Pop() {
            REQUIRE(sp_ > 0, "cannot pop an empty stack");
            --sp_;
        }
        FORCE_INLINE void Pop(const size_t n) {
            REQUIRE(n <= static_cast<size_t>(sp_), "cannot pop past the bottom of a stack");
            sp_ -= static_cast<int>(n);
        }
        FORCE_INLINE void Reset() { sp_ = 0; }

        void Clear() {
            delete[] data_;
            data_ = nullptr;
            size_ = sp_ = 0;
        }

        [[nodiscard]] FORCE_INLINE int Size() const { return sp_; }
        [[nodiscard]] FORCE_INLINE int Capacity() const { return size_; }
        [[nodiscard]] FORCE_INLINE bool IsEmpty() const { return sp_ == 0; }
    };

    template <class T = double, size_t SIZE = 128> class StaticStack_ {
        static_assert(SIZE > 0, "StaticStack_ capacity must be positive");

        T data_[SIZE]{};
        size_t sp_ = 0;

    public:
        template <typename T2> FORCE_INLINE void Push(T2&& value) {
            REQUIRE(sp_ < SIZE, "static stack capacity exceeded");
            data_[sp_++] = T(std::forward<T2>(value));
        }

        FORCE_INLINE T& Top() {
            REQUIRE(sp_ > 0, "cannot read the top of an empty stack");
            return data_[sp_ - 1];
        }

        FORCE_INLINE const T& Top() const {
            REQUIRE(sp_ > 0, "cannot read the top of an empty stack");
            return data_[sp_ - 1];
        }

        FORCE_INLINE T& operator[](size_t i) {
            REQUIRE(i < sp_, "stack index is out of bounds");
            return data_[sp_ - 1 - i];
        }

        FORCE_INLINE const T& operator[](size_t i) const {
            REQUIRE(i < sp_, "stack index is out of bounds");
            return data_[sp_ - 1 - i];
        }

        FORCE_INLINE T TopAndPop() {
            REQUIRE(sp_ > 0, "cannot pop an empty stack");
            return std::move(data_[--sp_]);
        }
        FORCE_INLINE T PopAndTop() {
            REQUIRE(sp_ > 1, "a new top does not exist after popping the stack");
            --sp_;
            return std::move(data_[sp_ - 1]);
        }

        FORCE_INLINE void Pop() {
            REQUIRE(sp_ > 0, "cannot pop an empty stack");
            --sp_;
        }

        FORCE_INLINE void Pop(size_t n) {
            REQUIRE(n <= sp_, "cannot pop past the bottom of a stack");
            sp_ -= n;
        }

        FORCE_INLINE void Reset() { sp_ = 0; }

        [[nodiscard]] FORCE_INLINE size_t Size() const { return sp_; }

        [[nodiscard]] FORCE_INLINE bool IsEmpty() const { return sp_ == 0; }
    };
} // namespace Dal
