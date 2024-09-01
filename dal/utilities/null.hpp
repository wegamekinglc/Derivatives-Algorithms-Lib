//
// Created by wegam on 2023/3/18.
//

#pragma once

#include <limits>
#include <type_traits>

namespace Dal {
    namespace detail {

        template <bool> struct FloatingPointNull;

        // null value for floating-point types
        template <> struct FloatingPointNull<true> {
            constexpr static float nullvalue() {
                // a specific values that should fit into any Real
                return (std::numeric_limits<float>::max)();
            }
        };

        // null value for integer types
        template <> struct FloatingPointNull<false> {
            constexpr static int nullvalue() {
                // a specific values that should fit into any Integer
                return (std::numeric_limits<int>::max)();
            }
        };

    } // namespace detail

    //! template class providing a null value for a given type.
    template <class Type> class Null_;

    // default implementation for built-in types
    template <typename T> class Null_ {
    public:
        Null_() = default;
        operator T() const { return T(detail::FloatingPointNull<std::is_floating_point<T>::value>::nullvalue()); }
    };
} // namespace Dal
