//
// Created by wegamekinglc on 2020/12/19.
//

#include <dal/math/operators.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>

using namespace Dal;


TEST(RandomTest, TestNewPseudoRandomIRN) {
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


TEST(PseudoRandomTest, TestNewPseudoRandomMRG32SkipTo) {
    int dim = 1;
    int seed = 1024;
    int size_to_skip = pow(2, 15);
    std::unique_ptr<Random_> gen(New(RNGType_("MRG32"), seed, dim));
    std::unique_ptr<Random_> gen2(New(RNGType_("MRG32"), seed, dim));

    Vector_<> data(dim);
    Vector_<> data2(dim);

    gen2->SkipTo(size_to_skip);
    for (int i = 0; i < size_to_skip; ++i)
        gen->FillUniform(&data);

    gen->FillUniform(&data);
    gen2->FillUniform(&data2);
    ASSERT_DOUBLE_EQ(data[0], data2[0]);

    dim = 10;
    gen = std::unique_ptr<Random_>(New(RNGType_("MRG32"), seed, dim));
    gen2 = std::unique_ptr<Random_>(New(RNGType_("MRG32"), seed, dim));

    data.Resize(dim);
    data2.Resize(dim);

    gen2->SkipTo(size_to_skip);
    for (int i = 0; i < size_to_skip; ++i)
        gen->FillUniform(&data);

    gen->FillUniform(&data);
    gen2->FillUniform(&data2);
    for (int k = 0; k < dim; ++k)
        ASSERT_DOUBLE_EQ(data[k], data2[k]);
}

TEST(RandomTest, NewPseudoRandomIRNPerformance) {
    int dim = 100;
    int seed = 1024;
    std::unique_ptr<Random_> gen(New(RNGType_("IRN"), seed, dim));

    int num_path = 2000000;
    Vector_<> dst(dim);
    double sum = 0.0;
    for (int i = 0; i < num_path; ++i) {
        gen->FillUniform(&dst);
        sum += dst[0];
    }
}

TEST(RandomTest, NewPseudoRandomMRG32Performance) {
    int dim = 100;
    int seed = 1024;
    std::unique_ptr<Random_> gen(New(RNGType_("MRG32"), seed, dim));

    int num_path = 2000000;
    Vector_<> dst(dim);
    double sum = 0.0;
    for (int i = 0; i < num_path; ++i) {
        gen->FillUniform(&dst);
        sum += dst[0];
    }
}