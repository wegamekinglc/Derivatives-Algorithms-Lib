//
// Created by wegam on 2023/1/26.
//

#pragma once
#include <type_traits>

namespace Dal::Script {
    template <typename... Vs_> struct Pack_;

    template <typename V_> struct Pack_<V_> {
        template <class T> static constexpr bool Includes() { return std::is_same<V_, T>::value; }
    };

    template <typename V_, typename... Vs_> struct Pack_<V_, Vs_...> {
        template <class T> static constexpr bool Includes() {
            return Pack_<V_>::template Includes<T>() || Pack_<Vs_...>::template Includes<T>();
        }
    };
}