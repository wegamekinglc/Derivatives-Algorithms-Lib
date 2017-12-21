//
// Created by Cheng Li on 2017/12/21.
//

#pragma once

#include <vector>

template <class E_>
class Vector_: private std::vector<E_> {
    typedef std::vector<E_> base_t;
public:
    Vector_():base_t() {}
    explicit Vector_(int size):base_t(size) {}
    Vector_(int size, const E_& fill):base_t(size, fill) {}
    template <class I_> Vector_(I_ start, I_ end): base_t(start, end) {}
    Vector_(const std::initializer_list<E_>& args): base_t(args) {}


    using std::vector<E_>::size;
    using std::vector<E_>::empty;
    using std::vector<E_>::operator[];
};
