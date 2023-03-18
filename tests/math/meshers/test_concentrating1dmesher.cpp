//
// Created by wegam on 2023/3/18.
//

#include <gtest/gtest.h>
#include <dal/math/pde/meshers/concentrating1dmesher.hpp>

using namespace Dal;

TEST(MeshersTest, TestConcentrating1dMesher) {
    const int n = 51;
    Concentrating1dMesher_ x(0.0, 5.0, n, std::make_pair(2.5, 1000));
    ASSERT_NEAR(x.DPlus(0), 0.1, 1e-8);
    ASSERT_NEAR(x.Location(n - 1), 5.0, 1e-8);
}
