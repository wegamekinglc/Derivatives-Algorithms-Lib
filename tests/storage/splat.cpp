//
// Created by wegamekinglc on 2020/11/24.
//

#include <dal/math/interp/interp.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/splat.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(SplatTest, TestSplatAndUnSplat) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Interp1Linear_ src("interp", x, f);

    auto dst = Splat(src);
    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Interp1Linear_> val(std::dynamic_pointer_cast<const Interp1Linear_>(rtn));
    ASSERT_EQ(x, val->x());
    ASSERT_EQ(f, val->f());
}