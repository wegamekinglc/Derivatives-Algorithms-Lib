//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include <dal/math/aad/native.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/lsmccurvature.hpp>
#include <dal/script/simulation.hpp>

#include "script_test_observers.hpp"

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct PoolRestore_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        ~PoolRestore_() { pool_->Start(threads_, true); }
    };

    std::shared_ptr<const Script::PreparedScript_>
    Preparation(bool compiled = true, const Dal::String_& mode = "Frozen", Script::MonteCarloSettings_ simulation = {}) {
        const Script::ScriptProductData_ product("", {Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                                 {"EXERCISE 100 - spot()", "EXERCISE 100 - spot()"});
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        simulation.smooth_ = 2.0;
        if (!simulation.lsmcTrainingPaths_)
            simulation.lsmcTrainingPaths_ = 256;
        simulation.lsmcPolicyRiskMode_ = mode;
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 10);
        AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
        return std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
    }

    AAD::BumpOverAADRequest_ SpotDirection(size_t inputs = 4, double step = 0.1) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, static_cast<int>(inputs), 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {step};
        return request;
    }

    std::shared_ptr<const Script::PreparedScript_> HistoricalPreparation(bool compiled, const Dal::String_& mode, double constant) {
        const Script::ScriptProductData_ product("", {Cell_("K"), Cell_(Date_(2026, 10, 9)), Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                                 {Dal::String_(std::to_string(constant)), "x = 2 * K", "EXERCISE x - spot()", "EXERCISE x - spot()"});
        Script::MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        simulation.smooth_ = 2.0;
        simulation.lsmcTrainingPaths_ = 256;
        simulation.lsmcPolicyRiskMode_ = mode;
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 10);
        AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
        return std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
    }

    double Predict(const Script::ExerciseRegression_& policy, double spot) {
        const bool scalar = policy.powers_.empty();
        const double z = (spot - (scalar ? policy.mean_ : policy.means_[0])) / (scalar ? policy.sigma_ : policy.sigmas_[0]);
        double value = 0.0;
        for (size_t i = 0; i < policy.coefficients_.size(); ++i)
            value += policy.coefficients_[i] * std::pow(z, scalar ? i : policy.powers_[i][0]);
        return value;
    }

    double FrozenPrice(const Script::BlackScholesLsmcCurvatureResult_& result, const Vector_<>& x) {
        const auto& prepared = result.Prepared();
        const auto& policy = result.BasePolicy();
        const auto& times = prepared.TimeLine();
        auto random = Script::CreateRNG("sobol", times.size(), false);
        random->SkipTo(result.Execution().trainingPaths_);
        Vector_<> gauss(times.size()), spots(times.size());
        const double eps = prepared.Simulation().smooth_;
        double sum = 0.0;
        for (size_t path = 0; path < result.Execution().pathsPerReplicate_; ++path) {
            random->FillNormal(&gauss);
            double spot = x[0], previous = 0.0;
            for (size_t date = 0; date < times.size(); ++date) {
                const double dt = times[date] - previous;
                spot *= std::exp((x[2] - x[3] - 0.5 * x[1] * x[1]) * dt + x[1] * std::sqrt(dt) * gauss[date]);
                spots[date] = spot;
                previous = times[date];
            }
            double value = 0.0;
            for (size_t date = times.size(); date-- > 0;) {
                if (date + 1 < times.size())
                    value *= std::exp(-x[2] * (times[date + 1] - times[date]));
                const double exercise = (x.size() == 5 ? 2.0 * x[4] : 100.0) - spots[date];
                const double degree =
                    std::clamp((exercise - Predict(policy[date], spots[date])) / eps + 0.5, 0.0, 1.0) * std::clamp(exercise / eps, 0.0, 1.0);
                value = degree * exercise + (1.0 - degree) * value;
            }
            sum += value * std::exp(-x[2] * times.front());
        }
        return sum / result.Execution().pathsPerReplicate_;
    }

    double FrozenGamma(const Script::BlackScholesLsmcCurvatureResult_& result, double inner) {
        const double outer = result.Steps()[0];
        auto x = result.Point();
        x[0] += outer + inner;
        const double pp = FrozenPrice(result, x);
        x[0] -= 2.0 * inner;
        const double pm = FrozenPrice(result, x);
        x[0] -= 2.0 * outer;
        const double mm = FrozenPrice(result, x);
        x[0] += 2.0 * inner;
        const double mp = FrozenPrice(result, x);
        return ((pp - pm) - (mp - mm)) / (4.0 * outer * inner);
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestCurvatureFrozenKeepsBasePolicy) {
    const auto prepared = Preparation();
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection());
    ASSERT_EQ(result.BasePolicy().size(), 2);
    ASSERT_NEAR(result.Value(), FrozenPrice(result, result.Point()), 1e-10);
    ASSERT_NEAR(result.HessianProducts()(0, 0), FrozenGamma(result, 1e-3), 2e-5);
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRetrainedMatchesDeclaredGradientEstimator) {
    const auto prepared = Preparation(true, "RetrainedBump");
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection());
    const auto gradient = [&](double spot) {
        const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", spot, 0.2, 0.05, 0.0));
        return Script::MCLsmcAadSimulation(*prepared, data, 128).risks_;
    };
    const auto base = gradient(100.0), plus = gradient(100.1), minus = gradient(99.9);
    for (size_t j = 0; j < base.size(); ++j) {
        ASSERT_NEAR(result.Gradient()[j], base[j], 1e-11);
        ASSERT_NEAR(result.HessianProducts()(0, j), (plus[j] - minus[j]) / 0.2, 1e-10);
    }
}

TEST(ScriptExerciseLSMCTest, TestCurvatureBudgetsAndRecovery) {
    const auto prepared = Preparation();
    auto request = SpotDirection();
    request.numericPayloadBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, request), Dal::Exception_);
    request.numericPayloadBudgetBytes_.reset();
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, request), Dal::Exception_);
    request.recordingCapacityBudgetBytes_.reset();
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, request);
    ASSERT_GT(result.Execution().maxBatchTapeBytes_, 0);
    ASSERT_GT(result.Execution().maxBatchCleanupReserveBytes_, 0);
    ASSERT_EQ(result.Execution().gradientEvaluations_, 3);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, AAD::BumpOverAADPayloadBytes(4, 1));
}

TEST(ScriptExerciseLSMCTest, TestCurvatureFrozenConstantsRebuildHistoryAndRemainActive) {
    for (bool compiled : {false, true}) {
        SCOPED_TRACE(compiled);
        const auto prepared = HistoricalPreparation(compiled, "Frozen", 45.0);
        const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0, 50.0}, 128, SpotDirection(5));
        ASSERT_NEAR(result.Value(), FrozenPrice(result, result.Point()), 1e-10);
        for (size_t j = 0; j < result.Point().size(); ++j) {
            SCOPED_TRACE(j);
            const double inner = j == 1 || j == 2 || j == 3 ? 1e-6 : 1e-3;
            auto plus = result.Point(), minus = result.Point();
            plus[j] += inner;
            minus[j] -= inner;
            ASSERT_NEAR(result.Gradient()[j], (FrozenPrice(result, plus) - FrozenPrice(result, minus)) / (2.0 * inner), 2e-6);
            plus[0] += result.Steps()[0];
            minus[0] += result.Steps()[0];
            const double up = (FrozenPrice(result, plus) - FrozenPrice(result, minus)) / (2.0 * inner);
            plus[0] -= 2.0 * result.Steps()[0];
            minus[0] -= 2.0 * result.Steps()[0];
            const double down = (FrozenPrice(result, plus) - FrozenPrice(result, minus)) / (2.0 * inner);
            ASSERT_NEAR(result.HessianProducts()(0, j), (up - down) / (2.0 * result.Steps()[0]), 2e-5);
        }
        ASSERT_DOUBLE_EQ(prepared->Product().ConstVarValues()[0], 45.0);
    }
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRetrainedConstantsUseCurrentOuterPoint) {
    for (bool compiled : {false, true}) {
        SCOPED_TRACE(compiled);
        auto request = SpotDirection(5, 0.05);
        request.directions_(0, 0) = 0.0;
        request.directions_(0, 4) = 1.0;
        const auto result = Script::EvaluateBlackScholesLsmcCurvature(HistoricalPreparation(compiled, "RetrainedBump", 45.0),
                                                                      {100.0, 0.2, 0.05, 0.0, 50.0}, 128, request);
        const auto gradient = [&](double constant) {
            const auto prepared = HistoricalPreparation(compiled, "RetrainedBump", constant);
            const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.05, 0.0));
            return Script::MCLsmcAadSimulation(*prepared, data, 128).risks_;
        };
        const auto base = gradient(50.0), plus = gradient(50.05), minus = gradient(49.95);
        for (size_t j = 0; j < base.size(); ++j) {
            SCOPED_TRACE(j);
            ASSERT_NEAR(result.Gradient()[j], base[j], 1e-11);
            ASSERT_NEAR(result.HessianProducts()(0, j), (plus[j] - minus[j]) / 0.1, 1e-9);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestCurvaturePreflightRejectsBeforeSubmission) {
    const auto prepared = Preparation();
    Script::TestSupport::SubmissionCounter_ observer;
    const Script::Detail::ScopedSimulationObserver_ scope(&observer);
    const Vector_<> point{100.0, 0.2, 0.05, 0.0};
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature({}, point, 128, SpotDirection()), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2}, 128, SpotDirection()), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 0, SpotDirection()), Dal::Exception_);
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, std::numeric_limits<uint32_t>::max(), SpotDirection()),
                 Dal::Exception_);
    auto request = SpotDirection();
    request.directions_(0, 0) = 0.0;
    request.directions_(0, 1) = 1.0;
    request.steps_[0] = 0.3;
    try {
        (void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 128, request);
        FAIL() << "negative outer volatility accepted";
    } catch (const Dal::Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("direction=0; minus"), std::string::npos);
    }
    request = SpotDirection();
    request.steps_[0] = std::numeric_limits<double>::denorm_min();
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 128, request), Dal::Exception_);
    request = SpotDirection();
    request.directions_(0, 0) = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 128, request), Dal::Exception_);
    ASSERT_EQ(observer.submissions_, 0);
}

TEST(ScriptExerciseLSMCTest, TestCurvatureInnerBumpsPreflightBothRepresentableSides) {
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcPolicyBumpRelative_ = 1e-16;
    const auto prepared = Preparation(true, "RetrainedBump", simulation);
    Script::TestSupport::SubmissionCounter_ observer;
    const Script::Detail::ScopedSimulationObserver_ scope(&observer);
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, -1.0, 0.0}, 128, SpotDirection()), Dal::Exception_);
    ASSERT_EQ(observer.submissions_, 0);
}

TEST(ScriptExerciseLSMCTest, TestCurvatureAdjointModesAndNestedRecording) {
    const auto prepared = Preparation();
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection());
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    auto request = SpotDirection();
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, result.Point(), 128, request), Dal::Exception_);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    const auto scalar = AAD::SetNumResultsForAAD(false, 1);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    AAD::Number_ output = input * input;
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, result.Point(), 128, SpotDirection()), Dal::Exception_);
    recording.FinishRecording();
    AAD::Adjoint(output) = 1.0;
    recording.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(input), 4.0);
    recording.Close();
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRqmcAdaptivePolicyAndConcurrentReuse) {
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcValidationPaths_ = 64;
    simulation.lsmcRqmcReplicates_ = 2;
    simulation.lsmcTrainingSeed_ = 17;
    simulation.lsmcPricingSeed_ = 29;
    simulation.useBb_ = true;
    simulation.normalPrecision_ = "Precise";
    const auto tree = Preparation(false, "Frozen", simulation), compiled = Preparation(true, "Frozen", simulation);
    const Vector_<> point{100.0, 0.2, 0.05, 0.0};
    auto first = std::async(std::launch::async, [&] { return Script::EvaluateBlackScholesLsmcCurvature(compiled, point, 128, SpotDirection()); });
    const auto treeResult = Script::EvaluateBlackScholesLsmcCurvature(tree, point, 128, SpotDirection());
    const auto compiledResult = first.get();
    const auto again = Script::EvaluateBlackScholesLsmcCurvature(compiled, point, 128, SpotDirection());
    ASSERT_EQ(compiledResult.Value(), again.Value());
    ASSERT_EQ(compiledResult.Gradient(), again.Gradient());
    ASSERT_NEAR(treeResult.Value(), compiledResult.Value(), 1e-10);
    for (size_t j = 0; j < point.size(); ++j) {
        ASSERT_EQ(compiledResult.HessianProducts()(0, j), again.HessianProducts()(0, j));
        ASSERT_NEAR(treeResult.Gradient()[j], compiledResult.Gradient()[j], 1e-10);
        ASSERT_NEAR(treeResult.HessianProducts()(0, j), compiledResult.HessianProducts()(0, j), 1e-9);
    }
    ASSERT_EQ(compiledResult.Execution().validationPaths_, 64);
    ASSERT_EQ(compiledResult.Execution().pricingReplicates_, 2);
    ASSERT_TRUE(compiledResult.BasePolicy()[0].validationMse_);
}

TEST(ScriptExerciseLSMCTest, TestCurvatureUnsupportedPreparationAndEmptyDirections) {
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 10);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
    for (const auto& product : {Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 9))}, {"pay PAYS 1"}),
                                Script::ScriptProductData_("", {Cell_(Date_(2027, 10, 10))}, {"pay PAYS spot()"})}) {
        const auto prepared = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
        ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection()), Dal::Exception_);
    }
    const Script::ScriptProductData_ product("", {Cell_(Date_(2027, 10, 10))}, {"EXERCISE 100 - spot()"});
    simulation.enableAad_ = false;
    const auto passive = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(passive, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection()), Dal::Exception_);
    AAD::BumpOverAADRequest_ empty;
    empty.directions_ = Dal::Matrix_<>(0, 4);
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(Preparation(), {100.0, 0.2, 0.05, 0.0}, 128, empty);
    ASSERT_EQ(result.HessianProducts().Rows(), 0);
    ASSERT_EQ(result.HessianProducts().Cols(), 4);
    ASSERT_EQ(result.Execution().gradientEvaluations_, 1);
    ASSERT_EQ(result.Execution().method_, "BumpOverFrozenNativeLsmcAAD");
}

TEST(ScriptExerciseLSMCTest, TestCurvatureBatchThreadParityAndSubmissionFailureRecovery) {
    PoolRestore_ restore;
    const auto prepared = Preparation();
    const Vector_<> point{100.0, 0.2, 0.05, 0.0};
    restore.pool_->Start(1, true);
    const auto single = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 8193, SpotDirection());
    restore.pool_->Start(4, true);
    const auto parallel = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 8193, SpotDirection());
    ASSERT_EQ(single.Value(), parallel.Value());
    ASSERT_EQ(single.Gradient(), parallel.Gradient());
    for (size_t j = 0; j < point.size(); ++j)
        ASSERT_EQ(single.HessianProducts()(0, j), parallel.HessianProducts()(0, j));
    struct FailReplaySubmission_ final : Script::Detail::SimulationObserver_ {
        size_t calls_ = 0;
        void AfterSubmission() override {
            if (++calls_ == 3)
                THROW("injected replay submission failure");
        }
    } observer;
    {
        const Script::Detail::ScopedSimulationObserver_ scope(&observer);
        ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 8193, SpotDirection()), Dal::Exception_);
    }
    const auto recovered = Script::EvaluateBlackScholesLsmcCurvature(prepared, point, 8193, SpotDirection());
    ASSERT_EQ(recovered.Value(), parallel.Value());
    ASSERT_EQ(recovered.Gradient(), parallel.Gradient());
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRetrainedOneSidedVolatilityAndSignedDirections) {
    const auto prepared = Preparation(true, "RetrainedBump");
    auto request = SpotDirection();
    request.directions_ = Dal::Matrix_<>(2, 4, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(1, 0) = -1.0;
    request.steps_ = {0.1, 0.1};
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.0, 0.05, 0.0}, 128, request);
    const auto gradient = [&](double spot) {
        const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", spot, 0.0, 0.05, 0.0));
        return Script::MCLsmcAadSimulation(*prepared, data, 128).risks_;
    };
    const auto plus = gradient(100.1), minus = gradient(99.9);
    for (size_t j = 0; j < result.Gradient().size(); ++j) {
        ASSERT_NEAR(result.HessianProducts()(0, j), (plus[j] - minus[j]) / 0.2, 1e-9);
        ASSERT_EQ(result.HessianProducts()(0, j), -result.HessianProducts()(1, j));
    }
    ASSERT_EQ(result.Execution().method_, "BumpOverRetrainedNativeLsmcPolicySecant");
    ASSERT_EQ(result.Execution().gradientEvaluations_, 5);
}

TEST(ScriptExerciseLSMCTest, TestCurvatureSelectedEstimatorStudy) {
    struct Case_ {
        const char* mode_;
        size_t paths_;
        double outer_;
        double inner_;
        int replicates_;
    };
    const Case_ cases[] = {{"Frozen", 128, 0.2, 1e-3, 1}, {"Frozen", 128, 0.1, 1e-3, 1},        {"Frozen", 128, 0.05, 1e-3, 1},
                           {"Frozen", 512, 0.1, 1e-3, 1}, {"RetrainedBump", 128, 0.1, 1e-3, 1}, {"RetrainedBump", 128, 0.1, 5e-4, 1},
                           {"Frozen", 512, 0.1, 1e-3, 2}, {"Frozen", 512, 0.1, 1e-3, 4}};
    size_t ordinal = 0;
    for (const auto& c : cases) {
        Script::MonteCarloSettings_ settings;
        settings.lsmcPolicyBumpRelative_ = c.inner_;
        if (c.replicates_ > 1) {
            settings.lsmcRqmcReplicates_ = c.replicates_;
            settings.lsmcTrainingSeed_ = 17;
            settings.lsmcPricingSeed_ = 29;
        }
        const auto result = Script::EvaluateBlackScholesLsmcCurvature(Preparation(true, c.mode_, settings), {100.0, 0.2, 0.05, 0.0}, c.paths_,
                                                                      SpotDirection(4, c.outer_));
        ASSERT_TRUE(std::isfinite(result.HessianProducts()(0, 0)));
        if (std::string(c.mode_) == "Frozen" && c.replicates_ == 1) {
            ASSERT_NEAR(result.HessianProducts()(0, 0), FrozenGamma(result, 1e-3), 2e-5);
        }
        std::ostringstream row;
        row << std::setprecision(17) << c.mode_ << ',' << c.paths_ << ',' << c.outer_ << ',' << c.inner_ << ',' << c.replicates_ << ','
            << result.HessianProducts()(0, 0);
        RecordProperty("estimator_" + std::to_string(ordinal++), row.str());
    }
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRetrainedIndependentNestedPassivePrices) {
    AAD::BumpOverAADRequest_ empty;
    empty.directions_ = Dal::Matrix_<>(0, 4);
    const auto frozen = Preparation();
    const auto train = [&](const Vector_<>& point) { return Script::EvaluateBlackScholesLsmcCurvature(frozen, point, 128, empty); };
    const auto passiveGradient = [&](const Vector_<>& point) {
        const auto basePolicy = train(point);
        Vector_<> gradient(point.size());
        for (size_t j = 0; j < point.size(); ++j) {
            const double partialStep = j == 0 ? 1e-3 : 1e-6;
            auto plus = point, minus = point;
            plus[j] += partialStep;
            minus[j] -= partialStep;
            const double partial = (FrozenPrice(basePolicy, plus) - FrozenPrice(basePolicy, minus)) / (2.0 * partialStep);
            const double policyStep = 1e-3 * std::max(1.0, std::abs(point[j]));
            plus = point;
            minus = point;
            plus[j] += policyStep;
            minus[j] -= policyStep;
            const auto plusPolicy = train(plus), minusPolicy = train(minus);
            gradient[j] = partial + (FrozenPrice(plusPolicy, point) - FrozenPrice(minusPolicy, point)) / (2.0 * policyStep);
        }
        return gradient;
    };
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(Preparation(true, "RetrainedBump"), {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection());
    auto plus = result.Point(), minus = result.Point();
    plus[0] += 0.1;
    minus[0] -= 0.1;
    const auto baseGradient = passiveGradient(result.Point()), plusGradient = passiveGradient(plus), minusGradient = passiveGradient(minus);
    for (size_t j = 0; j < result.Point().size(); ++j) {
        SCOPED_TRACE(j);
        ASSERT_NEAR(result.Gradient()[j], baseGradient[j], 2e-6);
        ASSERT_NEAR(result.HessianProducts()(0, j), (plusGradient[j] - minusGradient[j]) / 0.2, 2e-5);
    }
}

TEST(ScriptExerciseLSMCTest, TestCurvatureWorkerAdjointModeRestoredAfterCapacityFailure) {
    PoolRestore_ restore;
    restore.pool_->Start(2, true);
    auto setMode = restore.pool_->SpawnTask([] {
        AAD::Tape()->multi_ = true;
        AAD::Tape()->numAdj_ = 4;
        return true;
    });
    ASSERT_TRUE(setMode.get());
    const auto prepared = Preparation();
    auto request = SpotDirection();
    request.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 8193, request), Dal::Exception_);
    request.recordingCapacityBudgetBytes_.reset();
    const auto result = Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 8193, request);
    auto checkMode = restore.pool_->SpawnTask([] { return AAD::Tape()->multi_ && AAD::Tape()->numAdj_ == 4; });
    ASSERT_TRUE(checkMode.get());
    ASSERT_TRUE(std::isfinite(result.HessianProducts()(0, 0)));
}

TEST(ScriptExerciseLSMCTest, TestCurvatureRejectsForeignModelObservationPlan) {
    class ForeignObservationModel_ final : public AAD::BlackScholes_<> {
    public:
        ForeignObservationModel_() : BlackScholes_(100.0, 0.2, 0.05, 0.0) {}
        [[nodiscard]] bool SupportsIndex(const Dal::Index_&) const override { return true; }
    } model;
    const Script::ScriptProductData_ product("", {Cell_(Date_(2027, 4, 10)), Cell_(Date_(2027, 10, 10))},
                                             {"EXERCISE 100 - FIX(FX[EUR/USD])", "EXERCISE 100 - FIX(FX[EUR/USD])"});
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.lsmcTrainingPaths_ = 128;
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 10);
    const auto prepared = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
    Script::TestSupport::SubmissionCounter_ observer;
    const Script::Detail::ScopedSimulationObserver_ scope(&observer);
    ASSERT_THROW((void)Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, SpotDirection()), Dal::Exception_);
    ASSERT_EQ(observer.submissions_, 0);
}
