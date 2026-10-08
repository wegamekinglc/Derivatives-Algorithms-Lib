//
// Created by Codex on 2026/10/8.
//

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include <dal/math/pde/sampledthetastep.hpp>

using Dal::Matrix_;
using Dal::Vector_;
using Dal::PDE::SampledThetaStepInputs_;
using Dal::PDE::SampledThetaStepPullback_;

namespace {
    Matrix_<> RollbackMatrix(const std::array<std::array<double, 2>, 5>& values) {
        Matrix_<> result(5, 2);
        for (int row = 0; row < 5; ++row)
            for (int layer = 0; layer < 2; ++layer)
                result(row, layer) = values[row][layer];
        return result;
    }

    SampledThetaStepInputs_ RollbackInputs() {
        SampledThetaStepInputs_ inputs;
        inputs.x_ = {0.0, 0.7, 1.8, 3.0, 4.5};
        inputs.rates_ = {0.021, 0.022, 0.023};
        inputs.drifts_ = {-0.05, 0.0, 0.05};
        inputs.variances_ = {0.42, 0.44, 0.46};
        const std::array<std::array<double, 2>, 5> old = {{{0.6, 0.35},
                                                           {0.9333333333333333, 0.6833333333333333},
                                                           {1.2666666666666666, 1.0166666666666666},
                                                           {1.6, 1.35},
                                                           {1.9333333333333333, 1.6833333333333333}}};
        inputs.oldValues_ = RollbackMatrix(old);
        inputs.externalValues_ = Matrix_<>(2, 2);
        inputs.externalValues_(0, 0) = 1.3;
        inputs.externalValues_(0, 1) = 1.1;
        inputs.externalValues_(1, 0) = -0.4;
        inputs.externalValues_(1, 1) = -0.23333333333333334;
        return inputs;
    }

    void CheckRollbackMatrix(const Matrix_<>& actual, const Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int layer = 0; layer < actual.Cols(); ++layer)
                ASSERT_NEAR(actual(row, layer), expected(row, layer), 1e-12);
    }

    void CheckRollbackVector(const Vector_<>& actual, const Vector_<>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (size_t row = 0; row < actual.size(); ++row)
            ASSERT_NEAR(actual[row], expected[row], 1e-12);
    }
} // namespace

TEST(SampledThetaStepTest, TestIndependentCompleteThreeStepRollback) {
    // Frozen 80-digit complete solves independently checked all 33 risk coordinates.
    const std::array<std::array<double, 2>, 5> finalValues = {{{1.3, 1.1},
                                                               {0.9808999793787134, 0.7400612518770165},
                                                               {1.2437468438446349, 0.9985284418088635},
                                                               {1.4436660153183267, 1.221467234003024},
                                                               {-0.4, -0.23333333333333334}}};
    const std::array<std::array<double, 2>, 5> initialRisk = {{{0.005872972982808527, 0.014939215027417737},
                                                               {0.07109964645761843, 0.18435120875116356},
                                                               {0.0026931176505304577, 0.14007074668163677},
                                                               {-0.08510873238720855, 0.04883451950307496},
                                                               {-0.0011235967198862248, 0.0006148164354483977}}};
    const std::array<std::array<double, 2>, 5> seedValues = {{{0.2, 0.34285714285714286},
                                                              {0.1, 0.24285714285714285},
                                                              {0.0, 0.14285714285714285},
                                                              {-0.1, 0.04285714285714286},
                                                              {-0.2, -0.05714285714285714}}};
    const std::array<std::array<bool, 2>, 3> boundaries = {{{false, true}, {true, false}, {true, true}}};
    const std::array<double, 3> theta = {0.0, 0.5, 1.0};
    const std::array<double, 3> dtRisk = {-0.022913760612491972, 0.029842173154236425, 0.060577292362363616};
    const std::array<double, 3> thetaRisk = {0.001536709255203283, 0.014937112799043627, -0.0031285875692281056};
    std::array<Matrix_<>, 3> boundaryRisk = {
        Matrix_<>(2, 2),
        Matrix_<>(2, 2),
        Matrix_<>(2, 2),
    };
    boundaryRisk[0](0, 0) = 0.0;
    boundaryRisk[0](0, 1) = 0.0;
    boundaryRisk[0](1, 0) = -0.0023104138829004675;
    boundaryRisk[0](1, 1) = 0.0012043488902315953;
    boundaryRisk[1](0, 0) = 0.003025474425184163;
    boundaryRisk[1](0, 1) = 0.00764990195697253;
    boundaryRisk[1](1, 0) = 0.0;
    boundaryRisk[1](1, 1) = 0.0;
    boundaryRisk[2](0, 0) = 0.20961035087823188;
    boundaryRisk[2](0, 1) = 0.3668886153876424;
    boundaryRisk[2](1, 0) = -0.20356045148904273;
    boundaryRisk[2](1, 1) = -0.05537425977850755;
    auto inputs = RollbackInputs();
    std::vector<SampledThetaStepPullback_> steps;
    steps.reserve(3);
    for (int index = 0; index < 3; ++index) {
        inputs.dt_ = 0.1 * (index + 1);
        inputs.theta_ = theta[index];
        inputs.externalBoundaries_ = boundaries[index];
        steps.emplace_back(inputs, Dal::LinearSolveAccuracyPolicy_{1e-14, 1e-14});
        inputs.oldValues_ = steps.back().Solution();
        for (double error : steps.back().ForwardBackwardErrors())
            ASSERT_LE(error, 1e-14);
    }
    CheckRollbackMatrix(steps.back().Solution(), RollbackMatrix(finalValues));
    auto seed = RollbackMatrix(seedValues);
    Vector_<> rates(3, 0.0), drifts(3, 0.0), variances(3, 0.0);
    for (int index = 2; index >= 0; --index) {
        const auto risk = steps[index].Reverse(seed);
        ASSERT_NEAR(risk.dt_, dtRisk[index], 1e-12);
        ASSERT_NEAR(risk.theta_, thetaRisk[index], 1e-12);
        CheckRollbackMatrix(risk.externalValues_, boundaryRisk[index]);
        for (int row = 0; row < 3; ++row) {
            rates[row] += risk.rates_[row];
            drifts[row] += risk.drifts_[row];
            variances[row] += risk.variances_[row];
        }
        for (double error : risk.transposeBackwardErrors_)
            ASSERT_LE(error, 1e-14);
        seed = risk.oldValues_;
    }
    CheckRollbackMatrix(seed, RollbackMatrix(initialRisk));
    CheckRollbackVector(rates, Vector_<>{-0.13507391566300417, -0.08668274903846021, 0.04650592914579528});
    CheckRollbackVector(drifts, Vector_<>{-0.0031463687212749747, 0.021244428194340005, 0.01202508636860952});
    CheckRollbackVector(variances, Vector_<>{0.04481095917927114, -0.001588408354080354, 0.01444327539506597});
}
