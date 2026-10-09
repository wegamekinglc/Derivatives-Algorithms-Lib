//
// Created by wegamekinglc on 2020/12/19.
//

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <dal/math/operators.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>

using namespace Dal;

namespace {
    // Pins the pre-optimization floating recurrence independently of the production engine.
    struct LegacyMRG_ {
        double x_, x1_, x2_, y_, y1_, y2_;

        explicit LegacyMRG_(int seed) : x_(static_cast<unsigned>(seed)), x1_(x_), x2_(x_), y_(static_cast<unsigned>(seed) + 1U), y1_(y_), y2_(y_) {}

        double Next() {
            double x = 1403580.0 * x1_ - 810728.0 * x2_;
            x -= static_cast<int64_t>(x / 4294967087.0) * 4294967087.0;
            if (x < 0)
                x += 4294967087.0;
            x2_ = x1_;
            x1_ = x_;
            x_ = x;
            double y = 527612.0 * y_ - 1370589.0 * y2_;
            y -= static_cast<int64_t>(y / 4294944443.0) * 4294944443.0;
            if (y < 0)
                y += 4294944443.0;
            y2_ = y1_;
            y1_ = y_;
            y_ = y;
            return (x > y ? x - y : x - y + 4294967087.0) / 4294967088.0;
        }
    };

    struct RandomStreamMode_ {
        const char* name_;
        void (Random_::*fill_)(Vector_<>*);
        void (Random_::*seek_)(size_t);
    };
} // namespace

TEST(PseudoRandomTest, TestMRGUniformStreamMatchesLegacyAcrossSeeds) {
    for (const int seed : {0, 1, 1024, 12345, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        auto generator = New(RNGType_("MRG32"), seed, 1, false);
        LegacyMRG_ reference(seed);
        for (size_t draw = 0; draw < 2000000; ++draw)
            ASSERT_EQ(generator->NextUniform(), reference.Next()) << "seed=" << seed << "; draw=" << draw;
    }
}

TEST(PseudoRandomTest, TestMRGBulkUniformAndMixedCloneMatchScalarStream) {
    for (const int seed : {0, 1024, -1}) {
        auto generator = New(RNGType_("MRG32"), seed, 52, false);
        LegacyMRG_ reference(seed);
        Vector_<> actual(52), expected(52);
        for (int path = 0; path < 31; ++path) {
            for (auto& value : expected)
                value = reference.Next();
            generator->FillUniform(&actual);
            ASSERT_EQ(actual, expected);
            auto clone = generator->Clone();
            generator->FillUniform(&actual);
            for (size_t i = 0; i < actual.size(); ++i)
                ASSERT_EQ(actual[i], 1.0 - expected[i]);
            clone->FillUniform(&expected);
            ASSERT_EQ(actual, expected);
            ASSERT_EQ(generator->NextUniform(), reference.Next());
        }
    }
}

TEST(PseudoRandomTest, TestMRGNormalSeekingMatchesLegacyAtBatchBoundaries) {
    for (const int seed : {0, 1024, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (const bool precise : {false, true}) {
            auto generator = New(RNGType_("MRG32"), seed, 52, precise);
            Vector_<> actual(52);
            for (const size_t offset : {8193, 1025, 0, 1}) {
                LegacyMRG_ reference(seed);
                for (size_t draw = 0; draw < offset * actual.size(); ++draw)
                    reference.Next();
                generator->SkipNormalTo(offset);
                auto clone = generator->Clone();
                Vector_<> cloned(52);
                generator->FillNormal(&actual);
                clone->FillNormal(&cloned);
                ASSERT_EQ(actual, cloned);
                for (const auto value : actual)
                    ASSERT_EQ(value, InverseNCDF(reference.Next(), precise, precise)) << "seed=" << seed << "; offset=" << offset;
            }
        }
    }
}

TEST(PseudoRandomTest, TestMRGUniformSeekingMatchesLegacyAndAntitheticCache) {
    for (const int seed : {0, 1024, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        auto generator = New(RNGType_("MRG32"), seed, 52, false);
        Vector_<> actual(52);
        for (const size_t offset : {8192, 8193, 3, 0, 1}) {
            LegacyMRG_ reference(seed);
            for (size_t draw = 0; draw < (offset / 2) * actual.size(); ++draw)
                reference.Next();
            generator->SkipTo(offset);
            generator->FillUniform(&actual);
            for (const auto value : actual) {
                const double uniform = reference.Next();
                ASSERT_EQ(value, offset % 2 ? 1.0 - uniform : uniform) << "seed=" << seed << "; offset=" << offset;
            }
            generator->FillNormal(&actual);
            for (const auto value : actual)
                ASSERT_EQ(value, InverseNCDF(reference.Next(), false, false));
        }
    }
}

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
