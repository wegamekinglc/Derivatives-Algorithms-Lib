//
// Created by wegam on 2023/1/26.
//

#pragma once
#include <type_traits>

namespace Dal::Script {
    template <typename... Vs> struct Pack;

    template <typename V> struct Pack<V> {
        template <class T> static constexpr bool includes() { return std::is_same<V, T>::value; }
    };

    template <typename V, typename... Vs> struct Pack<V, Vs...> {
        template <class T> static constexpr bool includes() {
            return Pack<V>::template includes<T>() || Pack<Vs...>::template includes<T>();
        }
    };
}
