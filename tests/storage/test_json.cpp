//
// Created by wegam on 2023/1/22.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

TEST(JsonTest, TestJSONStore) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ src("interp", x, f);

    auto dst = JSON::WriteString(src);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    Handle_<Interp1Linear_> val(std::dynamic_pointer_cast<const Interp1Linear_>(rtn));
    ASSERT_EQ(x, val->x());
    ASSERT_EQ(f, val->f());
}

TEST(JsonTest, TestJSONStoreFile) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ src("interp", x, f);

    JSON::WriteFile(src, "src.json");
    Handle_<Storable_> rtn = JSON::ReadFile("src.json", true);
    Handle_<Interp1Linear_> val(std::dynamic_pointer_cast<const Interp1Linear_>(rtn));
    ASSERT_EQ(x, val->x());
    ASSERT_EQ(f, val->f());
    File::Remove("src.json");
}

