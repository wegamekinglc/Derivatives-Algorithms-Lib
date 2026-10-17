//
// Created by Codex on 2026/10/9.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <limits>
#include <memory>
#include <type_traits>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/segmentedpath.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    std::shared_ptr<const Script::BlackScholesSegmentedPreparation_> PreparePath(const Script::ScriptProductData_& product,
                                                                                 const Dal::Handle_<Dal::MarketFixingSnapshot_>& history = {}) {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 10, 1);
        return std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
            Script::PrepareBlackScholesSegmentedScript(product, valuation, history));
    }

    struct PathRisk_ {
        double value_;
        Vector_<> gradient_;
        size_t tapeBytes_;
    };

    PathRisk_ FullPathRisk(const Script::PreparedScript_& prepared, const Vector_<>& values, const Vector_<>& gaussian) {
        const auto mode = AAD::SetNumResultsForAAD(false, 1);
        AAD::RecordingScope_ recording;
        Vector_<AAD::Number_> parameters(values.size());
        for (size_t i = 0; i < values.size(); ++i)
            recording.RegisterInput(parameters[i], values[i]);
        recording.StartRecording();
        AAD::BlackScholes_<AAD::Number_> model(parameters[0], parameters[1], parameters[2], parameters[3]);
        auto state = prepared.BuildEvalState<AAD::Number_>();
        for (size_t i = 0; i < state.constVariables_.size(); ++i)
            state.constVariables_[i] = parameters[4 + i];
        prepared.InitializeHistoricalState(&state);
        AAD::Scenario_<AAD::Number_> scenario;
        if (!prepared.TimeLine().empty()) {
            model.Allocate(prepared.TimeLine(), prepared.DefLine());
            model.Init(prepared.TimeLine(), prepared.DefLine());
            AAD::AllocatePath(prepared.DefLine(), scenario);
            AAD::InitializePath(scenario);
            model.GeneratePath(gaussian, &scenario);
        }
        prepared.CompiledProgram(true).Evaluate(scenario, state);
        AAD::Number_ root = state.VarVals()[prepared.PayOffIdx()];
        recording.FinishRecording();
        AAD::NativeOperations_::AddSeed(root, 1.0);
        recording.Reverse();
        PathRisk_ result{AAD::Value(root), Vector_<>(values.size()), AAD::MeasureTape(*AAD::Tape()).capacityBytes_};
        for (size_t i = 0; i < values.size(); ++i)
            result.gradient_[i] = AAD::Adjoint(parameters[i]);
        recording.Close();
        return result;
    }

    Dal::Handle_<Dal::MarketFixingSnapshot_> PathHistory() {
        return Dal::Handle_<Dal::MarketFixingSnapshot_>(
            new Dal::MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{Dal::DateTime_(Date_(2026, 9, 30), 0.0), 80.0}}}}));
    }

    void AssertRisks(const AAD::SegmentedPathResult_& result, const PathRisk_& full) {
        ASSERT_NEAR(result.Value(), full.value_, 1e-10);
        ASSERT_EQ(result.Gradient().size(), full.gradient_.size());
        for (size_t i = 0; i < full.gradient_.size(); ++i)
            ASSERT_NEAR(result.Gradient()[i], full.gradient_[i], 1e-9);
    }
} // namespace

TEST(BlackScholesSegmentedPathTest, TestCompletePaymentAndAllParameterRisksAgainstAnalyticAndFullPath) {
    const auto prepared =
        PreparePath(Script::ScriptProductData_("", {Cell_("SCALE"), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 11, 3)), Cell_(Date_(2027, 1, 7))},
                                               {"2", "x = FIX(EQ[DAL196_TEST])", "x = x", "pay PAYS SCALE * FIX(EQ[DAL196_TEST]) ON 2027-04-01"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> gaussian{0.3, -0.6, 0.9};
    ASSERT_EQ(kernel.SimDim(), gaussian.size());
    const AAD::BlackScholes_<> labels(100.0, 0.2);
    auto expectedLabels = labels.ParameterLabels();
    expectedLabels.push_back("SCALE");
    ASSERT_EQ(kernel.ParameterLabels(), expectedLabels);
    for (const Vector_<>& parameters :
         Vector_<Vector_<>>{{100.0, 0.2, 0.03, 0.01, 2.0}, {120.0, 0.0, -0.02, 0.03, 3.0}, {85.0, 0.35, 0.08, -0.01, -1.5}}) {
        const double time = prepared->Prepared().TimeLine().back();
        const double maturity = static_cast<double>(Date_(2027, 4, 1) - prepared->Prepared().EvaluationDate()) / 365.0;
        double brownian = 0.0;
        for (size_t i = 0; i < gaussian.size(); ++i) {
            const double previous = i == 0 ? 0.0 : prepared->Prepared().TimeLine()[i - 1];
            brownian += std::sqrt(prepared->Prepared().TimeLine()[i] - previous) * gaussian[i];
        }
        const double unit = parameters[0] * std::exp(-parameters[3] * time - 0.5 * parameters[1] * parameters[1] * time + parameters[1] * brownian +
                                                     parameters[2] * (time - maturity));
        const double value = parameters[4] * unit;
        const Vector_<> expected{value / parameters[0], value * (brownian - parameters[1] * time), value * (time - maturity), -time * value, unit};
        const auto full = FullPathRisk(prepared->Prepared(), parameters, gaussian);
        ASSERT_NEAR(full.value_, value, 1e-10);
        for (size_t length : {1, 2, 8}) {
            AAD::SegmentedPathSettings_ settings;
            settings.segmentSteps_ = length;
            const auto result = kernel.Evaluate(parameters, gaussian, settings);
            ASSERT_NEAR(result.Value(), value, 1e-10);
            ASSERT_NEAR(result.Value(), full.value_, 1e-10);
            ASSERT_EQ(result.Gradient().size(), expected.size());
            for (size_t i = 0; i < expected.size(); ++i) {
                ASSERT_NEAR(result.Gradient()[i], expected[i], 1e-9);
                ASSERT_NEAR(result.Gradient()[i], full.gradient_[i], 1e-9);
            }
            ASSERT_EQ(result.Execution().passiveSteps_, prepared->Prepared().TimeLine().size());
            ASSERT_EQ(result.Execution().recomputedSteps_, prepared->Prepared().TimeLine().size());
        }
    }
}

TEST(BlackScholesSegmentedPathTest, TestHistoricalVectorOldFixingAndLaterPaymentReadCrossBoundaries) {
    const auto prepared = PreparePath(
        Script::ScriptProductData_(
            "", {Cell_("SCALE"), Cell_(Date_(2026, 9, 30)), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4)), Cell_(Date_(2026, 10, 6))},
            {"2", "x = SCALE * FIX(EQ[DAL196_TEST]) APPEND(v, x)", "APPEND(v, FIX(EQ[DAL196_TEST])) z = 0 pay PAYS x ON 2026-12-01",
             "z = pay APPEND(v, FIX(EQ[DAL196_TEST], 2026-10-02))", "pay PAYS SUM(v) + z + FIX(EQ[DAL196_TEST], 2026-10-02)"}),
        PathHistory());
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> gaussian{0.3, -0.6, 0.9};
    for (const Vector_<>& parameters : Vector_<Vector_<>>{{100.0, 0.2, 0.03, 0.01, 2.0}, {90.0, 0.4, -0.05, 0.02, 3.0}}) {
        const double firstTime = prepared->Prepared().TimeLine().front();
        const double finalTime = prepared->Prepared().TimeLine().back();
        const double maturity = static_cast<double>(Date_(2026, 12, 1) - prepared->Prepared().EvaluationDate()) / 365.0;
        const double fixing = parameters[0] * std::exp((parameters[2] - parameters[3] - 0.5 * parameters[1] * parameters[1]) * firstTime +
                                                       parameters[1] * std::sqrt(firstTime) * gaussian[0]);
        const double historical = 80.0 * parameters[4];
        const double firstPayment = historical * std::exp(-parameters[2] * maturity);
        const double discount = std::exp(-parameters[2] * finalTime);
        const double expected = firstPayment + discount * (historical + 3.0 * fixing + firstPayment);
        const auto full = FullPathRisk(prepared->Prepared(), parameters, gaussian);
        ASSERT_NEAR(full.value_, expected, 1e-10);
        for (size_t length : {1, 2, 8}) {
            AAD::SegmentedPathSettings_ settings;
            settings.segmentSteps_ = length;
            const auto result = kernel.Evaluate(parameters, gaussian, settings);
            AssertRisks(result, full);
            ASSERT_NEAR(result.Gradient()[4], 80.0 * (std::exp(-parameters[2] * maturity) * (1.0 + discount) + discount), 1e-9);
        }
    }
}

TEST(BlackScholesSegmentedPathTest, TestFuzzyVectorBranchesTiesAndFreshConstantPoints) {
    const auto prepared = PreparePath(
        Script::ScriptProductData_("", {Cell_("K"), Cell_("SCALE"), Cell_(Date_(2026, 10, 1)), Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
                                   {"100", "2", "APPEND(v, FIX(EQ[DAL196_TEST]))",
                                    "IF FIX(EQ[DAL196_TEST], 2026-10-01) = K:2 THEN APPEND(v, SCALE * FIX(EQ[DAL196_TEST])) ELSE v[3] = K END",
                                    "pay PAYS MAX(v) + MIN(v) + AVERAGE(v) + SUM(v)"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> gaussian{0.0, -0.4};
    ASSERT_EQ(kernel.SimDim(), gaussian.size());
    ASSERT_EQ(kernel.ParameterLabels()[4], "K");
    for (double strike : {98.0, 99.5, 100.0, 100.5, 102.0}) {
        const Vector_<> parameters{100.0, 0.0, 0.0, 0.0, strike, 2.0};
        const auto full = FullPathRisk(prepared->Prepared(), parameters, gaussian);
        AAD::SegmentedPathSettings_ settings;
        settings.segmentSteps_ = 1;
        const auto result = kernel.Evaluate(parameters, gaussian, settings);
        AssertRisks(result, full);
        if (strike == 99.5 || strike == 100.5) {
            auto differentiable = parameters;
            differentiable[5] = 1.7;
            auto minus = differentiable;
            auto plus = differentiable;
            minus[4] -= 1e-5;
            plus[4] += 1e-5;
            const double derivative =
                (FullPathRisk(prepared->Prepared(), plus, gaussian).value_ - FullPathRisk(prepared->Prepared(), minus, gaussian).value_) / 2e-5;
            const auto risk = kernel.Evaluate(differentiable, gaussian, settings);
            AssertRisks(risk, FullPathRisk(prepared->Prepared(), differentiable, gaussian));
            ASSERT_NEAR(risk.Gradient()[4], derivative, 1e-7);
        }
    }
}

TEST(BlackScholesSegmentedPathTest, TestTimeZeroUsesExactSpotAndNoDraw) {
    const auto prepared = PreparePath(Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 1))}, {"pay PAYS FIX(EQ[DAL196_TEST])"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    ASSERT_EQ(kernel.SimDim(), 0);
    ASSERT_EQ(kernel.Dimensions().steps_, 1);
    for (double spot : {1e-200, 1e200}) {
        const auto result = kernel.Evaluate({spot, 0.2, 0.03, 0.01}, {});
        ASSERT_DOUBLE_EQ(result.Value(), spot);
        ASSERT_DOUBLE_EQ(result.Gradient()[0], 1.0);
        for (size_t i = 1; i < 4; ++i)
            ASSERT_DOUBLE_EQ(result.Gradient()[i], 0.0);
    }
}

TEST(BlackScholesSegmentedPathTest, TestHistoricalOnlyReturnsFreshConstantRiskWithoutModelSteps) {
    const auto prepared = PreparePath(Script::ScriptProductData_("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 30))},
                                                                 {"2", "APPEND(v, SCALE * FIX(EQ[DAL196_TEST])) pay PAYS 0 pay = SUM(v)"}),
                                      PathHistory());
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    ASSERT_EQ(kernel.Dimensions().steps_, 0);
    ASSERT_EQ(kernel.SimDim(), 0);
    for (double scale : {2.0, -3.0}) {
        const Vector_<> parameters{100.0, 0.2, 0.03, 0.01, scale};
        const auto result = kernel.Evaluate(parameters, {});
        AssertRisks(result, FullPathRisk(prepared->Prepared(), parameters, {}));
        ASSERT_DOUBLE_EQ(result.Value(), 80.0 * scale);
        ASSERT_DOUBLE_EQ(result.Gradient()[4], 80.0);
        for (size_t i = 0; i < 4; ++i)
            ASSERT_DOUBLE_EQ(result.Gradient()[i], 0.0);
        ASSERT_EQ(result.Execution().segments_, 0);
        ASSERT_EQ(result.Execution().reverseSweeps_, 2);
    }
}

TEST(BlackScholesSegmentedPathTest, TestInvalidRequestsBudgetsAndModeRestoreRecover) {
    const auto prepared = PreparePath(Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 2))}, {"pay PAYS FIX(EQ[DAL196_TEST])"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> parameters{100.0, 0.2, 0.03, 0.01};
    const auto expected = kernel.Evaluate(parameters, {0.3});
    const auto mode = AAD::SetNumResultsForAAD(true, 4);
    ASSERT_THROW((void)Script::BlackScholesSegmentedPath_(nullptr), Dal::Exception_);
    ASSERT_THROW((void)kernel.Evaluate({}, {0.3}), Dal::Exception_);
    ASSERT_THROW((void)kernel.Evaluate(parameters, {}), Dal::Exception_);
    ASSERT_THROW((void)kernel.Evaluate(parameters, {0.3, 0.4}), Dal::Exception_);
    for (double value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid = parameters;
        invalid[2] = value;
        ASSERT_THROW((void)kernel.Evaluate(invalid, {0.3}), Dal::Exception_);
        ASSERT_THROW((void)kernel.Evaluate(parameters, {value}), Dal::Exception_);
    }
    ASSERT_THROW((void)kernel.Evaluate({0.0, 0.2, 0.03, 0.01}, {0.3}), Dal::Exception_);
    ASSERT_THROW((void)kernel.Evaluate({100.0, -0.2, 0.03, 0.01}, {0.3}), Dal::Exception_);
    AAD::SegmentedPathSettings_ settings;
    settings.segmentSteps_ = 0;
    ASSERT_THROW((void)kernel.Evaluate(parameters, {0.3}, settings), Dal::Exception_);
    settings.segmentSteps_ = 1;
    settings.checkpointCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)kernel.Evaluate(parameters, {0.3}, settings), Dal::Exception_);
    settings.checkpointCapacityBudgetBytes_.reset();
    settings.recordingCapacityBudgetBytes_ = 0;
    ASSERT_THROW((void)kernel.Evaluate(parameters, {0.3}, settings), Dal::Exception_);
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    settings.recordingCapacityBudgetBytes_.reset();
    const auto recovered = kernel.Evaluate(parameters, {0.3}, settings);
    ASSERT_DOUBLE_EQ(recovered.Value(), expected.Value());
    ASSERT_EQ(recovered.Gradient(), expected.Gradient());
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 4);
    AAD::RecordingScope_ outer;
    AAD::Number_ input;
    outer.RegisterInput(input, 3.0);
    outer.StartRecording();
    AAD::Number_ root = input * input;
    ASSERT_THROW((void)kernel.Evaluate(parameters, {0.3}), Dal::Exception_);
    outer.FinishRecording();
    AAD::NativeOperations_::SetSeed(root, 1.0, 2);
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 6.0);
    outer.Close();
}

TEST(BlackScholesSegmentedPathTest, TestPreparationAdmissionAndHistoricalFixingFailure) {
    const Script::ScriptProductData_ product("", {Cell_(Date_(2026, 10, 2))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    AAD::BlackScholes_<> model(100.0, 0.2);
    Script::MonteCarloSettings_ simulation;
    const auto plain = std::make_shared<const Script::PreparedScript_>(Script::PrepareScript(product, &model, valuation, simulation));
    ASSERT_FALSE(plain->Simulation().enableAad_);
    static_assert(!std::is_constructible_v<Script::BlackScholesSegmentedPath_, decltype(plain)>);
    ASSERT_THROW((void)Script::PrepareBlackScholesSegmentedScript(product, valuation, {}, {}, 0.0), Dal::Exception_);
    const Script::ScriptProductData_ historical("", {Cell_(Date_(2026, 9, 30))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    const Dal::Handle_<Dal::MarketFixingSnapshot_> empty(new Dal::MarketFixingSnapshot_({}));
    ASSERT_THROW((void)Script::PrepareBlackScholesSegmentedScript(historical, valuation, empty), Dal::Exception_);
    const auto accepted = Script::PrepareBlackScholesSegmentedScript(historical, valuation, PathHistory());
    ASSERT_TRUE(accepted.Prepared().AllExpired());
    ASSERT_EQ(accepted.Prepared().Plan().KnownValues(), (Vector_<>{80.0}));
    const Script::ScriptProductData_ exercise("", {Cell_(Date_(2026, 10, 2))}, {"EXERCISE MAX(100 - FIX(EQ[DAL196_TEST]), 0)"});
    const auto unsupported =
        std::make_shared<const Script::BlackScholesSegmentedPreparation_>(Script::PrepareBlackScholesSegmentedScript(exercise, valuation));
    ASSERT_THROW((void)Script::BlackScholesSegmentedPath_(unsupported), Dal::Exception_);
}

TEST(BlackScholesSegmentedPathTest, TestGenericSingleObservationMultiAssetPreparationCannotEnterKernel) {
    Dal::Matrix_<> correlations(2, 2, 0.25);
    correlations(0, 0) = correlations(1, 1) = 1.0;
    AAD::CorrelatedBlackScholes_<> model({"EQ[DAL196_TEST]", "EQ[OTHER]"}, {100.0, 90.0}, {0.2, 0.3}, {0.01, 0.02}, 0.03, correlations);
    const Script::ScriptProductData_ product("", {Cell_(Date_(2026, 10, 2))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 10, 1);
    Script::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    const auto untrusted = Script::PrepareScript(product, &model, valuation, simulation);
    ASSERT_EQ(untrusted.DefLine().front().indexNames_.size(), 1);
    ASSERT_NO_THROW((void)AAD::BlackScholesStepPlan_(untrusted.TimeLine(), untrusted.DefLine()));
    static_assert(!std::is_constructible_v<Script::BlackScholesSegmentedPath_, std::shared_ptr<const Script::PreparedScript_>>,
                  "generic preparation must not enter the Black-Scholes kernel");
    static_assert(!std::is_constructible_v<Script::BlackScholesSegmentedPreparation_, Script::PreparedScript_>);
    static_assert(!std::is_assignable_v<Script::BlackScholesSegmentedPreparation_&, Script::PreparedScript_>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Script::BlackScholesSegmentedPreparation_&>().Prepared()), const Script::PreparedScript_&>);
}

TEST(BlackScholesSegmentedPathTest, TestConcurrentRequestsShareOnlyImmutablePlansAndOwnResults) {
    auto prepared = PreparePath(Script::ScriptProductData_("", {Cell_(Date_(2026, 10, 2)), Cell_(Date_(2026, 10, 4))},
                                                           {"x = FIX(EQ[DAL196_TEST])", "pay PAYS x + FIX(EQ[DAL196_TEST])"}));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> first{100.0, 0.2, 0.03, 0.01};
    const Vector_<> second{90.0, 0.35, -0.02, 0.03};
    const Vector_<> gaussian{0.3, -0.6};
    const auto expectedFirst = FullPathRisk(prepared->Prepared(), first, gaussian);
    const auto expectedSecond = FullPathRisk(prepared->Prepared(), second, gaussian);
    prepared.reset();
    auto a = std::async(std::launch::async, [&] { return kernel.Evaluate(first, gaussian); });
    auto b = std::async(std::launch::async, [&] { return kernel.Evaluate(second, gaussian); });
    const auto firstResult = a.get();
    const auto secondResult = b.get();
    AAD::Clear(*AAD::Tape());
    AssertRisks(firstResult, expectedFirst);
    AssertRisks(secondResult, expectedSecond);
}

TEST(BlackScholesSegmentedPathTest, TestLongRunningObservationUsesBoundedSegmentTape) {
    Vector_<Cell_> dates;
    Vector_<Dal::String_> events;
    const size_t steps = 2048;
    for (size_t i = 0; i < steps; ++i) {
        dates.emplace_back(Date_(2026, 10, 2).AddDays(static_cast<int>(i)));
        events.emplace_back(i + 1 == steps ? "running = running + FIX(EQ[DAL196_TEST]) pay PAYS running"
                                           : "running = running + FIX(EQ[DAL196_TEST])");
    }
    const auto prepared = PreparePath(Script::ScriptProductData_("", dates, events));
    const Script::BlackScholesSegmentedPath_ kernel(prepared);
    const Vector_<> parameters{100.0, 0.2, 0.03, 0.01};
    const Vector_<> gaussian(steps, 0.01);
    AAD::Clear(*AAD::Tape());
    const auto full = FullPathRisk(prepared->Prepared(), parameters, gaussian);
    AAD::Clear(*AAD::Tape());
    AAD::SegmentedPathSettings_ settings;
    settings.segmentSteps_ = 64;
    const auto result = kernel.Evaluate(parameters, gaussian, settings);
    ASSERT_NEAR(result.Value(), full.value_, 1e-7);
    for (size_t i = 0; i < parameters.size(); ++i)
        ASSERT_NEAR(result.Gradient()[i], full.gradient_[i], 1e-6);
    ASSERT_LT(result.Execution().peakTapeBytes_ + result.Execution().checkpointBytes_, full.tapeBytes_);
    ASSERT_EQ(result.Execution().recomputedSteps_, steps);
    ASSERT_EQ(result.Execution().segments_, 32);
}
