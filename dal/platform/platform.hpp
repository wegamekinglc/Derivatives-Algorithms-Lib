//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <cassert>
#include <memory>
#include <utility>
#include <dal/platform/strict.hpp>

#ifdef INFINITY
#undef INFINITY
#endif

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

using std::pair;
using std::make_pair;

template <class T_> using scoped_ptr = const std::unique_ptr<T_>;
template <class = double> class Vector_;
template <class = double> class Matrix_;
template <class =double> class SquareMatrix_;
class Dictionary_;


namespace dal {
    static const double EPSILON = 2e-14;
    static const double INFINITY = 1e29;
    static const double PI = 3.1415926535897932;
}

template <class T_> inline bool IsZero(const T_& x) { return x < dal::EPSILON && -x < dal::EPSILON;}
template <class T_> inline bool IsPositive(const T_& x) { return x >= dal::EPSILON;}
template <class T_> inline bool IsNegative(const T_& x) { return x <= -dal::EPSILON;}

template <class T_> inline T_ Square(const T_& x) { return x * x;}
template <class T_> inline T_ Cube(const T_& x) { return x * x * x;}
template <class T_> inline T_ Max(const T_& a, const T_& b) { return a > b ? a : b;}
template <class T_> inline T_ Min(const T_& a, const T_& b) { return a < b ? a : b;}
struct Empty_ {};

template <class T_>
class Handle_ : public std::shared_ptr<const T_> {
    typedef typename std::shared_ptr<const T_> base_t;
public:
    Handle_(): base_t() {}
    explicit Handle_(const T_* src): base_t(src) {}
    explicit Handle_(const base_t& src): base_t(src) {}
    bool isEmpty() const { return !base_t::get();}
};

template <class T_, class U_> Handle_<T_> handle_cast(const std::shared_ptr<U_>& src) {
    return Handle_<T_>(std::dynamic_pointer_cast<const T_>(src));
};

