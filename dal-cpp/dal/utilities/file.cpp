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
        REQUIRE(dst != nullptr, "File read requires an output vector");
        REQUIRE(src.is_open(), "Cannot open file for reading: " + fileName);
        std::string line;
        while (std::getline(src, line))
            dst->emplace_back(line.begin(), line.end());
        REQUIRE(src.eof() && !src.bad(), "File read failed: " + fileName);
    }

    void Write(const String_& fileName, const Vector_<String_>& src) {
        std::ofstream dst(fileName.c_str());
        REQUIRE(dst.is_open(), "Cannot open file for writing: " + fileName);
        for (const auto& line : src)
            dst << line << '\n';
        dst.close();
        REQUIRE(!dst.fail(), "File write failed: " + fileName);
    }

    void Remove(const String_& fileName) {
        REQUIRE(remove(fileName.c_str()) == 0, "file remove failed");
    }
} // namespace Dal::File
