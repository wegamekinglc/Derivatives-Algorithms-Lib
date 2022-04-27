//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <cassert>
#include <dal/utilities/noncopyable.hpp>
#include <memory>
#include <utility>

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

using std::make_pair;
using std::pair;

template <class T_> using scoped_ptr = const std::unique_ptr<T_>;

namespace Dal {

    using Time_ = double;

    template <class = double> class Stack_;

    template <class = double> class Vector_;

    template <class = double> class Matrix_;

    template <class = double> class SquareMatrix_;

    class Dictionary_;

    constexpr const double EPSILON = 2e-14;
    constexpr const double INF = 1e29;
    constexpr const double PI = 3.1415926535897932;
    constexpr const double M_SQRT_2 = 1.4142135623730951;

    template <class T_> inline bool IsZero(const T_& x) { return x < Dal::EPSILON && -x < Dal::EPSILON; }

    template <class T_> inline bool IsPositive(const T_& x) { return x >= Dal::EPSILON; }

    template <class T_> inline bool IsNegative(const T_& x) { return x <= -Dal::EPSILON; }

    template <class T_> inline T_ Square(const T_& x) { return x * x; }

    template <class T_> inline T_ Cube(const T_& x) { return x * x * x; }

    template <class T_> inline T_ Max(const T_& a, const T_& b) { return a > b ? a : b; }

    template <class T_> inline T_ Min(const T_& a, const T_& b) { return a < b ? a : b; }

    struct Empty_ {};

    template <class T_> class Handle_ : public std::shared_ptr<const T_> {
        typedef typename std::shared_ptr<const T_> base_t;

    public:
        Handle_() : base_t() {}

        explicit Handle_(const T_* src) : base_t(src) {}

        explicit Handle_(const base_t& src) : base_t(src) {}

        bool IsEmpty() const { return !base_t::get(); }
    };

    template <class T_, class U_> Handle_<T_> handle_cast(const std::shared_ptr<U_>& src) {
        return Handle_<T_>(std::dynamic_pointer_cast<const T_>(src));
    };

} // namespace Dal

#ifdef WIN32
    #ifdef IS_BASE
        #define BASE_EXPORT __declspec(dllexport)
    #else
        #define BASE_EXPORT __declspec(dllimport)
    #endif
#else
    #define BASE_EXPORT
#endif

#define RETURN_STATIC(...)                                                                                             \
    static __VA_ARGS__ RETVAL;                                                                                         \
    return RETVAL
#define DYN_PTR(n, t, s) t* n = dynamic_cast<t*>(s)
#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define VALUE_TYPE_OF(expr) std::remove_const_t<std::remove_reference_t<decltype(expr)>>
#define XXRUN_AT_LOAD(c, u1, u2) namespace{struct __run##u1##u2{__run##u1##u2(){c;}}; static const __run##u1##u2 runAtLoad##u1##u2;}	// can't consume a semicolon after the macro, because we need to wrap in a local namespace
#define XRUN_AT_LOAD(c, u1, u2) XXRUN_AT_LOAD(c, u1, u2)
#define RUN_AT_LOAD(code) XRUN_AT_LOAD(code, __COUNTER__, __LINE__)
