//
// Created by wegam on 2022/2/21.
//

#pragma once

#include <iterator>

namespace Dal {
    template <class E_, size_t DefaultSize = 64>
    class Stack_ {

    private:
        E_* data_;
        int size_;
        int sp_;

    public:
        //	Constructor, destructor

        explicit Stack_(int chunk_size = DefaultSize) {
            size_ = chunk_size;
            if (size_)
                data_ = new E_[size_];
            else
                data_ = nullptr;
            sp_ = 0;
        }

        virtual ~Stack_() {
            if (data_)
                delete[] data_;
        }

        //	Copier, mover

        Stack_(const Stack_& rhs) {
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            if (size_)
                data_ = new E_[size_];
            else
                data_ = nullptr;
            if (sp_ > 0)
                copy(rhs.data_, rhs.data_ + sp_, data_);
        }

        Stack_& operator=(const Stack_& rhs) {
            if (this == &rhs)
                return *this;
            if (size_ < rhs.size_) {
                if (data_)
                    delete[] data_;
                if (rhs.size_)
                    data_ = new E_[rhs.size_];
                else
                    data_ = nullptr;
            }
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            if (sp_ > 0)
                std::copy(rhs.data_, rhs.data_ + sp_, data_);
            return *this;
        }

        Stack_(Stack_&& rhs) noexcept {
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            data_ = rhs.data_;
            rhs.data_ = nullptr;
            rhs.size_ = rhs.sp_ = 0;
        }

        Stack_& operator=(Stack_&& rhs) {
            if (this == &rhs)
                return *this;
            if (data_)
                delete[] data_;
            size_ = rhs.size_;
            sp_ = rhs.sp_;
            data_ = rhs.data_;
            rhs.data_ = nullptr;
            rhs.size_ = rhs.sp_ = 0;

            return *this;
        }

        using iterator = std::reverse_iterator<E_*>;
        using const_iterator = std::reverse_iterator<const E_*>;

        inline iterator begin() { return std::reverse_iterator<E_*>(data_ + sp_); }
        inline const_iterator begin() const { return std::reverse_iterator<const E_*>(data_ + sp_); }
        inline iterator end() { return std::reverse_iterator<E_*>(data_); }
        inline const_iterator end() const { return std::reverse_iterator<const E_*>(data_); }

        template <typename T_> void Push(T_&& value) {
            data_[sp_] = std::forward<T_>(value);
            ++sp_;
            if (sp_ >= size_) {
                E_* newData = new E_[size_ << 1];

#ifdef _MSC_VER
                std::move(data_, data_ + size_, stdext::make_unchecked_array_iterator(newData));
#else
                std::move(data_, data_ + size_, newData);
#endif

                delete[] data_;
                data_ = newData;
                size_ <<= 1;
            }
        }

        inline E_& Top() { return data_[sp_ - 1]; }
        inline const E_& Top() const { return data_[sp_ - 1]; }
        inline E_& operator[](const size_t i) { return data_[sp_ - 1 - i]; }
        inline const E_& operator[](const size_t i) const { return data_[sp_ - 1 - i]; }

        inline E_ TopAndPop() {
            --sp_;
            return std::move(data_[sp_]);
        }

        void Pop() { --sp_; }
        void Pop(const size_t n) { sp_ -= n; }
        void Reset() { sp_ = 0; }

        void Clear() {
            if (data_)
                delete[] data_;
            data_ = nullptr;
            size_ = sp_ = 0;
        }

        int Size() const { return sp_; }
        int Capacity() const { return size_; }
        bool IsEmpty() const { return sp_ == 0; }
    };
} // namespace Dal