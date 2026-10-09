//
// Created by Codex on 2026/10/09.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <memory>
#include <string>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/montecarlocurvature.hpp>
#include <dal/script/simulation.hpp>

#include <script_test_observers.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct ScopedThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> Prepare(const Script::ScriptProductData_& product, double smoothing = 0.25) {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        return std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(product, valuation, {}, {}, smoothing));
    }

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> QuadraticPreparation() {
        const Script::ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 10, 1))},
                                                 {"2", "s = FIX(EQ[DAL196_TEST]) pay PAYS SCALE * s * s"});
        return Prepare(product);
    }

    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> FutureQuadraticPreparation() {
        return Prepare(
            Script::ScriptProductData_("", {Cell_("SCALE"), Cell_(Date_(2026, 12, 1)), Cell_(Date_(2027, 10, 1))},
                                       {"2", "x = FIX(EQ[DAL196_TEST])", "s = FIX(EQ[DAL196_TEST]) pay PAYS SCALE * s * s ON 2028-01-01"}));
    }

    AAD::BumpOverAADRequest_ Direction(const Vector_<>& values, double step = 1e-4) {
        AAD::BumpOverAADRequest_ result;
        result.directions_ = Dal::Matrix_<>(1, static_cast<int>(values.size()), 0.0);
        for (size_t column = 0; column < values.size(); ++column)
            result.directions_(0, static_cast<int>(column)) = values[column];
        result.steps_ = {step};
        return result;
    }

    Vector_<> Products(const Script::MonteCarloCurvatureResult_& result) {
        return Vector_<>(result.HessianProducts().begin(), result.HessianProducts().end());
    }

    struct PolynomialReference_ {
        double value_ = 0.0;
        Vector_<> gradient_ = Vector_<>(5, 0.0);
        Dal::Matrix_<> hessian_ = Dal::Matrix_<>(5, 5, 0.0);
    };

    PolynomialReference_ AnalyticQuadratic(const Script::PreparedScript_& prepared,
                                           const Vector_<>& point,
                                           size_t paths,
                                           const Script::SegmentedMonteCarloSettings_& settings) {
        const auto& times = prepared.TimeLine();
        auto random = Script::CreateRNG(settings.rsg_, times.size(), settings.useBb_, settings.scrambleKey_, settings.normalPrecision_);
        random->SkipNormalTo(settings.firstPath_);
        Vector_<> gaussian(times.size());
        const double time = times.back();
        const double payTime = static_cast<double>(Date_(2028, 1, 1) - prepared.EvaluationDate()) / 365.0;
        const Vector_<> logHessianDiagonal{-2.0 / (point[0] * point[0]), -2.0 * time, 0.0, 0.0};
        PolynomialReference_ result;
        for (size_t path = 0; path < paths; ++path) {
            random->FillNormal(&gaussian);
            double brownian = 0.0;
            for (size_t i = 0; i < times.size(); ++i)
                brownian += std::sqrt(times[i] - (i == 0 ? 0.0 : times[i - 1])) * gaussian[i];
            const double unit = point[0] * point[0] *
                                std::exp((2.0 * (point[2] - point[3]) - point[1] * point[1]) * time + 2.0 * point[1] * brownian - point[2] * payTime);
            const double value = point[4] * unit;
            const Vector_<> logGradient{2.0 / point[0], 2.0 * brownian - 2.0 * point[1] * time, 2.0 * time - payTime, -2.0 * time};
            result.value_ += value / static_cast<double>(paths);
            result.gradient_[4] += unit / static_cast<double>(paths);
            for (int i = 0; i < 4; ++i) {
                result.gradient_[i] += value * logGradient[i] / static_cast<double>(paths);
                result.hessian_(i, 4) += unit * logGradient[i] / static_cast<double>(paths);
                result.hessian_(4, i) = result.hessian_(i, 4);
                for (int j = 0; j < 4; ++j) {
                    const double curvature = i == j ? logHessianDiagonal[i] : 0.0;
                    result.hessian_(i, j) += value * (logGradient[i] * logGradient[j] + curvature) / static_cast<double>(paths);
                }
            }
        }
        return result;
    }

    struct MutateInputs_ : Script::Detail::SimulationObserver_ {
        Vector_<>* point_;
        AAD::BumpOverAADRequest_* bumps_;
        Script::SegmentedMonteCarloSettings_* settings_;
        Script::BlackScholesSegmentedPath_* kernel_;
        bool changed_ = false;
        MutateInputs_(Vector_<>* point,
                      AAD::BumpOverAADRequest_* bumps,
                      Script::SegmentedMonteCarloSettings_* settings,
                      Script::BlackScholesSegmentedPath_* kernel)
            : point_(point), bumps_(bumps), settings_(settings), kernel_(kernel) {}
        void AfterSubmission() override {
            if (changed_)
                return;
            changed_ = true;
            *point_ = {1.0};
            *bumps_ = {};
            settings_->rsg_ = "invalid-after-admission";
            settings_->normalPrecision_ = "invalid-after-admission";
            settings_->path_.segmentSteps_ = 0;
            *kernel_ = Script::BlackScholesSegmentedPath_(QuadraticPreparation());
        }
    };
} // namespace

TEST(MonteCarloCurvatureTest, TestTimeZeroGammaCrossAndSignedHessianProducts) {
    const ScopedThreads_ threads(1);
    const Script::BlackScholesSegmentedPath_ kernel(QuadraticPreparation());
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(3, 5, 0.0);
    bumps.directions_(0, 0) = 1.0;
    bumps.directions_(1, 4) = 1.0;
    bumps.directions_(2, 0) = -2.0;
    bumps.directions_(2, 4) = 0.5;
    bumps.steps_ = {0.25, 0.25, 0.25};
    const auto result = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, bumps);
    ASSERT_DOUBLE_EQ(result.Base().MeanValue(), 20000.0);
    ASSERT_EQ(result.Base().MeanGradient(), (Vector_<>{400.0, 0.0, 0.0, 0.0, 10000.0}));
    const double expected[3][5] = {{4.0, 0.0, 0.0, 0.0, 200.0}, {200.0, 0.0, 0.0, 0.0, 0.0}, {92.0, 0.0, 0.0, 0.0, -400.0}};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 5; ++column)
            ASSERT_DOUBLE_EQ(result.HessianProducts()(row, column), expected[row][column]);
    ASSERT_EQ(result.Execution().method_, "BumpOverSegmentedNativeAAD");
    ASSERT_EQ(result.Execution().gradientEvaluations_, 7);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, AAD::BumpOverAADPayloadBytes(5, 3));
    ASSERT_EQ(result.Point(), point);
    ASSERT_EQ(result.Prepared().EvaluationDate(), Date_(2026, 10, 1));
    ASSERT_EQ(result.Prepared().Simulation().smooth_, 0.25);
}

TEST(MonteCarloCurvatureTest, TestCommonPathsAgainstIndependentGBMPolynomialHessian) {
    const ScopedThreads_ threads(2);
    const auto preparation = FutureQuadraticPreparation();
    const Script::BlackScholesSegmentedPath_ kernel(preparation);
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(6, 5, 0.0);
    for (int i = 0; i < 5; ++i)
        bumps.directions_(i, i) = 1.0;
    const Vector_<> mixed{-2.0, 0.3, -0.2, 0.1, 0.5};
    for (int i = 0; i < 5; ++i)
        bumps.directions_(5, i) = mixed[i];
    bumps.steps_ = Vector_<>(6, 1e-5);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    settings.scrambleKey_ = 194;
    settings.useBb_ = true;
    settings.normalPrecision_ = "Precise";
    settings.path_.segmentSteps_ = 1;
    const auto expected = AnalyticQuadratic(preparation->Prepared(), point, 65, settings);
    const auto result = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings);
    ASSERT_NEAR(result.Base().MeanValue(), expected.value_, 1e-8);
    for (int column = 0; column < 5; ++column) {
        ASSERT_NEAR(result.Base().MeanGradient()[column], expected.gradient_[column], 1e-8);
        for (int row = 0; row < 6; ++row) {
            double product = 0.0;
            for (int axis = 0; axis < 5; ++axis)
                product += expected.hessian_(column, axis) * bumps.directions_(row, axis);
            ASSERT_NEAR(result.HessianProducts()(row, column), product, 2e-5);
        }
    }
    ASSERT_EQ(result.Base().ParameterLabels(), kernel.ParameterLabels());
    ASSERT_EQ(result.Base().Execution().firstPath_, 7);
    ASSERT_EQ(result.Base().Execution().normalPrecision_, "Precise");
    ASSERT_EQ(result.Base().Execution().scrambleKey_, settings.scrambleKey_);
    ASSERT_EQ(result.Execution().gradientEvaluations_, 13);
    ASSERT_GE(result.Execution().maxPathTapeBytes_, result.Base().Execution().maxPathTapeBytes_);
    ASSERT_GT(result.Execution().maxPathCheckpointBytes_, 0);
}

TEST(MonteCarloCurvatureTest, TestSmoothQuarticStepRefinement) {
    const ScopedThreads_ threads(1);
    const auto prepared = Prepare(Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 1))}, {"s = FIX(EQ[DAL196_TEST]) pay PAYS s * s * s * s"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    double previous = 0.0;
    for (double step : {0.5, 0.25, 0.125}) {
        const auto result = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {3.0, 0.2, 0.03, 0.01}, 1, Direction({1.0, 0.0, 0.0, 0.0}, step));
        const double error = result.HessianProducts()(0, 0) - 108.0;
        ASSERT_NEAR(error, 4.0 * step * step, 1e-10);
        if (previous != 0.0) {
            ASSERT_NEAR(previous / error, 4.0, 1e-10);
        }
        previous = error;
    }
}

TEST(MonteCarloCurvatureTest, TestVanillaCallGammaWithDeclaredFiniteStepTolerance) {
    const ScopedThreads_ threads(2);
    const auto prepared =
        Prepare(Script::ScriptProductData_("", {Cell_("K"), Cell_(Date_(2027, 10, 1))}, {"100", "pay PAYS MAX(FIX(EQ[DAL196_TEST]) - K, 0)"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    settings.normalPrecision_ = "Precise";
    const auto result = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {100.0, 0.2, 0.03, 0.01, 100.0}, 8192,
                                                                        Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 1.0), settings);
    const double d1 = (0.03 - 0.01 + 0.5 * 0.2 * 0.2) / 0.2;
    const double gamma = std::exp(-0.01 - 0.5 * d1 * d1) / (100.0 * 0.2 * std::sqrt(2.0 * std::acos(-1.0)));
    ASSERT_NEAR(result.HessianProducts()(0, 0), gamma, 2e-4);
    ASSERT_FALSE(AAD::NativeOperations_::Capabilities().higherOrder_);
}

TEST(MonteCarloCurvatureTest, TestAllBumpedDomainsAdmittedBeforeAnySubmission) {
    const Script::BlackScholesSegmentedPath_ kernel(QuadraticPreparation());
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    Script::TestSupport::SubmissionCounter_ submissions;
    const Script::Detail::ScopedSimulationObserver_ observer(&submissions);
    for (const auto& bumps : {Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 101.0), Direction({0.0, 1.0, 0.0, 0.0, 0.0}, 0.3)}) {
        try {
            (void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, bumps);
            FAIL() << "invalid minus model domain was admitted";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("direction=0; minus"), std::string::npos);
        }
    }
    const auto valid = Direction({1.0, 0.0, 0.0, 0.0, 0.0});
    auto invalid = valid;
    invalid.steps_ = {};
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, invalid), Dal::Exception_);
    invalid = valid;
    invalid.directions_(0, 0) = std::numeric_limits<double>::infinity();
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, invalid), Dal::Exception_);
    invalid = valid;
    invalid.steps_[0] = std::numeric_limits<double>::denorm_min();
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, invalid), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {}, 35, valid), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 0, valid), Dal::Exception_);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.normalPrecision_ = "precise";
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, valid, settings), Dal::Exception_);
    settings.normalPrecision_ = "Default";
    settings.firstPath_ = std::numeric_limits<size_t>::max();
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 2, valid, settings), Dal::Exception_);
    settings.firstPath_ = 0;
    settings.path_.segmentSteps_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 35, valid, settings), Dal::Exception_);
    ASSERT_EQ(submissions.submissions_, 0);
}

TEST(MonteCarloCurvatureTest, TestSnapshotOfKernelPointBumpsAndRandomSettings) {
    const ScopedThreads_ threads(2);
    Script::BlackScholesSegmentedPath_ kernel(FutureQuadraticPreparation());
    Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    auto bumps = Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 0.25);
    Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    const auto expected = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings);
    MutateInputs_ mutate(&point, &bumps, &settings, &kernel);
    const Script::Detail::ScopedSimulationObserver_ observer(&mutate);
    const auto result = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings);
    ASSERT_TRUE(mutate.changed_);
    ASSERT_EQ(result.Base().MeanGradient(), expected.Base().MeanGradient());
    ASSERT_EQ(Products(result), Products(expected));
    ASSERT_EQ(result.Point(), expected.Point());
    ASSERT_EQ(result.Steps(), expected.Steps());
    ASSERT_EQ(result.Settings().firstPath_, 7);
    ASSERT_EQ(result.Settings().rsg_, "sobol");
    ASSERT_EQ(result.Prepared().TimeLine(), expected.Prepared().TimeLine());
}

TEST(MonteCarloCurvatureTest, TestPerPathBudgetIntersectionAndRecovery) {
    const ScopedThreads_ threads(2);
    const Script::BlackScholesSegmentedPath_ kernel(FutureQuadraticPreparation());
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    auto bumps = Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 0.25);
    const auto expected = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps);
    bumps.numericPayloadBudgetBytes_ = expected.Execution().numericPayloadBytes_ - 1;
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps), Dal::Exception_);
    bumps.numericPayloadBudgetBytes_ = expected.Execution().numericPayloadBytes_;
    Script::SegmentedMonteCarloSettings_ settings;
    settings.path_.checkpointCapacityBudgetBytes_ = expected.Execution().maxPathCheckpointBytes_;
    const size_t capacity = expected.Execution().maxPathTapeBytes_ + expected.Execution().maxPathCleanupReserveBytes_;
    settings.path_.recordingCapacityBudgetBytes_ = capacity;
    bumps.recordingCapacityBudgetBytes_ = capacity + 1;
    const auto bounded = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings);
    ASSERT_EQ(bounded.Execution().recordingCapacityBudgetBytes_, capacity);
    ASSERT_EQ(Products(bounded), Products(expected));
    bumps.recordingCapacityBudgetBytes_ = capacity;
    settings.path_.recordingCapacityBudgetBytes_ = capacity + 1;
    const auto other = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings);
    ASSERT_EQ(other.Execution().recordingCapacityBudgetBytes_, capacity);
    ASSERT_EQ(other.Settings().path_.recordingCapacityBudgetBytes_, capacity + 1);
    for (bool fromBump : {false, true}) {
        bumps.recordingCapacityBudgetBytes_ = fromBump ? std::optional<size_t>(0) : std::nullopt;
        settings.path_.recordingCapacityBudgetBytes_ = fromBump ? std::nullopt : std::optional<size_t>(0);
        ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings), Dal::Exception_);
    }
    bumps.recordingCapacityBudgetBytes_.reset();
    settings.path_.recordingCapacityBudgetBytes_.reset();
    settings.path_.checkpointCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings), Dal::Exception_);
    const auto recovered = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps);
    ASSERT_EQ(Products(recovered), Products(expected));
}

TEST(MonteCarloCurvatureTest, TestFailureRecoveryRestoresWideCallerAndNestedGraph) {
    const ScopedThreads_ threads(2);
    const Script::BlackScholesSegmentedPath_ kernel(FutureQuadraticPreparation());
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    const auto bumps = Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 0.25);
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    {
        Script::TestSupport::RejectSubmissions_ reject;
        const Script::Detail::ScopedSimulationObserver_ observer(&reject);
        ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps), Dal::Exception_);
        ASSERT_EQ(reject.calls_, 1);
    }
    auto failing = point;
    failing[2] = 1e308;
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, failing, 65, bumps), Dal::Exception_);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    ASSERT_GT(Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps).HessianProducts()(0, 0), 0.0);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    AAD::RecordingScope_ outer;
    AAD::Number_ input;
    outer.RegisterInput(input, 3.0);
    outer.StartRecording();
    AAD::Number_ root = input * input;
    ASSERT_THROW((void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps), Dal::Exception_);
    outer.FinishRecording();
    AAD::NativeOperations_::SetSeed(root, 1.0, 2);
    outer.Reverse();
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 6.0);
    outer.Close();
}

TEST(MonteCarloCurvatureTest, TestEmptyDirectionsFinalZeroDriverPathAndDetachedOwnership) {
    const ScopedThreads_ threads(1);
    const auto detached = []() {
        const Script::BlackScholesSegmentedPath_ kernel(QuadraticPreparation());
        AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Dal::Matrix_<>(0, 5);
        Script::SegmentedMonteCarloSettings_ settings;
        settings.rsg_ = "irn";
        settings.useBb_ = true;
        settings.firstPath_ = std::numeric_limits<size_t>::max();
        return Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {100.0, 0.2, 0.03, 0.01, 2.0}, 1, bumps, settings);
    }();
    ASSERT_EQ(detached.HessianProducts().Rows(), 0);
    ASSERT_EQ(detached.HessianProducts().Cols(), 5);
    ASSERT_EQ(detached.Execution().gradientEvaluations_, 1);
    ASSERT_EQ(detached.Base().MeanValue(), 20000.0);
    ASSERT_EQ(detached.Prepared().Simulation().smooth_, 0.25);
    ASSERT_EQ(detached.Base().ParameterLabels().size(), 5);
    ASSERT_EQ(detached.Base().Execution().firstPath_, std::numeric_limits<size_t>::max());
}

TEST(MonteCarloCurvatureTest, TestPlusAndMinusWorkerFailuresIdentifyPhaseAndRecover) {
    const ScopedThreads_ threads(2);
    const auto prepared =
        Prepare(Script::ScriptProductData_("", {Cell_("K"), Cell_(Date_(2026, 10, 1))}, {"99", "pay PAYS 1 / (FIX(EQ[DAL196_TEST]) - K)"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const auto bumps = Direction({1.0, 0.0, 0.0, 0.0, 0.0}, 1.0);
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    for (double strike : {101.0, 99.0}) {
        try {
            (void)Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {100.0, 0.2, 0.03, 0.01, strike}, 65, bumps);
            FAIL() << "singular bumped payoff was admitted";
        } catch (const Dal::Exception_& error) {
            const std::string phase = strike == 101.0 ? "direction=0; plus" : "direction=0; minus";
            ASSERT_NE(std::string(error.what()).find(phase), std::string::npos);
        }
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 4);
        const auto recovered = Script::EvaluateBlackScholesMonteCarloCurvature(kernel, {100.0, 0.2, 0.03, 0.01, 90.0}, 65, bumps);
        ASSERT_TRUE(std::isfinite(recovered.HessianProducts()(0, 0)));
    }
}

TEST(MonteCarloCurvatureTest, TestWorkerCountDeterminismAndConcurrentCallers) {
    const Script::BlackScholesSegmentedPath_ kernel(FutureQuadraticPreparation());
    const Vector_<> point{100.0, 0.2, 0.03, 0.01, 2.0};
    const auto bumps = Direction({-2.0, 0.3, -0.2, 0.1, 0.5});
    Script::SegmentedMonteCarloSettings_ settings;
    settings.rsg_ = "mrg32";
    settings.useBb_ = true;
    settings.firstPath_ = 7;
    const auto evaluate = [&]() { return Script::EvaluateBlackScholesMonteCarloCurvature(kernel, point, 65, bumps, settings); };
    const auto one = [&]() {
        const ScopedThreads_ threads(1);
        return evaluate();
    }();
    const ScopedThreads_ threads(2);
    auto first = std::async(std::launch::async, evaluate);
    auto second = std::async(std::launch::async, evaluate);
    const auto two = first.get();
    const auto three = second.get();
    for (const auto* result : {&two, &three}) {
        ASSERT_EQ(result->Base().MeanValue(), one.Base().MeanValue());
        ASSERT_EQ(result->Base().MeanGradient(), one.Base().MeanGradient());
        ASSERT_EQ(Products(*result), Products(one));
    }
}
