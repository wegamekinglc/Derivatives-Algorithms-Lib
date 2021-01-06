//
// Created by wegam on 2021/1/6.
//

#include <dal/platform/platform.hpp>
#include <dal/utilities/file.hpp>
#include <dal/math/vectors.hpp>
#include <fstream>
#include <dal/platform/strict.hpp>

namespace Dal::File {
    void Read(const String_& file_name, Vector_<String_>* dst) {
        std::ifstream  src(file_name.c_str());
        char buf[2048];
        while (src.getline(buf, 2048))
            dst->emplace_back(buf);
    }
}