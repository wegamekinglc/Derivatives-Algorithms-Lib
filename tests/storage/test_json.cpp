//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

TEST(StorageTest, TestJSONStore) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    auto dst = JSON::WriteString(*src);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(StorageTest, TestJSONStoreFile) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    JSON::WriteFile(*src, "src.json");
    Handle_<Storable_> rtn = JSON::ReadFile("src.json", true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
    File::Remove("src.json");
}

