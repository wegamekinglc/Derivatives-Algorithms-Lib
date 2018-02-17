//
// Created by Cheng Li on 2018/1/13.
//

#pragma once

#include <dal/utilities/algorithms.hpp>
#include <dal/math/vectors.hpp>


namespace dal {
    template<class E_>
    class Matrix_ {
        Vector_<E_> vals_;
        int cols_;
        using I_ = typename Vector_<E_>::iterator;
        using CI_ = typename Vector_<E_>::const_iterator;
        using R_ = typename Vector_<E_>::reference;
        using CR_ = typename Vector_<E_>::const_reference;
        Vector_<I_> hooks_;

        void SetHook(int from=0);

    public:
        virtual ~Matrix_() = default;
        Matrix_(): cols_(0) {}
        Matrix_(int rows, int cols): vals_(static_cast<size_t >(rows * cols)),
                                     cols_(cols),
                                     hooks_(static_cast<size_t >(rows)) { SetHook(); vals_.Fill(E_());}
        Matrix_(const Matrix_& src): vals_(src.vals_),
                                     cols_(src.cols_),
                                     hooks_(src.hooks_.size()) { SetHook();}

        int Rows() const { return static_cast<int>(hooks_.size());}
        int Cols() const { return cols_;}
        bool Empty() const { return vals_.empty();}
        void Clear() { vals_.clear(); cols_ = 0; hooks_.clear();}
        CI_ First() const { return vals_.begin(); }
        CI_ Last() const { return  vals_.end();}

        CR_ operator()(int row, int col) const { return hooks_[row][col];}
        R_ operator()(int row, int col) { return hooks_[row][col];}

        // slices -- ephemeral containers of rows or columns
        class ConstRow_ {
        protected:
            I_ begin_;
            I_ end_;
        public:
            using value_type = E_;
            using const_iterator = typename Vector_<E_>::const_iterator;

            ConstRow_(I_ begin, I_ end): begin_(begin), end_(end) {}
            ConstRow_(I_ begin, int size): begin_(begin), end_(begin + size) {}

            const_iterator begin() const { return begin_; }
            const_iterator end() const { return end_; }
            int size() const { return static_cast<int>(end_ - begin_);}
            const E_& operator[](int col) const { return *(begin_ + col);}
            const E_& front() const { return *begin_;}
            const E_& back() const { return *(end_ - 1);}
        };

        ConstRow_ Row(int i_row) const { return ConstRow_(hooks_[i_row], cols_);}
        ConstRow_ operator[](int i_row) const { return Row(i_row);}

        struct Row_ : ConstRow_ {
            using iterator = I_;
            using const_iterator = typename ConstRow_::const_iterator;
            Row_(I_ begin, I_ end): ConstRow_(begin, end) {}
            Row_(I_ begin, int size): ConstRow_(begin, size) {}

            // have to double-implement begin/end, otherwise non-const implementations hide the inherited const
            iterator begin() { return ConstRow_::begin_;}
            const_iterator begin() const { return ConstRow_::begin_;}
            iterator end() { return ConstRow_::end_;}
            const_iterator end() const { return ConstRow_::end_;}
            E_& operator[](int col) { return *(ConstRow_::begin_ + col);}
            const E_& operator[](int col) const { return *(ConstRow_::begin_ + col);}
        };

        Row_ Row(int i_row) { return Row_(hooks_[i_row], cols_);}
        Row_ operator[](int i_row) { return Row(i_row);}
    };

    template <class E_>
    void Matrix_<E_>::SetHook(int from) {
        for(auto ii = from; ii < hooks_.size(); ++ii)
            hooks_[ii] = vals_.begin() + ii * cols_;
    }
}