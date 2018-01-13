//
// Created by Cheng Li on 2018/1/13.
//

#pragma once

#include <dal/utilities/algorithms.hpp>
#include <dal/math/vector.hpp>


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
    };

    template <class E_>
    void Matrix_<E_>::SetHook(int from) {
        for(auto ii = from; ii < hooks_.size(); ++ii)
            hooks_[ii] = vals_.begin() + ii * cols_;
    }
}