//
// Created by wegam on 2020/12/26.
//

#include <dal/math/operators.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <gtest/gtest.h>
#include <memory>

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

TEST(RandomTest, TestNewSobolWithLargePath) {
    // TODO: this test currently does not work
    int dim = 443;
    size_t i_path = std::pow(2, 30);
    std::unique_ptr<SequenceSet_> set(NewSobol(dim, i_path));
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
}