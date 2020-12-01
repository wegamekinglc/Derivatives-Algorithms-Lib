//
// Created by wegam on 2020/12/1.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {

    // template implementation of composite object with same base type as its elements

    template <class T_>
    struct ElementHolder_ { typedef std::shared_ptr<T_> type; };

    template <class T_>
    struct ElementHolder_<const T_> { typedef Handle_<T_> type; };

    template <class T_, class H_ = typename ElementHolder_<T_>::type>
    class Composite_ : public std::remove_const<T_>::type {
    public:
        typedef H_ element_t;

    protected:
        Vector_<element_t> contents_;

    public:
        void Append(const element_t& p) { contents_.push_back(p); }
        void Append(T_* p) { Append(element_t(p)); }
        int Size() const { return contents_.size(); }
        int Empty() const { return contents_.empty(); }

        T_* operator[](int i) {
            assert(i < contents_.size());
            return contents_[i].get();
        }
        const T_* operator[](int i) const {
            assert(i < contents_.size());
            return contents_[i].get();
        }

        // Constructible from anything that makes a T_
        template <typename... Args_> Composite_(Args_&&... args) : T_(std::forward<Args_>(args)...) {}
        // or from components plus the T_
        template <typename... Args_>
        Composite_(const Vector_<element_t>& contents, Args_&&... args)
            : T_(std::forward<Args_>(args)...), contents_(contents) {}
        // Copy constructible if T_ is clonable
        Composite_(const Composite_<T_>& src) {
            for (const auto& sc : src.contents_)
                Append(sc ? sc->Clone() : 0);
        }
        Composite_<T_>* Clone() const { return new Composite_<T_>(*this); }
        // Assignable if T_ is
        T_& operator=(const T_& rhs);
    };

    template <class T_>
    Composite_<T_>& CastComposite(T_* src) { return dynamic_cast<Composite_<T_>*>(src); }

    template <class T_>
    Composite_<T_>& CoerceComposite(T_* src) {
        assert(dynamic_cast<Composite_<T_>*>(src));
        return static_cast<Composite_<T_>&>(*src);
    }

    template <class T_>
    const Composite_<T_>& CastComposite(const T_* src) {
        return dynamic_cast<Composite_<T_>*>(src);
    }

    template <class T_>
    const Composite_<T_>& CoerceComposite(const T_* src) {
        assert(dynamic_cast<Composite_<T_>*>(src));
        return static_cast<Composite_<T_>&>(*src);
    }
}


