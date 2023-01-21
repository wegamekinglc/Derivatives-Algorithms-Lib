//
// Created by wegam on 2023/1/21.
//

#pragma once
#include <dal/platform/platform.hpp>

namespace Dal {
    class String_;
    class Storable_;

    namespace JSON {
        Handle_<Storable_> ReadString(const String_& src, bool quiet);
        Handle_<Storable_> ReadFile(const String_& filename, bool quiet);
        void WriteFile(const Storable_& object, const String_& filename);
        String_ WriteString(const Storable_& object);
    }
}
