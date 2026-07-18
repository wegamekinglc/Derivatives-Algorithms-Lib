//
// Created by wegam on 2020/12/26.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

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
