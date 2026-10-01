//
// Created by wegam on 2021/1/7.
//

#include <gtest/gtest.h>

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

TEST(FileTest, TestReadLongLinesAndAppend) {
    const String_ fileName("dal_test_long_lines.txt");
    const Vector_<String_> lines{String_(5000, 'x'), "", "last"};
    File::Write(fileName, lines);
    Vector_<String_> result{"existing"};
    File::Read(fileName, &result);
    File::Remove(fileName);
    ASSERT_EQ(result.size(), 4);
    ASSERT_EQ(result[0], "existing");
    for (int i = 0; i < lines.size(); ++i)
        ASSERT_EQ(result[i + 1], lines[i]);
}

TEST(FileTest, TestReadWriteFailuresThrow) {
    Vector_<String_> result;
    ASSERT_THROW(File::Read("dal_missing_directory/file.txt", &result), Exception_);
    ASSERT_THROW(File::Write("dal_missing_directory/file.txt", {"line"}), Exception_);
}

TEST(FileTest, TestReadWrite) {
    Vector_<String_> src{String_("This is first line."), String_("This is second line.")};
    String_ file_name("src.csv");
    File::Write(file_name, src);

    Vector_<String_> dst;
    File::Read(file_name, &dst);

    ASSERT_EQ(dst.size(), 2);
    for (auto i = 0; i < src.size(); ++i)
        ASSERT_EQ(src[i], dst[i]);
    File::Remove(file_name);
}
