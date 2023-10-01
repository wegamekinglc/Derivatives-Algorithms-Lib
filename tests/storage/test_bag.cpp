//
// Created by wegam on 2023/1/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/storage/bag.hpp>
#include <dal/storage/splat.hpp>
#include <dal/math/interp/interp.hpp>

using namespace Dal;

TEST(BagTest, TestBag) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    std::multimap<String_, Handle_<Storable_>> map;
    map.insert(std::make_pair(String_("interp1"), Handle_<Interp1Linear_>(new Interp1Linear_("interp", x, f))));
    map.insert(std::make_pair(String_("interp2"), Handle_<Interp1Linear_>(new Interp1Linear_("interp", x, f))));

    Bag_ bag("bag", map);
    auto dst = Splat(bag);

    Handle_<Storable_> rtn = UnSplat(dst, true);
    Handle_<Bag_> val(std::dynamic_pointer_cast<const Bag_>(rtn));

    auto iter = val->contents_.find("interp1");
    ASSERT_TRUE(iter != val->contents_.end());
    ASSERT_EQ(x, std::dynamic_pointer_cast<const Interp1Linear_>(iter->second)->x());
    ASSERT_EQ(f, std::dynamic_pointer_cast<const Interp1Linear_>(iter->second)->f());

    iter = val->contents_.find("interp2");
    ASSERT_TRUE(iter != val->contents_.end());
    ASSERT_EQ(x, std::dynamic_pointer_cast<const Interp1Linear_>(iter->second)->x());
    ASSERT_EQ(f, std::dynamic_pointer_cast<const Interp1Linear_>(iter->second)->f());

}

