//
// Created by wegam on 2021/1/6.
//

#include <cstdio>
#include <fstream>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/file.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::File {
    void Read(const String_& fileName, Vector_<String_>* dst) {
        std::ifstream src(fileName.c_str());
        char buf[2048];
        while (src.getline(buf, 2048))
            dst->emplace_back(buf);
        src.close();
    }

    void Write(const String_& fileName, const Vector_<String_>& src) {
        std::ofstream dst(fileName.c_str());
        for (const auto& line : src)
            dst << line << std::endl;
        dst.close();
    }

    void Remove(const String_& fileName) {
        REQUIRE(remove(fileName.c_str()) == 0, "file remove failed");
    }
} // namespace Dal::File
