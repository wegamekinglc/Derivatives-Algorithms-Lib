//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <dal/platform/config.hpp>
#include <dal/platform/initall.hpp>
#include <cassert>
#include <dal/utilities/noncopyable.hpp>
#include <dal/utilities/null.hpp>
#include <dal/platform/host.hpp>
#include <memory>
#include <utility>
#include <cmath>
#include <set>
#include <map>
#include <cmath>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

using std::make_pair;
using std::pair;

using Int = int64_t;

template <class T_> using scoped_ptr = const std::unique_ptr<T_>;

namespace Dal {

    template <class = double, size_t DefaultSize = 64> class Stack_;

    template <class = double> class Vector_;

    template <class = double> class Matrix_;

    template <class = double> class SquareMatrix_;

    template <class = double> class ArrayN_;

    template <class = double> class Cube_;

    class Dictionary_;

    constexpr double EPSILON = 2e-14;
    constexpr double INF = 1e29;
    constexpr double PI = 3.1415926535897932;
    constexpr double M_SQRT_2 = 1.4142135623730951;
    constexpr double M_SQRT_2_PI = 2.5066282746310002;
    constexpr double ONE_MINUS_EPS = 0.999999999999;

    template <class T_> constexpr bool IsZero(const T_& x) { return x < Dal::EPSILON && -x < Dal::EPSILON; }

    template <class T_> constexpr bool IsPositive(const T_& x) { return x >= Dal::EPSILON; }

    template <class T_> constexpr bool IsNegative(const T_& x) { return x <= -Dal::EPSILON; }

    template <class T_> constexpr bool IsClose(const T_& x, const T_& y) {
        T_ diff = std::fabs(x-y);
        constexpr double tolerance = 42 * Dal::EPSILON;

        if (x == 0.0 || y == 0.0)
            return diff < (tolerance * tolerance);
        return diff <= tolerance * std::fabs(x) && diff <= tolerance * std::fabs(y);
    }

    FORCE_INLINE constexpr double Square(double x) { return x * x; }
    FORCE_INLINE constexpr double Cube(double x) { return x * x * x; }

    struct Empty_ {};

    template <class T_> class Handle_ : public std::shared_ptr<const T_> {
        using base_t = std::shared_ptr<const T_>;

    public:
        Handle_() : base_t() {}

        explicit Handle_(const T_* src) : base_t(src) {}

        explicit Handle_(const base_t& src) : base_t(src) {}
        [[nodiscard]] bool IsEmpty() const { return !base_t::get(); }
    };

    template <class T_, class U_> Handle_<T_> handle_cast(const std::shared_ptr<U_>& src) {
        return Handle_<T_>(std::dynamic_pointer_cast<const T_>(src));
    };

    template<class T_, class... Args > Handle_<T_> make_handle(Args&&... args) {
        return std::make_shared<const T_>(std::forward<Args>(args)...);
    }

} // namespace Dal

#ifdef WIN32
    #ifdef IS_BASE
        #define BASE_EXPORT __declspec(dllexport)
    #else
        #define BASE_EXPORT
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
