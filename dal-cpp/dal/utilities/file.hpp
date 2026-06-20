//
// Created by wegam on 2021/1/6.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    template <class T_> class Vector_;

    namespace File {
        void Read(const String_& fileName, Vector_<String_>* dst);
        void Write(const String_& fileName, const Vector_<String_>& src);
        void Remove(const String_& fileName);
    } // namespace File
} // namespace Dal
