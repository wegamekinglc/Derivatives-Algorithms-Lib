//
// Created by Codex on 2026/10/08.
//

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <limits>

#include <dal/math/aad/structuraljacobian.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::Matrix_;
using Dal::Vector_;
using Dal::AAD::PlanStructuralJacobian;
using Dal::AAD::RecoverStructuralJacobian;

namespace {
    void CheckMatrix(const Matrix_<>& actual, const Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int column = 0; column < actual.Cols(); ++column)
                ASSERT_DOUBLE_EQ(actual(row, column), expected(row, column));
    }

    void CheckColorGroups(const Dal::AAD::StructuralJacobianPlan_& plan) {
        Vector_<int> seenRows(plan.Outputs(), 0);
        for (size_t color = 0; color < plan.ColorCount(); ++color) {
            Vector_<int> seenColumns(plan.Inputs(), 0);
            for (const auto row : plan.ColorRows(color)) {
                ASSERT_EQ(plan.RowColor(row), color);
                ASSERT_EQ(++seenRows[row], 1);
                for (const auto column : plan.RowSupport(row))
                    ASSERT_EQ(++seenColumns[column], 1);
            }
        }
        for (size_t row = 0; row < plan.Outputs(); ++row)
            ASSERT_EQ(seenRows[row], plan.RowColor(row) ? 1 : 0);
    }

    struct LinearFixture_ {
        Vector_<Vector_<size_t>> supports_{3};
        Matrix_<> dense_{3, 3, 0.0};
    };

    LinearFixture_ MakeLinearFixture(unsigned int terms) {
        LinearFixture_ result;
        for (size_t row = 0; row < 3; ++row)
            for (size_t column = 0; column < 3; ++column)
                if (terms & (1U << (3 * row + column))) {
                    result.supports_[row].push_back(column);
                    result.dense_(static_cast<int>(row), static_cast<int>(column)) = static_cast<double>(3 * row + column + 1);
                }
        return result;
    }

    Matrix_<> DirectSeedSum(const Dal::AAD::StructuralJacobianPlan_& plan, const Matrix_<>& dense) {
        Matrix_<> directions(static_cast<int>(plan.ColorCount()), dense.Cols(), 0.0);
        for (size_t color = 0; color < plan.ColorCount(); ++color)
            for (const auto row : plan.ColorRows(color))
                for (int column = 0; column < dense.Cols(); ++column)
                    directions(static_cast<int>(color), column) += dense(static_cast<int>(row), column);
        return directions;
    }
} // namespace

TEST(StructuralJacobianTest, TestTwoDirectionsRecoverIndependentNonPrefixMatrix) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    ASSERT_EQ(plan.Inputs(), 4U);
    ASSERT_EQ(plan.Outputs(), 3U);
    ASSERT_EQ(plan.ColorCount(), 2U);
    ASSERT_EQ(plan.RowColor(0), 0U);
    ASSERT_EQ(plan.RowColor(1), 0U);
    ASSERT_EQ(plan.RowColor(2), 1U);
    ASSERT_EQ(plan.ResultBytes(), 96U);
    ASSERT_EQ(plan.DirectionBytes(), 64U);
    ASSERT_EQ(plan.NumericPayloadBytes(), 160U);

    Matrix_<> directions(2, 4, 0.0);
    directions(0, 0) = 2.0;
    directions(0, 1) = 3.0;
    directions(0, 2) = 4.0;
    directions(1, 1) = 5.0;
    directions(1, 3) = 6.0;
    const Vector_<Vector_<double>> expected{{2.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 4.0, 0.0}, {0.0, 5.0, 0.0, 6.0}};
    const auto actual = RecoverStructuralJacobian(plan, directions);
    ASSERT_EQ(actual.Rows(), 3);
    ASSERT_EQ(actual.Cols(), 4);
    for (int row = 0; row < actual.Rows(); ++row)
        for (int column = 0; column < actual.Cols(); ++column)
            ASSERT_DOUBLE_EQ(actual(row, column), expected[row][column]);
}

TEST(StructuralJacobianTest, TestAllSmallSupportPatternsAgainstDirectDenseSeedSums) {
    for (unsigned int terms = 0; terms < 512; ++terms) {
        SCOPED_TRACE(terms);
        const auto fixture = MakeLinearFixture(terms);
        const auto plan = PlanStructuralJacobian(3, fixture.supports_);
        ASSERT_NO_FATAL_FAILURE(CheckColorGroups(plan));
        const auto directions = DirectSeedSum(plan, fixture.dense_);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(RecoverStructuralJacobian(plan, directions), fixture.dense_));
    }
}

TEST(StructuralJacobianTest, TestConnectedZeroDerivativeRemainsSupportedAtAnotherPoint) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {}, {2, 3}});
    ASSERT_EQ(plan.ColorCount(), 1U);
    ASSERT_FALSE(plan.RowColor(1));
    ASSERT_EQ(plan.RowSupport(0), (Vector_<size_t>{0, 1}));
    for (const auto& point : std::array<std::array<double, 2>, 2>{{{0.0, 0.0}, {2.0, 3.0}}}) {
        Matrix_<> expected(3, 4, 0.0);
        expected(0, 0) = point[1];
        expected(0, 1) = point[0];
        expected(2, 2) = 2.0;
        expected(2, 3) = 3.0;
        const auto directions = DirectSeedSum(plan, expected);
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(RecoverStructuralJacobian(plan, directions), expected));
    }
}

TEST(StructuralJacobianTest, TestEmptyAxesRowsAndWideUnusedAxisUseZeroPayload) {
    const std::array<size_t, 3> inputs{0, 4, static_cast<size_t>(std::numeric_limits<int>::max())};
    const std::array<size_t, 3> rows{3, 0, 0};
    for (size_t index = 0; index < inputs.size(); ++index) {
        const auto plan = PlanStructuralJacobian(inputs[index], Vector_<Vector_<size_t>>(rows[index]), {0});
        ASSERT_EQ(plan.Inputs(), inputs[index]);
        ASSERT_EQ(plan.Outputs(), rows[index]);
        ASSERT_EQ(plan.ColorCount(), 0U);
        ASSERT_EQ(plan.NumericPayloadBytes(), 0U);
        const auto result = RecoverStructuralJacobian(plan, Matrix_<>(0, static_cast<int>(inputs[index])));
        ASSERT_EQ(result.Rows(), static_cast<int>(rows[index]));
        ASSERT_EQ(result.Cols(), static_cast<int>(inputs[index]));
        for (size_t row = 0; row < rows[index]; ++row)
            ASSERT_FALSE(plan.RowColor(row));
    }
    const auto emptyRows = PlanStructuralJacobian(4, {{}, {}});
    const auto zeros = RecoverStructuralJacobian(emptyRows, Matrix_<>(0, 4));
    ASSERT_EQ(emptyRows.ColorCount(), 0U);
    ASSERT_EQ(emptyRows.ResultBytes(), 64U);
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(zeros, Matrix_<>(2, 4, 0.0)));
}

TEST(StructuralJacobianTest, TestCanonicalSupportsCopiesAndResultsOwnTheirSnapshots) {
    Matrix_<> detached;
    {
        Vector_<Vector_<size_t>> source{{2, 0, 2}, {}, {1, 1}, {}};
        auto plan = PlanStructuralJacobian(3, source);
        ASSERT_EQ(plan.RowSupport(0), (Vector_<size_t>{0, 2}));
        ASSERT_EQ(plan.RowSupport(2), (Vector_<size_t>{1}));
        ASSERT_EQ(plan.ColorRows(0), (Vector_<size_t>{0, 2}));
        const auto copied = plan;
        source.clear();
        plan = PlanStructuralJacobian(0, {});
        Matrix_<> directions(1, 3, 0.0);
        directions(0, 0) = 7.0;
        directions(0, 1) = 11.0;
        directions(0, 2) = 13.0;
        detached = RecoverStructuralJacobian(copied, directions);
        for (int column = 0; column < directions.Cols(); ++column)
            directions(0, column) = 99.0;
        ASSERT_EQ(copied.RowSupport(0), (Vector_<size_t>{0, 2}));
    }
    Matrix_<> expected(4, 3, 0.0);
    expected(0, 0) = 7.0;
    expected(0, 2) = 13.0;
    expected(2, 1) = 11.0;
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(detached, expected));
}

TEST(StructuralJacobianTest, TestExactCombinedBudgetAndWideAdmissionBeforeAllocation) {
    const Vector_<Vector_<size_t>> supports{{0, 1}, {2}, {1, 3}};
    ASSERT_EQ(PlanStructuralJacobian(4, supports, {160}).NumericPayloadBytes(), 160U);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(4, supports, {159})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(4, supports, {95})), Dal::Exception_);
    const Vector_<Vector_<size_t>> denseSupports{{0, 1, 2}, {0, 1, 2}, {0, 1, 2}};
    const auto dense = PlanStructuralJacobian(3, denseSupports, {144});
    ASSERT_EQ(dense.ColorCount(), 3U);
    ASSERT_EQ(dense.ResultBytes(), 72U);
    ASSERT_EQ(dense.DirectionBytes(), 72U);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(3, denseSupports, {143})), Dal::Exception_);
    const auto maximum = static_cast<size_t>(std::numeric_limits<int>::max());
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(maximum, {{0}, {1}}, {512})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(maximum + 1, {})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(std::numeric_limits<size_t>::max(), {{0}})), Dal::Exception_);
    ASSERT_EQ(PlanStructuralJacobian(4, supports, {160}).ColorCount(), 2U);
}

TEST(StructuralJacobianTest, TestInvalidSupportsAndGetterRangesLeavePlanUsable) {
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(0, {{0}})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(3, {{3}})), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(PlanStructuralJacobian(3, {{std::numeric_limits<size_t>::max()}})), Dal::Exception_);
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    ASSERT_THROW(static_cast<void>(plan.RowSupport(3)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(plan.RowColor(3)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(plan.ColorRows(2)), Dal::Exception_);
    const auto empty = PlanStructuralJacobian(0, {});
    ASSERT_THROW(static_cast<void>(empty.RowSupport(0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(empty.RowColor(0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(empty.ColorRows(0)), Dal::Exception_);
    ASSERT_EQ(plan.RowSupport(2), (Vector_<size_t>{1, 3}));
    ASSERT_EQ(plan.ColorRows(0), (Vector_<size_t>{0, 1}));
}

TEST(StructuralJacobianTest, TestWrongShapesAndNonFiniteUnusedGradientsRejectBeforeRecovery) {
    const auto plan = PlanStructuralJacobian(2, {{0}});
    ASSERT_THROW(static_cast<void>(RecoverStructuralJacobian(plan, Matrix_<>(0, 2))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(RecoverStructuralJacobian(plan, Matrix_<>(2, 2))), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(RecoverStructuralJacobian(plan, Matrix_<>(1, 1))), Dal::Exception_);
    for (const auto bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        Matrix_<> directions(1, 2, 3.0);
        directions(0, 1) = bad;
        ASSERT_THROW(static_cast<void>(RecoverStructuralJacobian(plan, directions)), Dal::Exception_);
        directions(0, 1) = 0.0;
        directions(0, 0) = bad;
        ASSERT_THROW(static_cast<void>(RecoverStructuralJacobian(plan, directions)), Dal::Exception_);
    }
    Matrix_<> valid(1, 2, 3.0);
    const auto result = RecoverStructuralJacobian(plan, valid);
    ASSERT_DOUBLE_EQ(result(0, 0), 3.0);
    ASSERT_DOUBLE_EQ(result(0, 1), 0.0);
    ASSERT_DOUBLE_EQ(valid(0, 1), 3.0);
}

TEST(StructuralJacobianTest, TestConservativeSupersetRetainsZerosWithAdditionalDirections) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {1, 2}, {1, 3}});
    ASSERT_EQ(plan.ColorCount(), 3U);
    Matrix_<> expected(3, 4, 0.0);
    expected(0, 0) = 2.0;
    expected(0, 1) = 3.0;
    expected(1, 2) = 4.0;
    expected(2, 1) = 5.0;
    expected(2, 3) = 6.0;
    ASSERT_NO_FATAL_FAILURE(CheckColorGroups(plan));
    ASSERT_NO_FATAL_FAILURE(CheckMatrix(RecoverStructuralJacobian(plan, DirectSeedSum(plan, expected)), expected));
}

TEST(StructuralJacobianTest, TestConcurrentConstRecoveryReturnsIndependentOwningResults) {
    const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
    Matrix_<> reference(3, 4, 0.0);
    reference(0, 0) = 2.0;
    reference(0, 1) = 3.0;
    reference(1, 2) = 4.0;
    reference(2, 1) = 5.0;
    reference(2, 3) = 6.0;
    const std::array<double, 4> factors{0.0, 1.0, -2.0, 4.0};
    std::array<std::future<Matrix_<>>, 4> futures;
    for (size_t index = 0; index < factors.size(); ++index)
        futures[index] = std::async(std::launch::async, [&, factor = factors[index]] {
            auto directions = DirectSeedSum(plan, reference);
            for (int row = 0; row < directions.Rows(); ++row)
                for (int column = 0; column < directions.Cols(); ++column)
                    directions(row, column) *= factor;
            return RecoverStructuralJacobian(plan, directions);
        });
    for (size_t index = 0; index < factors.size(); ++index) {
        Matrix_<> expected = reference;
        for (int row = 0; row < expected.Rows(); ++row)
            for (int column = 0; column < expected.Cols(); ++column)
                expected(row, column) *= factors[index];
        ASSERT_NO_FATAL_FAILURE(CheckMatrix(futures[index].get(), expected));
    }
}
