//
// Created by wegamekinglc on 2020/12/19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

namespace {
    struct RandomStreamMode_ {
        const char* name_;
        void (Random_::*fill_)(Vector_<>*);
        void (Random_::*seek_)(size_t);
    };
} // namespace

TEST(PseudoRandomTest, TestSeekingMatchesReplayForEveryOffsetAndMode) {
    const RandomStreamMode_ modes[] = {{"uniform", &Random_::FillUniform, &Random_::SkipTo},
                                       {"normal", &Random_::FillNormal, &Random_::SkipNormalTo}};
    for (const auto* name : {"IRN", "MRG32"}) {
        for (size_t dimension : {1, 3, 10}) {
            for (const auto& mode : modes) {
                auto sought = New(RNGType_(name), 1024, dimension, false);
                Vector_<> actual(dimension), expected(dimension);
                for (size_t offset : {0, 1, 2, 17, 32, 3, 0}) {
                    auto replay = New(RNGType_(name), 1024, dimension, false);
                    for (size_t path = 0; path < offset; ++path)
                        (replay.get()->*mode.fill_)(&expected);
                    (sought.get()->*mode.seek_)(offset);
                    for (int path = 0; path < 4; ++path) {
                        (replay.get()->*mode.fill_)(&expected);
                        (sought.get()->*mode.fill_)(&actual);
                        ASSERT_EQ(actual, expected) << name << "; mode=" << mode.name_ << "; offset=" << offset;
                    }
                }
            }
        }
    }
}

TEST(PseudoRandomTest, TestClonePreservesStateAndPrecision) {
    for (const auto* name : {"IRN", "MRG32"}) {
        for (const bool precise : {false, true}) {
            auto generator = New(RNGType_(name), 1024, 3, precise);
            Vector_<> actual(3), expected(3);
            generator->FillUniform(&actual);
            generator->FillNormal(&actual);
            auto clone = generator->Clone();
            for (int path = 0; path < 4; ++path) {
                generator->FillUniform(&expected);
                clone->FillUniform(&actual);
                ASSERT_EQ(actual, expected);
                generator->FillNormal(&expected);
                clone->FillNormal(&actual);
                ASSERT_EQ(actual, expected);
            }
        }
    }
}

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

    gen = New(RNGType_("MRG32"), 1024, n_dim);
    gen2 = gen->Clone();
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

TEST(RandomTest, TestNewPseudoRandomIRNPerformance) {
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

TEST(RandomTest, TestNewPseudoRandomMRG32Performance) {
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
