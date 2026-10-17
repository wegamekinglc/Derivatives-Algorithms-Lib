//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

#include <dal/math/matrix/physicalrowerrorinternal.hpp>
#include <dal/math/pde/sampledthetastepinternal.hpp>
#include <dal/platform/platform.hpp>

using Dal::Matrix_;
using Dal::Vector_;
using Dal::PDE::SampledThetaDetail::TridiagonalFactors_;

namespace {
    struct FactorReference_ {
        const char* name_;
        bool transpose_;
        std::initializer_list<double> lower_, diagonal_, upper_, rhs_, expected_;
    };

    const FactorReference_ FACTOR_REFERENCES[] = {
        {"scalar/forward/0", false, {}, {2}, {}, {1}, {0.5}},
        {"scalar/forward/1", false, {}, {2}, {}, {1}, {0.5}},
        {"scalar/transpose/0", true, {}, {2}, {}, {1}, {0.5}},
        {"scalar/transpose/1", true, {}, {2}, {}, {1}, {0.5}},
        {"last-pivot-n2/forward/0", false, {2}, {0, 3}, {1}, {1, 1.125}, {-0.9375, 1}},
        {"last-pivot-n2/forward/1", false, {2}, {0, 3}, {1}, {1, -2}, {-2.5, 1}},
        {"last-pivot-n2/transpose/0", true, {2}, {0, 3}, {1}, {1, 1.125}, {-0.375, 0.5}},
        {"last-pivot-n2/transpose/1", true, {2}, {0, 3}, {1}, {1, -2}, {-3.5, 0.5}},
        {"no-pivot-n3/forward/0",
         false,
         {1, -2},
         {3, 4, 5},
         {-1, 2},
         {1, 1.125, 1.25},
         {0.35227272727272729, 0.056818181818181816, 0.27272727272727271}},
        {"no-pivot-n3/forward/1", false, {1, -2}, {3, 4, 5}, {-1, 2}, {1, -2, 3}, {0.1038961038961039, -0.68831168831168832, 0.32467532467532467}},
        {"no-pivot-n3/transpose/0",
         true,
         {1, -2},
         {3, 4, 5},
         {-1, 2},
         {1, 1.125, 1.25},
         {0.20616883116883117, 0.3814935064935065, 0.097402597402597407}},
        {"no-pivot-n3/transpose/1", true, {1, -2}, {3, 4, 5}, {-1, 2}, {1, -2, 3}, {0.36363636363636365, -0.090909090909090912, 0.63636363636363635}},
        {"second-upper-fill-n3/forward/0", false, {2, 5}, {0, 3, 6}, {1, 4}, {1, 1.125, 1.25}, {0.3125, 1, -0.625}},
        {"second-upper-fill-n3/forward/1", false, {2, 5}, {0, 3, 6}, {1, 4}, {1, -2, 3}, {-1.8333333333333333, 1, -0.33333333333333331}},
        {"second-upper-fill-n3/transpose/0", true, {2, 5}, {0, 3, 6}, {1, 4}, {1, 1.125, 1.25}, {0.25, 0.5, -0.125}},
        {"second-upper-fill-n3/transpose/1", true, {2, 5}, {0, 3, 6}, {1, 4}, {1, -2, 3}, {-4.333333333333333, 0.5, 0.16666666666666666}},
        {"interior-pivot-n4/forward/0",
         false,
         {1, 3, 1},
         {4, 0.01, 2, 3},
         {2, 1, -1},
         {1, 1.125, 1.25, 1.375},
         {0.29022526146419952, -0.080450522928399035, 0.83557924376508452, 0.17980691874497184}},
        {"interior-pivot-n4/forward/1",
         false,
         {1, 3, 1},
         {4, 0.01, 2, 3},
         {2, 1, -1},
         {1, -2, 3, -4},
         {-0.58467417538213995, 1.6693483507642799, -1.4320193081255028, -0.85599356395816573}},
        {"interior-pivot-n4/transpose/0",
         true,
         {1, 3, 1},
         {4, 0.01, 2, 3},
         {2, 1, -1},
         {1, 1.125, 1.25, 1.375},
         {0.19469026548672566, 0.22123893805309736, 0.24446902654867256, 0.53982300884955747}},
        {"interior-pivot-n4/transpose/1",
         true,
         {1, 3, 1},
         {4, 0.01, 2, 3},
         {2, 1, -1},
         {1, -2, 3, -4},
         {-0.88636363636363635, 4.5454545454545459, -0.090909090909090912, -1.3636363636363635}},
        {"consecutive-pivots-n5/forward/0",
         false,
         {2, 3, 4, 5},
         {0, 0, 0, 0.10000000000000001, 3},
         {1, 2, 2, 1},
         {1, 1.125, 1.25, 1.375, 1.5},
         {0.68645833333333328, 1, -0.12395833333333334, -0.875, 1.9583333333333333}},
        {"consecutive-pivots-n5/forward/1",
         false,
         {2, 3, 4, 5},
         {0, 0, 0, 0.10000000000000001, 3},
         {1, 2, 2, 1},
         {1, -2, 3, -4, 5},
         {0.41666666666666669, 1, -1.4166666666666667, 0, 1.6666666666666667}},
        {"consecutive-pivots-n5/transpose/0",
         true,
         {2, 3, 4, 5},
         {0, 0, 0, 0.10000000000000001, 3},
         {1, 2, 2, 1},
         {1, 1.125, 1.25, 1.375, 1.5},
         {2.6656249999999999, 0.5, -0.51354166666666667, 0.0625, 0.47916666666666669}},
        {"consecutive-pivots-n5/transpose/1",
         true,
         {2, 3, 4, 5},
         {0, 0, 0, 0.10000000000000001, 3},
         {1, 2, 2, 1},
         {1, -2, 3, -4, 5},
         {15.324999999999999, 0.5, -5.7750000000000004, 0.5, 1.5}},
        {"identity-n5/forward/0", false, {0, 0, 0, 0}, {1, 1, 1, 1, 1}, {0, 0, 0, 0}, {1, 1.125, 1.25, 1.375, 1.5}, {1, 1.125, 1.25, 1.375, 1.5}},
        {"identity-n5/forward/1", false, {0, 0, 0, 0}, {1, 1, 1, 1, 1}, {0, 0, 0, 0}, {1, -2, 3, -4, 5}, {1, -2, 3, -4, 5}},
        {"identity-n5/transpose/0", true, {0, 0, 0, 0}, {1, 1, 1, 1, 1}, {0, 0, 0, 0}, {1, 1.125, 1.25, 1.375, 1.5}, {1, 1.125, 1.25, 1.375, 1.5}},
        {"identity-n5/transpose/1", true, {0, 0, 0, 0}, {1, 1, 1, 1, 1}, {0, 0, 0, 0}, {1, -2, 3, -4, 5}, {1, -2, 3, -4, 5}},
    };

    struct ResidualReference_ {
        const char* name_;
        double rhs_;
        std::initializer_list<std::pair<double, double>> products_;
        double expected_;
    };

    const ResidualReference_ RESIDUAL_REFERENCES[] = {
        {"zero-row", 0, {{0, 2}, {1, 0}, {0, 0}}, 0},
        {"fma-roundoff", 1, {{1.0000000074505806, 0.9999999925494194}}, 2.7755575615628914e-17},
        {"tiny-ratio-with-cancellation",
         0,
         {{3.2733906078961419e+150, 3.2733906078961419e+150}, {-3.2733906078961419e+150, 3.2733906078961419e+150}, {1.862645149230957e-09, 1}},
         8.6916947597937554e-311},
        {"overflowing-products-with-finite-ratio",
         1.0715086071862673e+301,
         {{8.9884656743115795e+307, 1024}, {-8.9884656743115795e+307, 1024}},
         5.8207660910079275e-11},
        {"underflowing-physical-product", 0, {{4.9406564584124654e-324, 0.5}}, 1},
        {"asymmetric-transpose-left-boundary", 3, {{1, 2}, {-0.125, 8}}, 0.33333333333333331},
        {"asymmetric-transpose-right-boundary", -2, {{0.75, 4}, {1, -1}}, 0.66666666666666663},
    };
} // namespace

TEST(SampledThetaStepTest, TestIndependentTridiagonalForwardAndTranspose) {
    for (const auto& reference : FACTOR_REFERENCES) {
        SCOPED_TRACE(reference.name_);
        const TridiagonalFactors_ factor(Vector_<>(reference.lower_), Vector_<>(reference.diagonal_), Vector_<>(reference.upper_),
                                         64.0 * std::numeric_limits<double>::epsilon());
        Matrix_<> rhs(static_cast<int>(reference.rhs_.size()), 1);
        auto input = reference.rhs_.begin();
        for (int row = 0; row < rhs.Rows(); ++row)
            rhs(row, 0) = *input++;
        const auto solution = factor.Solve(rhs, reference.transpose_);
        auto expected = reference.expected_.begin();
        ASSERT_EQ(solution.Rows(), rhs.Rows());
        ASSERT_EQ(solution.Cols(), 1);
        for (int row = 0; row < solution.Rows(); ++row) {
            ASSERT_NEAR(solution(row, 0), *expected, 1e-10 * std::max(1.0, std::abs(*expected)));
            ++expected;
        }
    }
}

TEST(SampledThetaStepTest, TestIndependentPhysicalResidualExtremeReferences) {
    for (const auto& reference : RESIDUAL_REFERENCES) {
        SCOPED_TRACE(reference.name_);
        const auto products = reference.products_.begin();
        const double actual =
            Dal::LinearSolveDetail::PhysicalRowBackwardError(reference.rhs_, static_cast<int>(reference.products_.size()), [products](int entry) {
                return Dal::LinearSolveDetail::ScaledProduct_(products[entry].first, products[entry].second);
            });
        ASSERT_DOUBLE_EQ(actual, reference.expected_);
    }
}
