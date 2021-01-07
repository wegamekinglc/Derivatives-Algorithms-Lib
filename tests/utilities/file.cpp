//
// Created by wegam on 2021/1/7.
//

#include <gtest/gtest.h>
#include <dal/utilities/file.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

TEST(FileTest, TestReadWrite) {
    Vector_<String_> src{String_("This is first line."), String_("This is second line.")};
    String_ file_name("src.csv");
    File::Write(file_name, src);

    Vector_<String_> dst;
    File::Read(file_name, &dst);

    ASSERT_EQ(dst.size(), 2);
    for (auto i = 0; i < src.size(); ++i)
        ASSERT_EQ(src[i], dst[i]);
}