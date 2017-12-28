//
// Created by Cheng Li on 2017/12/29.
//

#include <functional>
#include <dal/platform/platform.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/math/vector.hpp>
#include <gtest/gtest.h>



TEST(AlgorothmsTest, TestTransformWithContainer) {
    Vector_<> s1 = {1, 2, 3};
    Vector_<> s2(3);

    Transform(s1, std::negate<>(), &s2);

    for(int i=0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], -s1[i]);
    }
}