//
// Created by wegam on 2023/1/26.
//

#pragma once

namespace Dal::Script {
    template <typename... Vs> struct Pack;

    template <typename V> struct Pack<typename V> {
        template <class T> static constexpr bool includes() { return false; }

        template <> static constexpr bool includes<V>() { return true; }
    };

    template <typename V, typename... Vs> struct Pack<V, Vs...> {
        template <class T> static constexpr bool includes() {
            return Pack<V>::template includes<T>() || Pack<Vs...>::template includes<T>();
        }
    };
}
