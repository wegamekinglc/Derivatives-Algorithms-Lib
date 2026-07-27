//
// Created by wegam on 2023/1/21.
//

#pragma once

#include <cstddef>

namespace Dal {
    class String_;
    class Storable_;

    struct JSONReadOptions_ {
        static constexpr std::size_t DEFAULT_MAX_INPUT_BYTES = 50U * 1024U * 1024U;
        std::size_t maxInputBytes_ = DEFAULT_MAX_INPUT_BYTES;
    };

    namespace JSON {
        Handle_<Storable_> ReadString(const char* src, std::size_t length, const JSONReadOptions_& options);
        Handle_<Storable_> ReadString(const String_& src, bool quiet);
        Handle_<Storable_> ReadFile(const String_& filename, bool quiet);
        void WriteFile(const Storable_& object, const String_& filename);
        String_ WriteString(const Storable_& object);
    } // namespace JSON
} // namespace Dal
