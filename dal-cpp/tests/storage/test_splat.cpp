//
// Created by wegamekinglc on 2020/11/24.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/splat.hpp>
#include <dal/utilities/file.hpp>

using namespace Dal;

TEST(StorageTest, TestSplatAndUnSplat) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    auto dst = Splat(*src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(StorageTest, TestSplatFileAndUnSplatFile) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    SplatFile("src.csv", *src);
    Handle_<Storable_> rtn = UnSplatFile("src.csv", true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
    File::Remove("src.csv");
}
