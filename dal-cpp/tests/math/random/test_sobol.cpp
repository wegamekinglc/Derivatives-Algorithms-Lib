//
// Created by wegam on 2020/12/26.
//

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/json.hpp>
#include <dal/script/simulation.hpp>

using namespace Dal;

TEST(RandomTest, TestSobolExhaustionAndInvalidSeekPreserveState) {
    const size_t maximum = std::numeric_limits<uint32_t>::max();
    for (bool shifted : {false, true})
        for (int dimension : {0, 1}) {
            auto sequence = shifted ? NewDigitallyShiftedSobol(dimension, maximum - 1, UINT64_MAX) : NewSobol(dimension, maximum - 1);
            Vector_<> last;
            sequence->FillUniform(&last);
            if (!shifted && dimension == 1)
                ASSERT_DOUBLE_EQ(last[0], std::ldexp(1., -32));
            auto exhaustedClone = sequence->Clone();
            for (auto* exhausted : {sequence.get(), dynamic_cast<SequenceSet_*>(exhaustedClone.get())}) {
                Vector_<> output{123., 456.};
                try {
                    exhausted->FillNormal(&output);
                    FAIL() << "Sobol exhaustion accepted";
                } catch (const Exception_& error) {
                    ASSERT_NE(String_(error.what()).find("RandomSequenceExhausted"), String_::npos);
                }
                ASSERT_EQ(output, (Vector_<>{123., 456.}));
                ASSERT_THROW(exhausted->FillUniform(&output), Exception_);
                ASSERT_EQ(output, (Vector_<>{123., 456.}));
                exhausted->SkipTo(maximum - 1);
                exhausted->FillUniform(&output);
                ASSERT_EQ(output, last);
                if (std::numeric_limits<size_t>::max() > maximum) {
                    exhausted->SkipTo(7);
                    auto reference = exhausted->Clone();
                    ASSERT_THROW(exhausted->SkipTo(maximum + size_t{1}), Exception_);
                    ASSERT_THROW(exhausted->SkipNormalTo(maximum + size_t{1}), Exception_);
                    Vector_<> expected;
                    reference->FillUniform(&expected);
                    exhausted->FillUniform(&output);
                    ASSERT_EQ(output, expected);
                }
            }
            ASSERT_NO_THROW(NewSobol(dimension, maximum));
        }
    if (std::numeric_limits<size_t>::max() > maximum) {
        ASSERT_THROW(NewSobol(1, maximum + size_t{1}), Exception_);
        ASSERT_THROW(NewDigitallyShiftedSobol(1, maximum + size_t{1}, 0), Exception_);
    }
}

TEST(RandomTest, TestSobolWrapperNumericBoundsAndArchiveCompatibility) {
    for (const auto& [position, dimension] : Vector_<std::pair<double, double>>{
             {.5, 1.}, {-1., 1.}, {4294967296., 1.}, {1e100, 1.},
             {std::numeric_limits<double>::quiet_NaN(), 1.}, {std::numeric_limits<double>::infinity(), 1.},
             {0., -.5}, {0., -1.}, {0., .5}, {0., 21201.}, {0., 1e100}})
        ASSERT_THROW(SobolRSG_("label", position, dimension), Exception_);
    ASSERT_THROW(NewSobol(-1, 0), Exception_);
    ASSERT_THROW(NewSobol(21201, 0), Exception_);
    ASSERT_EQ(Script::CreateRNG("sobol", 0, false), nullptr);
    ASSERT_THROW(Script::CreateRNG("sobol", std::numeric_limits<size_t>::max(), false), Exception_);
    ASSERT_EQ(NewSobol(21200, 0)->NDim(), 21200U);
    const SobolRSG_ original("label", 0., 1., true, true);
    const auto copy = handle_cast<SobolRSG_>(JSON::ReadString(JSON::WriteString(original), false));
    Vector_<> expected, actual;
    original.FillNormal(&expected);
    copy->FillNormal(&actual);
    ASSERT_EQ(actual, expected);
    ASSERT_THROW(JSON::ReadString(R"({"~type":"SobolRSG_v1","name":"label","i_path":-1,"n_dim":1,"precise":false,"polish":false})", false), Exception_);
}

TEST(RandomTest, TestNewSobol) {
    int dim = 443;
    int i_path = 0;
    std::unique_ptr<SequenceSet_> set(NewSobol(dim, i_path));
    Vector_<> dst(dim);

    int num_path = 500000;

    Vector_<> means(dim, 0.);
    Vector_<> vars(dim, 0.);

    for (int i = 0; i < num_path; ++i) {
        set->FillUniform(&dst);
        for (int j = 0; j < dim; ++j) {
            means[j] += dst[j];
            vars[j] += Square(dst[j] - 0.5);
        }
    }

    for (int i = 0; i < dim; ++i) {
        means[i] /= num_path;
        vars[i] /= num_path;
        ASSERT_NEAR(means[i], 0.5, 1e-4);
        ASSERT_NEAR(vars[i], 1. / 12, 1e-4);
    }

    means.Fill(0.);
    vars.Fill(0.);
    for (int i = 0; i < num_path; ++i) {
        set->FillNormal(&dst);
        for (int j = 0; j < dim; ++j) {
            means[j] += dst[j];
            vars[j] += Square(dst[j]);
        }
    }

    for (int i = 0; i < dim; ++i) {
        means[i] /= num_path;
        vars[i] /= num_path;
        ASSERT_NEAR(means[i], 0.0, 1e-4);
        ASSERT_NEAR(vars[i], 1.0, 1e-4);
    }
}

TEST(RandomTest, TestNewSobolWithSkip) {
    int dim = 1;
    int i_path = 0;
    int size_to_skip = pow(2, 20);
    std::unique_ptr<SequenceSet_> set(NewSobol(dim, i_path));
    std::unique_ptr<SequenceSet_> set2(NewSobol(dim, i_path));

    Vector_<> data(dim);
    Vector_<> data2(dim);

    set2->SkipTo(size_to_skip);
    for (int i = 0; i < size_to_skip; ++i)
        set->FillUniform(&data);

    set->FillUniform(&data);
    set2->FillUniform(&data2);
    ASSERT_DOUBLE_EQ(data[0], data2[0]);

    dim = 441;
    i_path = pow(2, 12);
    set = std::unique_ptr<SequenceSet_>(NewSobol(dim, i_path));
    set2 = std::unique_ptr<SequenceSet_>(NewSobol(dim, i_path));

    data.Resize(dim);
    data2.Resize(dim);

    set2->SkipTo(size_to_skip);
    for (int i = 0; i < size_to_skip - i_path; ++i)
        set->FillUniform(&data);

    set->FillUniform(&data);
    set2->FillUniform(&data2);
    for (int k = 0; k < dim; ++k)
        ASSERT_DOUBLE_EQ(data[k], data2[k]);

    set2->SkipTo(2 * size_to_skip + 1);
    for (int i = 0; i < size_to_skip; ++i)
        set->FillUniform(&data);
    set->FillUniform(&data);
    set2->FillUniform(&data2);
    for (int k = 0; k < dim; ++k)
        ASSERT_DOUBLE_EQ(data[k], data2[k]);
}

TEST(RandomTest, TestSobolNormalPrecisionPolicy) {
    constexpr size_t iPath = (1u << 20) - 2;
    std::unique_ptr<SequenceSet_> uniform(NewSobol(1, iPath, false, false));
    std::unique_ptr<SequenceSet_> defaultMode(NewSobol(1, iPath));
    std::unique_ptr<SequenceSet_> fast(NewSobol(1, iPath, false, false));
    std::unique_ptr<SequenceSet_> polishedFast(NewSobol(1, iPath, false, true));
    std::unique_ptr<SequenceSet_> preciseUnpolished(NewSobol(1, iPath, true, false));
    std::unique_ptr<SequenceSet_> precise(NewSobol(1, iPath, true, true));
    Vector_<> u;
    Vector_<> zDefault;
    Vector_<> zFast;
    Vector_<> zPolishedFast;
    Vector_<> zPreciseUnpolished;
    Vector_<> zPrecise;

    uniform->FillUniform(&u);
    defaultMode->FillNormal(&zDefault);
    fast->FillNormal(&zFast);
    polishedFast->FillNormal(&zPolishedFast);
    preciseUnpolished->FillNormal(&zPreciseUnpolished);
    precise->FillNormal(&zPrecise);

    ASSERT_DOUBLE_EQ(zDefault[0], InverseNCDF(u[0], false, false));
    ASSERT_DOUBLE_EQ(zDefault[0], zFast[0]);
    ASSERT_DOUBLE_EQ(zFast[0], InverseNCDF(u[0], false, false));
    ASSERT_DOUBLE_EQ(zPolishedFast[0], InverseNCDF(u[0], false, true));
    ASSERT_DOUBLE_EQ(zPreciseUnpolished[0], InverseNCDF(u[0], true, false));
    ASSERT_DOUBLE_EQ(zPrecise[0], InverseNCDF(u[0], true, true));
    ASSERT_NE(zPrecise[0], zFast[0]);
}

TEST(RandomTest, TestSobolClonePreservesStateAndNormalPolicy) {
    constexpr size_t iPath = (1u << 20) - 2;
    std::unique_ptr<SequenceSet_> source(NewSobol(2, iPath, true, true));
    std::unique_ptr<Random_> clone(source->Clone());
    Vector_<> sourceValues;
    Vector_<> cloneValues;

    for (int i = 0; i < 4; ++i) {
        source->FillNormal(&sourceValues);
        clone->FillNormal(&cloneValues);
        ASSERT_EQ(sourceValues.size(), cloneValues.size());
        for (int j = 0; j < static_cast<int>(sourceValues.size()); ++j)
            ASSERT_DOUBLE_EQ(sourceValues[j], cloneValues[j]);

        source->FillUniform(&sourceValues);
        clone->FillUniform(&cloneValues);
        for (int j = 0; j < static_cast<int>(sourceValues.size()); ++j)
            ASSERT_DOUBLE_EQ(sourceValues[j], cloneValues[j]);
    }
}

TEST(RandomTest, TestSobolBlockModesMatchScalarAfterSeeking) {
    for (const size_t dimension : {31, 32, 33, 366}) {
        for (const bool precise : {false, true}) {
            for (const bool polish : {false, true}) {
                auto normal = NewSobol(dimension, 0, precise, polish);
                auto uniform = NewSobol(dimension, 0);
                Vector_<> values, reference;
                for (const size_t offset : {0, 8193, 1}) {
                    normal->SkipTo(offset);
                    uniform->SkipTo(offset);
                    for (size_t path = 0; path < 4; ++path) {
                        normal->FillNormal(&values);
                        uniform->FillUniform(&reference);
                        for (size_t i = 0; i < dimension; ++i)
                            ASSERT_EQ(values[i], InverseNCDF(reference[i], precise, polish));
                    }
                    auto clone = normal->Clone();
                    normal->FillUniform(&values);
                    clone->FillUniform(&reference);
                    ASSERT_EQ(values, reference);
                }
            }
        }
    }
}

TEST(RandomTest, TestNewSobolWithLargePath) {
    const int dim = 443;
    const size_t iPath = size_t{1} << 30;
    std::unique_ptr<SequenceSet_> direct(NewSobol(dim, iPath));
    std::unique_ptr<SequenceSet_> skipped(NewSobol(dim, 0));
    skipped->SkipTo(iPath);

    Vector_<> directValues;
    Vector_<> skippedValues;
    direct->FillUniform(&directValues);
    skipped->FillUniform(&skippedValues);

    ASSERT_EQ(directValues.size(), dim);
    ASSERT_EQ(skippedValues.size(), dim);
    for (int i = 0; i < dim; ++i) {
        ASSERT_DOUBLE_EQ(directValues[i], skippedValues[i]);
        ASSERT_GE(directValues[i], 0.0);
        ASSERT_LT(directValues[i], 1.0);
    }
}

TEST(RandomTest, TestNewSobolPerformance) {
    int dim = 100;
    int i_path = 0;
    std::unique_ptr<SequenceSet_> set(NewSobol(dim, i_path));

    int num_path = 2000000;
    Vector_<> dst(dim);
    double sum = 0.0;
    for (int i = 0; i < num_path; ++i) {
        set->FillUniform(&dst);
        sum += dst[0];
    }
    ASSERT_NEAR(sum / num_path, 0.5, 1e-4);
}

TEST(RandomTest, TestDigitallyShiftedSobolReproducibleAndSeparated) {
    constexpr uint64_t KEY = 0x123456789abcdef0ULL;
    auto source = NewDigitallyShiftedSobol(3, 2048, KEY);
    auto matching = NewDigitallyShiftedSobol(3, 0, KEY);
    matching->SkipTo(2048);
    auto different = NewDigitallyShiftedSobol(3, 2048, KEY + 1);
    Vector_<> first, same, other;
    source->FillUniform(&first);
    matching->FillUniform(&same);
    different->FillUniform(&other);
    ASSERT_EQ(first.size(), 3u);
    ASSERT_EQ(first, same);
    ASSERT_NE(first, other);
    auto pricingRole = NewDigitallyShiftedSobol(3, 2048, (uint64_t{1} << 63) | (uint64_t{17} << 32));
    auto trainingRole = NewDigitallyShiftedSobol(3, 2048, uint64_t{17} << 32);
    pricingRole->FillUniform(&first);
    trainingRole->FillUniform(&same);
    ASSERT_NE(first, same);
    for (double value : first) {
        ASSERT_GT(value, 0.0);
        ASSERT_LT(value, 1.0);
    }

    auto cloned = source->Clone();
    source->FillNormal(&first);
    cloned->FillNormal(&same);
    ASSERT_EQ(first, same);
    for (double value : first)
        ASSERT_TRUE(std::isfinite(value));
}

TEST(RandomTest, TestDigitallyShiftedSobolTakeAwayPreservesCoordinates) {
    constexpr uint64_t KEY = 0x123456789abcdef0ULL;
    auto complete = NewDigitallyShiftedSobol(3, 17, KEY);
    auto prefix = NewDigitallyShiftedSobol(3, 17, KEY);
    auto suffix = prefix->TakeAway(1);
    Vector_<> all, left, right;
    for (int i = 0; i < 16; ++i) {
        complete->FillUniform(&all);
        prefix->FillUniform(&left);
        suffix->FillUniform(&right);
        ASSERT_EQ(left.size(), 2u);
        ASSERT_EQ(right.size(), 1u);
        ASSERT_DOUBLE_EQ(all[0], left[0]);
        ASSERT_DOUBLE_EQ(all[1], left[1]);
        ASSERT_DOUBLE_EQ(all[2], right[0]);
    }
}
