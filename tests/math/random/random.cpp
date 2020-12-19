//
// Created by wegamekinglc on 2020/12/19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/random/random.hpp>

using namespace Dal;

TEST(RandomTest, TestNewRandom) {
    std::unique_ptr<Random_> gen(Random::New(1024));
    auto n = 10000000;
    Vector_<> values(n);

    gen->FillUniform(&values);
    auto mean = 0.0;
    auto var = 0.0;
    for(const auto& v : values) {
        mean += v;
        var += Square(v - 0.5);
    }

    mean /= values.size();
    var = var / values.size();
    ASSERT_NEAR(mean, 0.5, 1e-4);
    ASSERT_NEAR(var, 1. / 12, 1e-4);

    gen->FillNormal(&values);
    mean = 0.0;
    var = 0.0;
    for(const auto& v : values) {
        mean += v;
        var += Square(v);
    }
    mean /= values.size();
    var = var / values.size();
    ASSERT_NEAR(mean, 0.0, 1e-3);
    ASSERT_NEAR(var, 1.0, 1e-3);
}
