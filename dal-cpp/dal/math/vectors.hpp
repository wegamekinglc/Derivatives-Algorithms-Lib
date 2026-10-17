//
// Created by Cheng Li on 2017/12/21.
//

#pragma once

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
#include <dal/math/bufferallocation.hpp>
#include <dal/platform/host.hpp>

namespace Dal {
    namespace Vector {
        void RequireSameSize(size_t lhs, size_t rhs);
        void RequireAtLeastTwoPoints(size_t points);
    }
    /*
     * PRIVATE INHERITANCE from STL class vector
     * DON'T add any member variable to this class
     * DON'T override any methods from vector
     */
    template <class E_> class Vector_ : private std::vector<E_, Detail::VectorAllocator_<E_>> {
        using base_t = std::vector<E_, Detail::VectorAllocator_<E_>>;

    public:
        Vector_() : base_t() {}

        explicit Vector_(size_t size) : base_t(size) {}

        Vector_(size_t size, const E_& fill) : base_t(size, fill) {}

        template <class I_> Vector_(I_ start, I_ end) : base_t(start, end) {}

        Vector_(const std::initializer_list<E_>& args) : base_t(args) {}

        void Swap(Vector_<E_>* other) { base_t::swap(*other); }

        void Fill(const E_& val) { std::fill(begin(), end(), val); }

        void Resize(size_t new_size) { base_t::resize(new_size); }

        // capture by value: the operand may alias one of our elements, and the write must not
        // change the value seen by later elements (the snapshot std::bind used to take)
        template <class T_> void operator*=(const T_& scale) {
            std::transform(begin(), end(), begin(), [scale](const E_& val) { return val * scale; });
        }

        template <class T_> void operator+=(const T_& shift) {
            std::transform(begin(), end(), begin(), [shift](const E_& val) { return val + shift; });
        }

        template <class T_> void operator-=(const T_& shift) {
            std::transform(begin(), end(), begin(), [shift](const E_& val) { return val - shift; });
        }

        template <class T_> void operator+=(const Vector_<T_>& other) {
            Vector::RequireSameSize(size(), other.size());
            std::transform(begin(), end(), other.begin(), begin(), std::plus<E_>());
        }

        template <class T_> void operator-=(const Vector_<T_>& other) {
            Vector::RequireSameSize(size(), other.size());
            std::transform(begin(), end(), other.begin(), begin(), std::minus<E_>());
        }

        template <class I_> void Assign(I_ begin, I_ end) { base_t::assign(begin, end); }

        template <class I_> void Append(I_ iBegin, I_ iEnd) { base_t::insert(end(), iBegin, iEnd); }

        template <class C_> void Append(const C_& other) { base_t::insert(end(), other.begin(), other.end()); }

        bool operator==(const Vector_<E_>& rhs) const;

        bool operator!=(const Vector_<E_>& rhs) const;

        using typename base_t::iterator;
        using typename base_t::const_iterator;
        using typename base_t::reference;
        using typename base_t::const_reference;
        using typename base_t::value_type;

        using base_t::size;
        using base_t::capacity;
        using base_t::empty;
        using base_t::operator[];
        using base_t::begin;
        using base_t::cbegin;
        using base_t::end;
        using base_t::rbegin;
        using base_t::rend;
        using base_t::front;
        using base_t::back;
        using base_t::erase;
        using base_t::push_back;
        using base_t::pop_back;
        using base_t::reserve;
        using base_t::clear;

        // std::vector<bool> has no data(); expose the raw storage for the other element types
        template <class E2_ = E_, class = std::enable_if_t<!std::is_same_v<E2_, bool>>> E_* data() noexcept { return base_t::data(); }
        template <class E2_ = E_, class = std::enable_if_t<!std::is_same_v<E2_, bool>>> const E_* data() const noexcept { return base_t::data(); }

        E_& operator()(size_t i) { return (*this)[i];}
        const E_& operator()(size_t i) const { return (*this)[i];}

        // emplace_back is a special case; because it is not part of std::vector<bool>, we have to explicitly forward
        template <class... ValType> void emplace_back(ValType&&... Val) { base_t::emplace_back(std::forward<ValType>(Val)...); }
    };

    template <class C1_, class C2_> bool EqualElements(const C1_& lhs, const C2_& rhs) {
        for (auto pl = lhs.begin(), pr = rhs.begin(); pl != lhs.end(); ++pl, ++pr)
            if (*pl != *pr)
                return false;
        return true;
    }

    template <class E_> bool Equal(const Vector_<E_>& lhs, const Vector_<E_>& rhs) {
        return lhs.size() == rhs.size() && EqualElements(lhs, rhs);
    }

    namespace Vector {
        template <class E_> Vector_<E_> V1(const E_& val) { return Vector_<E_>(1, val); }

        template <class E1_, class C2_> Vector_<E1_> Join(const Vector_<E1_>& c1, const C2_& c2) {
            auto ret_val = c1;
            ret_val.Append(c2);
            return ret_val;
        };

        Vector_<int> UpTo(int n);

        template <class E_> Vector_<E_> XRange(E_ start, E_ finish, size_t points) {
            Vector::RequireAtLeastTwoPoints(points);
            Vector_<E_> x(points);
            E_ dx = (finish - start) / (points - 1);
            for (size_t i = 0; i < points - 1; ++i)
                x[i] = start + i * dx;
            x[points - 1] = finish;
            return x;
        }
    } // namespace Vector

    template <class E_> FORCE_INLINE bool Vector_<E_>::operator==(const Vector_<E_>& rhs) const { return Equal(*this, rhs); }

    template <class E_> FORCE_INLINE bool Vector_<E_>::operator!=(const Vector_<E_>& rhs) const { return !Equal(*this, rhs); }

    template <class E_> FORCE_INLINE auto operator*(const Vector_<E_>& left, const E_& right) {
        Vector_<E_> ret(left.size());
        std::transform(left.begin(), left.end(), ret.begin(), [&right](const E_& val) { return right * val; });
        return ret;
    }

    template <class E_> FORCE_INLINE auto operator*(const E_& left, const Vector_<E_>& right) { return right * left; }

    template <class E_> FORCE_INLINE auto operator*(const Vector_<E_>& left, const Vector_<E_>& right) {
        Vector::RequireSameSize(left.size(), right.size());
        Vector_<E_> ret(left.size());
        std::transform(left.begin(), left.end(), right.begin(), ret.begin(), [](const E_& val1, const E_& val2) { return val1 * val2; });
        return ret;
    }
} // namespace Dal
