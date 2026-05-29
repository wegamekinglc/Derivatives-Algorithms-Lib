//
// Created by wegamekinglc on 2020/5/2.
//

#pragma once

#include <functional>

namespace Dal {

    template <class P_> struct GetFirst_ {
        auto operator()(const P_& pair) const { return pair.first; }
    };

    template <class P_> struct GetSecond_ {
        auto operator()(const P_& pair) const { return pair.second; }
    };
    // note these functions do not actually "get" anything -- they return a functor to get
    template <class P_> GetFirst_<P_> GetFirst(const P_&) { return GetFirst_<P_>(); }

    template <class P_> GetSecond_<P_> GetSecond(const P_&) { return GetSecond_<P_>(); }

    template <class T_> struct LinearIncrement_ {
        T_ scale_;
        explicit LinearIncrement_(const T_& s) : scale_(s) {}
        template <class U_> U_ operator()(const U_& pvs, const U_& incr) const { return pvs + scale_ * incr; }
    };

    template <class T_> LinearIncrement_<T_> LinearIncrement(const T_& scale) { return LinearIncrement_<T_>(scale); }

    template <class T_> struct AverageIn_ {
        T_ newFrac_;
        explicit AverageIn_(const T_& n) : newFrac_(n) {}
        template <class U_> U_ operator()(const U_& pvs, const U_& nw) { return pvs + newFrac_ * (nw - pvs); }
    };

    template <class T_> inline AverageIn_<T_> AverageIn(const T_& new_frac) { return AverageIn_(new_frac); }

    template <class T_> const T_& Dereference(const T_* p, const T_& v) { return p ? *p : v; }

    template <class B_, class D_> B_* GetShared(const std::shared_ptr<D_>& src) { return src.get(); }

    template <class A_, class R_> std::function<R_(A_)> AsFunctor(R_ (*func)(A_)) {
        return std::function<R_(A_)>(func);
    }

    template <class T_> struct Identity_ {
        const T_& operator()(const T_& arg) const { return arg; }
    };

    // functor for lookup-in-array, though lambdas are often easier
    template <class C_, class XK_ = C_> struct ArrayFunctor_ {
        XK_ val_;
        explicit ArrayFunctor_(const C_& val) : val_(val) {}
        const typename C_::value_type& operator()(int ii) const { return val_[ii]; }
    };

    // returns an ephemeral class containing a reference to src
    template <class C_> ArrayFunctor_<C_, const C_&> XLookupIn(const C_& src) {
        return ArrayFunctor_<C_, const C_&>(src);
    }

    // takes a copy of src
    template <class C_> ArrayFunctor_<C_> LookupIn(const C_& src) { return ArrayFunctor_<C_, C_>(src); }

    template <class SRC_, class DST_> struct ConstructCast_ {
        DST_ operator()(const SRC_& src) const { return DST_(src); }
    };
} // namespace Dal