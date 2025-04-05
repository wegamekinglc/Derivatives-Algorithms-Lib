//
// Created by wegamekinglc on 4/5/25.
//

#include <gtest/gtest.h>
#include <dal/storage/_repository.hpp>
#include <dal/math/interp/interp.hpp>

using namespace Dal;

TEST(StorageTest, TestObjectAccess) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    Handle_<Interp1Linear_> src(new Interp1Linear_("interp", x, f));

    ObjectAccess_::Add(src, RepositoryErase_::Value_::NAME);

    auto matched_objects = ObjectAccess_::Find("interp");
    ASSERT_EQ(matched_objects.size(), 1);

    (void)ObjectAccess_::Erase("interp");
    matched_objects = ObjectAccess_::Find("interp");
    ASSERT_EQ(matched_objects.size(), 0);
}