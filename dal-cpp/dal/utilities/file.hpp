//
// Created by wegam on 2021/1/6.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    template <class T_> class Vector_;

    namespace File {
        void Read(const String_& file_name, Vector_<String_>* dst);
        void Write(const String_& file_name, const Vector_<String_>& src);
        void Remove(const String_& file_name);
    } // namespace File
} // namespace Dal
