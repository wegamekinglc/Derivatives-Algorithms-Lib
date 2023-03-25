//
// Created by wegam on 2023/3/18.
//

#include <gtest/gtest.h>
#include "dal/math/pde/meshers/uniform1dmesher.hpp"

using namespace Dal;

TEST(MeshersTest, TestUniform1DMesher) {
    const int n = 51;
    Uniform1DMesher_ x(0.0, 5.0, n);
    ASSERT_NEAR(x.DPlus(0), 0.1, 1e-8);
    ASSERT_NEAR(x.Location(n - 1), 5.0, 1e-8);
}
