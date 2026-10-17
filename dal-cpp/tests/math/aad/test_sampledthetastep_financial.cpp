//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>

#include "financialthetastepfixture.hpp"
#include "test-support/europeanaadfd.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    namespace Financial = DalTest::FinancialPDE;

    void CheckFinancialReports(const Financial::Result_& result, size_t steps, int channels) {
        ASSERT_LE(result.forwardError_, 1e-12);
        ASSERT_EQ(result.events_.size(), steps);
        for (const auto& sweep : result.sweeps_) {
            ASSERT_EQ(sweep.reports_.Entries().size(), steps);
            for (const auto& event : result.events_) {
                const auto& errors = sweep.reports_.Report(event).transposeBackwardErrors_;
                ASSERT_EQ(errors.Rows(), 2);
                ASSERT_EQ(errors.Cols(), channels);
                for (double error : errors)
                    ASSERT_LE(error, 1e-12);
            }
        }
    }

    void CheckCenteredFinancialDifferences(const Financial::Result_& result, size_t coordinate) {
        const std::array<std::array<double, 3>, 3> bumps = {{{0.001, 0.0005, 0.00025}, {0.001, 0.0005, 0.00025}, {0.1, 0.05, 0.025}}};
        const std::array<double, 3> errorFactors = {100.0, 500.0, 0.0};
        const std::array<double, 3> direct = {2.0, -3.0, 0.01};
        for (double bump : bumps[coordinate]) {
            SCOPED_TRACE(bump);
            auto plus = Financial::PARAMETERS, minus = Financial::PARAMETERS;
            plus[coordinate] += bump;
            minus[coordinate] -= bump;
            const auto high = Financial::DenseSmallPrice(plus), low = Financial::DenseSmallPrice(minus);
            std::array<double, 2> differences;
            const double ceiling = errorFactors[coordinate] * bump * bump + 1e-7;
            for (size_t layer = 0; layer < 2; ++layer) {
                differences[layer] = (high[layer] - low[layer]) / (2.0 * bump);
                ASSERT_NEAR(result.sweeps_[0].risks_(static_cast<int>(layer), static_cast<int>(coordinate)), differences[layer], ceiling);
            }
            const double weighted = 1.25 * differences[0] - 0.75 * differences[1] + direct[coordinate];
            ASSERT_NEAR(result.sweeps_[0].risks_(2, static_cast<int>(coordinate)), weighted, 2.0 * ceiling);
        }
    }

    void CheckFrozenFinancialReference(const Financial::Result_& result, const Financial::Quote_& reference) {
        for (int layer = 0; layer < 2; ++layer) {
            ASSERT_NEAR(result.prices_[layer], reference.prices_[layer], 1e-9);
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                ASSERT_NEAR(result.sweeps_[0].risks_(layer, coordinate), reference.risks_[layer][coordinate], 1e-8);
        }
    }

    Matrix_<> FinancialErrors(const Financial::Result_& result, const Financial::Quote_& continuum) {
        Matrix_<> errors(2, 4);
        for (int layer = 0; layer < 2; ++layer) {
            errors(layer, 0) = std::abs(result.prices_[layer] - continuum.prices_[layer]);
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                errors(layer, coordinate + 1) = std::abs(result.sweeps_[0].risks_(layer, coordinate) - continuum.risks_[layer][coordinate]);
        }
        return errors;
    }

    void CheckFinancialConvergence(const Matrix_<>& errors, const Matrix_<>& previous, double spacing) {
        const std::array<double, 4> coefficients = {0.0002, 0.002, 0.006, 0.00012};
        for (int layer = 0; layer < 2; ++layer)
            for (int quantity = 0; quantity < 4; ++quantity) {
                ASSERT_LE(errors(layer, quantity), coefficients[quantity] * spacing * spacing);
                ASSERT_LE(errors(layer, quantity), 0.25 * previous(layer, quantity));
            }
    }

    void CheckFinancialParity(const Financial::Result_& result, int steps) {
        const double dt = 1.0 / steps, strikeDiscount = 110.0 * std::exp(-0.05);
        const std::array<double, 4> expected = {100.0 * std::exp(-0.02) - strikeDiscount, strikeDiscount, 0.0, -std::exp(-0.05)};
        const std::array<double, 4> ceilings = {0.2 * dt * dt, 6.0 * dt * dt, 1e-8, 0.002 * dt * dt};
        const auto& risks = result.sweeps_[0].risks_;
        const std::array<double, 4> actual = {result.prices_[0] - result.prices_[1], risks(0, 0) - risks(1, 0), risks(0, 1) - risks(1, 1),
                                              risks(0, 2) - risks(1, 2)};
        for (size_t quantity = 0; quantity < actual.size(); ++quantity)
            ASSERT_NEAR(actual[quantity], expected[quantity], ceilings[quantity]);
    }
} // namespace

TEST(AADSampledThetaStepTest, TestFinancialSmallMeshFrozenPriceAndGreeks) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ scope;
    std::array<Number_, 3> parameters;
    const std::array<double, 3> values = {0.05, 0.20, 110.0};
    for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
        scope.RegisterInput(parameters[coordinate], values[coordinate]);
    scope.StartRecording();
    auto result = Example::RecordEuropeanOptions(&scope, {9, 8}, parameters);
    scope.FinishRecording();
    scope.ClearAdjoints();
    for (size_t layer = 0; layer < 2; ++layer)
        NativeOperations_::SetSeed(result.prices_[layer], 1.0, layer);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    const std::array<double, 2> prices = {4.153690693968586, 10.770781220697858};
    const std::array<std::array<double, 3>, 2> risks = {
        {{40.40684028445804, 27.8766807705311, -0.0907395605282994}, {-64.1496803210784, 27.87666960963085, 0.8605082862555484}}};
    ASSERT_EQ(reports.Entries().size(), 10);
    ASSERT_LE(result.largestForwardError_, 1e-12);
    for (size_t layer = 0; layer < 2; ++layer) {
        ASSERT_NEAR(Value(result.prices_[layer]), prices[layer], 1e-10);
        for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
            ASSERT_NEAR(NativeOperations_::ReadAdjoint(parameters[coordinate], layer), risks[layer][coordinate], 1e-9);
    }
    for (const auto& event : result.events_) {
        const auto& errors = reports.Report(event).transposeBackwardErrors_;
        ASSERT_EQ(errors.Rows(), 2);
        ASSERT_EQ(errors.Cols(), 2);
        for (double error : errors)
            ASSERT_LE(error, 1e-12);
    }
    scope.Close();
    Clear(*Tape());
}

TEST(AADSampledThetaStepTest, TestFinancialIndependentDenseChainAndThreeSizeDifferences) {
    const auto result = Financial::Evaluate({9, 8}, 3, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}});
    ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(result, 10, 3));
    const auto independent = Financial::DenseSmallPrice(Financial::PARAMETERS);
    for (size_t layer = 0; layer < 2; ++layer)
        ASSERT_NEAR(result.prices_[layer], independent[layer], 1e-10);
    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
        ASSERT_NO_FATAL_FAILURE(CheckCenteredFinancialDifferences(result, coordinate));
}

TEST(AADSampledThetaStepTest, TestFinancialScalarVectorWeightedRepeatedAndZeroLanes) {
    const Vector_<> scales = {1.0, -2.0, 0.0, 1.5};
    const auto scalar = Financial::Evaluate({9, 8}, 0, {{0.0, 0.0, 1.0}}, scales);
    const auto vector = Financial::Evaluate({9, 8}, 4, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}}, scales);
    ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(scalar, 10, 1));
    ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(vector, 10, 4));
    ASSERT_EQ(scalar.sweeps_.size(), scales.size());
    ASSERT_EQ(vector.sweeps_.size(), scales.size());
    ASSERT_NEAR(scalar.objective_, -2.2859725480626616, 1e-10);
    ASSERT_DOUBLE_EQ(scalar.objective_, vector.objective_);
    const std::array<std::array<double, 3>, 3> expected = {{{40.40684028445804, 27.8766807705311, -0.0907395605282994},
                                                            {-64.1496803210784, 27.87666960963085, 0.8605082862555484},
                                                            {100.62081059638136, 10.93834875594074, -0.7488056653520355}}};
    for (size_t sweep = 0; sweep < scales.size(); ++sweep) {
        for (int coordinate = 0; coordinate < 3; ++coordinate) {
            ASSERT_NEAR(scalar.sweeps_[sweep].risks_(0, coordinate), scales[sweep] * expected[2][coordinate], 1e-9);
            ASSERT_NEAR(vector.sweeps_[sweep].risks_(2, coordinate), scalar.sweeps_[sweep].risks_(0, coordinate), 1e-9);
            ASSERT_EQ(vector.sweeps_[sweep].risks_(3, coordinate), 0.0);
            for (int layer = 0; layer < 3; ++layer)
                ASSERT_NEAR(vector.sweeps_[sweep].risks_(layer, coordinate), scales[sweep] * expected[layer][coordinate], 1e-9);
        }
    }
}

TEST(AADSampledThetaStepTest, TestFinancialThreeLevelFrozenConvergenceAndParity) {
    const auto continuum = Financial::Continuum();
    Matrix_<> previous(2, 4, std::numeric_limits<double>::infinity());
    for (const auto& reference : Financial::REFERENCES) {
        SCOPED_TRACE(reference.settings_.gridPoints_);
        const auto result = Financial::Evaluate(reference.settings_, 2, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}});
        ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(result, reference.settings_.ordinarySteps_ + 2, 2));
        ASSERT_NO_FATAL_FAILURE(CheckFrozenFinancialReference(result, reference.quote_));
        const auto errors = FinancialErrors(result, continuum);
        ASSERT_NO_FATAL_FAILURE(CheckFinancialConvergence(errors, previous, 400.0 / (reference.settings_.gridPoints_ - 1)));
        ASSERT_NO_FATAL_FAILURE(CheckFinancialParity(result, reference.settings_.ordinarySteps_));
        previous = errors;
    }
}

TEST(AADSampledThetaStepTest, TestFinancialTerminalAndDiscountedBoundaryDependencies) {
    const auto result = Financial::Evaluate({9, 8}, 2, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}});
    ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(result, 10, 2));
    struct Omission_ {
        size_t coordinate_;
        double bump_;
        Financial::FrozenDependencies_ frozen_;
        std::array<double, 2> minimum_;
        double differenceCeiling_;
    };
    const std::array<Omission_, 3> omissions = {{{2, 0.025, {true, false}, {0.05, 0.5}, 1e-7},
                                                 {2, 0.025, {false, true}, {1e-6, 5e-5}, 1e-7},
                                                 {0, 0.00025, {false, true}, {5e-5, 0.003}, 6.35e-6}}};
    for (const auto& omission : omissions) {
        SCOPED_TRACE(omission.coordinate_);
        auto plus = Financial::PARAMETERS, minus = Financial::PARAMETERS;
        plus[omission.coordinate_] += omission.bump_;
        minus[omission.coordinate_] -= omission.bump_;
        const auto high = Financial::DenseSmallPrice(plus), low = Financial::DenseSmallPrice(minus);
        const auto partialHigh = Financial::DenseSmallPrice(plus, omission.frozen_), partialLow = Financial::DenseSmallPrice(minus, omission.frozen_);
        for (int layer = 0; layer < 2; ++layer) {
            const double complete = (high[layer] - low[layer]) / (2.0 * omission.bump_);
            const double partial = (partialHigh[layer] - partialLow[layer]) / (2.0 * omission.bump_);
            const double actual = result.sweeps_[0].risks_(layer, static_cast<int>(omission.coordinate_));
            ASSERT_NEAR(actual, complete, omission.differenceCeiling_);
            ASSERT_GT(std::abs(actual - partial), omission.minimum_[layer]);
        }
    }
}

TEST(AADSampledThetaStepTest, TestFinancialMinimumDampedScheduleAndSettingsAdmission) {
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ scope;
    std::array<Number_, 3> parameters;
    for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
        scope.RegisterInput(parameters[coordinate], Financial::PARAMETERS[coordinate]);
    scope.StartRecording();
    for (const Example::EuropeanThetaSettings_ settings :
         {Example::EuropeanThetaSettings_{4, 2}, {6, 2}, {5, 1}, {5, std::numeric_limits<int>::max()}})
        ASSERT_THROW(Example::RecordEuropeanOptions(&scope, settings, parameters), Exception_);
    auto recording = Example::RecordEuropeanOptions(&scope, {5, 2}, parameters);
    scope.FinishRecording();
    scope.ClearAdjoints();
    for (size_t layer = 0; layer < 2; ++layer)
        NativeOperations_::SetSeed(recording.prices_[layer], 1.0, layer);
    const auto reports = ReverseWithSolveAccuracy(&scope);
    const Financial::Quote_ reference{
        {3.0191928457965087, 9.662011535300886},
        {{{43.2807233743607, 16.675123551546886, -0.031187133057668574}, {-60.08887696754076, 16.67119093130088, 0.9203362221742037}}}};
    Financial::Result_ result{{Value(recording.prices_[0]), Value(recording.prices_[1])},
                              0.0,
                              recording.largestForwardError_,
                              recording.events_,
                              {{Financial::ReadRisks(parameters, 2), reports}}};
    ASSERT_NO_FATAL_FAILURE(CheckFinancialReports(result, 4, 2));
    ASSERT_NO_FATAL_FAILURE(CheckFrozenFinancialReference(result, reference));
    scope.Close();
    Clear(*Tape());
}
