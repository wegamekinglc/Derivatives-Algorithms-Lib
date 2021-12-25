//
// Created by wegamekinglc on 2020/12/19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include "dal/math/random/pseudorandom.hpp"

using namespace Dal;

TEST(PseudoRandomTest, TestNewPseudoRandomIRN) {
    auto n_dim = 10;
    auto n_samples = 10000000;
    std::unique_ptr<Random_> gen(New(RNGType_("IRN"), 1024, n_dim));
    Vector_<> values(n_dim);

    Vector_<> mean(n_dim, 0.0);
    Vector_<> var(n_dim, 0.0);
    for(auto i = 0; i < n_samples; ++i) {
        gen->FillUniform(&values);
        for (auto k = 0 ; k < n_dim; ++k) {
            mean[k] += values[k];
            var[k] += Square(values[k] - 0.5);
        }
    }
    for (auto k = 0 ; k < n_dim; ++k) {
        mean[k] /= n_samples;
        var[k] /= n_samples;
        ASSERT_NEAR(mean[k], 0.5, 1e-4);
        ASSERT_NEAR(var[k], 1. / 12, 1e-4);
    }

    gen->FillNormal(&values);
    mean = Vector_<>(n_dim, 0.0);
    var = Vector_<>(n_dim, 0.0);
    for(auto i = 0; i < n_samples; ++i) {
        gen->FillNormal(&values);
        for (auto k = 0 ; k < n_dim; ++k) {
            mean[k] += values[k];
            var[k] += Square(values[k]);
        }
    }
    for (auto k = 0 ; k < n_dim; ++k) {
        mean[k] /= n_samples;
        var[k] /= n_samples;
        ASSERT_NEAR(mean[k], 0.0, 1e-3);
        ASSERT_NEAR(var[k], 1.0, 1e-3);
    }
}


TEST(PseudoRandomTest, TestNewPseudoRandomMRG32) {
    auto n_dim = 10;
    auto n_samples = 10000000;
    std::unique_ptr<Random_> gen(New(RNGType_("MRG32"), 1024, n_dim));
    Vector_<> values(n_dim);

    Vector_<> mean(n_dim, 0.0);
    Vector_<> var(n_dim, 0.0);
    for(auto i = 0; i < n_samples; ++i) {
        gen->FillUniform(&values);
        for (auto k = 0 ; k < n_dim; ++k) {
            mean[k] += values[k];
            var[k] += Square(values[k] - 0.5);
        }
    }
    for (auto k = 0 ; k < n_dim; ++k) {
        mean[k] /= n_samples;
        var[k] /= n_samples;
        ASSERT_NEAR(mean[k], 0.5, 1e-4);
        ASSERT_NEAR(var[k], 1. / 12, 1e-4);
    }

    gen->FillNormal(&values);
    mean = Vector_<>(n_dim, 0.0);
    var = Vector_<>(n_dim, 0.0);
    for(auto i = 0; i < n_samples; ++i) {
        gen->FillNormal(&values);
        for (auto k = 0 ; k < n_dim; ++k) {
            mean[k] += values[k];
            var[k] += Square(values[k]);
        }
    }
    for (auto k = 0 ; k < n_dim; ++k) {
        mean[k] /= n_samples;
        var[k] /= n_samples;
        ASSERT_NEAR(mean[k], 0.0, 1e-3);
        ASSERT_NEAR(var[k], 1.0, 1e-3);
    }
}

TEST(PseudoRandomTest, TestPseudoRandomClone) {
    auto n_dim = 10;
    std::unique_ptr<Random_> gen(New(RNGType_("MRG32"), 1024, n_dim));
    std::unique_ptr<Random_> gen2(gen->Clone());
    ASSERT_EQ(gen2->NDim(), n_dim);

    gen.reset(New(RNGType_("MRG32"), 1024, n_dim));
    gen2.reset(gen->Clone());
    ASSERT_EQ(gen2->NDim(), n_dim);
}
