//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal-public/src/montecarlocurvature.hpp>
#include <dal-public/src/script.hpp>

#include <script_test_observers.hpp>

namespace {
    auto Valuation() {
        Dal::Script::ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Dal::Date_(2026, 1, 2);
        return settings;
    }

    auto TodayPlan() {
        return Dal::PlanBlackScholesMonteCarlo(Dal::NewScriptProduct("today", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2026, 1, 2))},
                                                                     {"2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PUBLIC]) ^ 2"}),
                                               Valuation());
    }

    auto Bumps() {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, 5, 0.0);
        request.directions_(0, 0) = 1.0;
        request.directions_(0, 4) = 0.5;
        request.steps_ = {0.01};
        return request;
    }

    struct MutateInputs_ : Dal::Script::Detail::SimulationObserver_ {
        Dal::BlackScholesMonteCarloPlan_* plan_;
        Dal::Vector_<>* point_;
        Dal::Script::SegmentedMonteCarloSettings_* settings_;
        Dal::BlackScholesMonteCarloPlan_ replacement_ = TodayPlan();
        MutateInputs_(Dal::BlackScholesMonteCarloPlan_* plan, Dal::Vector_<>* point, Dal::Script::SegmentedMonteCarloSettings_* settings)
            : plan_(plan), point_(point), settings_(settings) {}
        void AfterSubmission() override {
            *plan_ = replacement_;
            *point_ = {1.0};
            settings_->rsg_ = "changed";
            settings_->firstPath_ = 999;
        }
    };
} // namespace

TEST(BlackScholesMonteCarloBoundaryTest, TestOwningPreparationAndExplicitPoint) {
    const auto product = Dal::NewScriptProduct("segmented", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2027, 1, 2))},
                                               {"2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PUBLIC])"});
    Dal::Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Dal::Date_(2026, 1, 2);
    const auto plan = Dal::PlanBlackScholesMonteCarlo(product, valuation);
    const Dal::Vector_<> point{100.0, 0.0, 0.03, 0.01, 2.0};
    const auto result = Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, point, 1);
    ASSERT_NEAR(result.Mean().MeanValue(), 200.0 * std::exp(-0.01), 1e-10);
    ASSERT_EQ(result.Point(), point);
    ASSERT_EQ(result.Mean().ParameterLabels(), (Dal::Vector_<Dal::String_>{"spot", "vol", "rate", "div", "SCALE"}));
    ASSERT_EQ(result.Plan().Contract().EventTexts(), product->EventTexts());
    ASSERT_EQ(result.Plan().Valuation().evaluationDate_, valuation.evaluationDate_);
}

TEST(BlackScholesMonteCarloBoundaryTest, TestTimeZeroMixedCurvatureAndMetadata) {
    const auto plan = TodayPlan();
    const Dal::Vector_<> point{10.0, 0.2, 0.03, 0.01, 2.0};
    Dal::Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = std::numeric_limits<size_t>::max();
    settings.path_.recordingCapacityBudgetBytes_ = size_t(1) << 29;
    auto request = Bumps();
    request.recordingCapacityBudgetBytes_ = size_t(1) << 28;
    const auto result = Dal::ValueByBlackScholesMonteCarloWithCurvature(plan, point, 1, request, settings);
    const auto& curvature = result.Curvature();
    ASSERT_NEAR(curvature.Base().MeanValue(), 200.0, 1e-10);
    ASSERT_NEAR(curvature.Base().MeanGradient()[0], 40.0, 1e-10);
    ASSERT_NEAR(curvature.Base().MeanGradient()[4], 100.0, 1e-10);
    ASSERT_NEAR(curvature.HessianProducts()(0, 0), 14.0, 1e-9);
    ASSERT_NEAR(curvature.HessianProducts()(0, 4), 20.0, 1e-9);
    ASSERT_EQ(curvature.Execution().numericPayloadBytes_, 176);
    ASSERT_EQ(curvature.Execution().gradientEvaluations_, 3);
    ASSERT_EQ(curvature.Execution().recordingCapacityBudgetBytes_, request.recordingCapacityBudgetBytes_);
    ASSERT_EQ(curvature.Settings().path_.recordingCapacityBudgetBytes_, settings.path_.recordingCapacityBudgetBytes_);
    ASSERT_EQ(curvature.Base().Execution().firstPath_, settings.firstPath_);
    ASSERT_EQ(result.Plan().Contract().EventTexts(), plan.Contract().EventTexts());
}

TEST(BlackScholesMonteCarloBoundaryTest, TestHistoricalConstantReplayWithoutHistoryReads) {
    auto valuation = Valuation();
    valuation.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(
        new Dal::MarketFixingSnapshot_({{"EQ[SEGMENTED_PUBLIC_HISTORY]", {{Dal::DateTime_(Dal::Date_(2025, 1, 2), 0.0), 80.0}}}}));
    const auto plan =
        Dal::PlanBlackScholesMonteCarlo(Dal::NewScriptProduct("history", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2025, 1, 2))},
                                                              {"2", "APPEND(v, SCALE * FIX(EQ[SEGMENTED_PUBLIC_HISTORY])) pay PAYS 0 pay = SUM(v)"}),
                                        valuation);
    Dal::Script::TestSupport::RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observer(&reads);
    const auto result = Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, {100, 0.2, 0.03, 0.01, -3}, 33);
    ASSERT_EQ(result.Mean().MeanValue(), -240);
    ASSERT_EQ(result.Mean().MeanGradient(), (Dal::Vector_<>{0, 0, 0, 0, 80}));
    ASSERT_EQ(result.Plan().ScriptConstants(), (Dal::Vector_<>{2}));
    ASSERT_EQ(result.Plan().Observations().size(), 1);
    ASSERT_EQ(result.Plan().Observations()[0].value_, 80.0);
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
}

TEST(BlackScholesMonteCarloBoundaryTest, TestInvalidRequestsAdmittedBeforeWorkersAndRecover) {
    const auto plan = TodayPlan();
    const Dal::Vector_<> point{10, 0.2, 0.03, 0.01, 2};
    {
        Dal::Script::TestSupport::RejectSubmissions_ submissions;
        const Dal::Script::Detail::ScopedSimulationObserver_ observer(&submissions);
        ASSERT_THROW((void)Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, point, 0), Dal::Exception_);
        ASSERT_THROW((void)Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, {}, 1), Dal::Exception_);
        auto request = Bumps();
        request.numericPayloadBudgetBytes_ = 175;
        ASSERT_THROW((void)Dal::ValueByBlackScholesMonteCarloWithCurvature(plan, point, 1, request), Dal::Exception_);
        request.numericPayloadBudgetBytes_.reset();
        request.directions_(0, 1) = 1.0;
        request.steps_ = {0.3};
        ASSERT_THROW((void)Dal::ValueByBlackScholesMonteCarloWithCurvature(plan, point, 1, request), Dal::Exception_);
        ASSERT_EQ(submissions.calls_, 0);
    }
    ASSERT_NEAR(Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, point, 1).Mean().MeanValue(), 200, 1e-10);
}

TEST(BlackScholesMonteCarloBoundaryTest, TestEmptyDirectionsExactPayloadAndClosedPreparation) {
    const auto plan = TodayPlan();
    Dal::AAD::BumpOverAADRequest_ request;
    request.directions_ = Dal::Matrix_<>(0, 5);
    request.numericPayloadBudgetBytes_ = 88;
    const auto result = Dal::ValueByBlackScholesMonteCarloWithCurvature(plan, {10, 0.2, 0.03, 0.01, 2}, 1, request);
    ASSERT_EQ(result.Curvature().HessianProducts().Rows(), 0);
    ASSERT_EQ(result.Curvature().HessianProducts().Cols(), 5);
    ASSERT_EQ(result.Curvature().Execution().gradientEvaluations_, 1);
    ASSERT_EQ(result.Curvature().Base().MeanGradient().size(), 5);
    ASSERT_THROW((void)Dal::PlanBlackScholesMonteCarlo({}, Valuation()), Dal::Exception_);
    ASSERT_THROW((void)Dal::PlanBlackScholesMonteCarlo(
                     Dal::NewScriptProduct("exercise", {Dal::Cell_(Dal::Date_(2027, 1, 2))}, {"EXERCISE MAX(100 - FIX(EQ[SEGMENTED_PUBLIC]), 0)"}),
                     Valuation()),
                 Dal::Exception_);
}

TEST(BlackScholesMonteCarloBoundaryTest, TestInputMutationAfterSubmissionCannotChangeStoredEstimator) {
    auto plan = Dal::PlanBlackScholesMonteCarlo(Dal::NewScriptProduct("original", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2027, 1, 2))},
                                                                      {"2", "pay PAYS SCALE * FIX(EQ[SEGMENTED_PUBLIC])"}),
                                                Valuation());
    Dal::Vector_<> point{100, 0, 0.03, 0.01, 2};
    const auto originalPoint = point;
    Dal::Script::SegmentedMonteCarloSettings_ settings;
    settings.firstPath_ = 7;
    MutateInputs_ mutate(&plan, &point, &settings);
    const Dal::Script::Detail::ScopedSimulationObserver_ observer(&mutate);
    const auto result = Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, point, 33, settings);
    ASSERT_NEAR(result.Mean().MeanValue(), 200 * std::exp(-0.01), 1e-10);
    ASSERT_EQ(result.Point(), originalPoint);
    ASSERT_EQ(result.Settings().firstPath_, 7);
    ASSERT_EQ(result.Settings().rsg_, Dal::String_("sobol"));
    ASSERT_EQ(result.Plan().Contract().Name(), Dal::String_("original"));
    ASSERT_EQ(plan.Contract().Name(), Dal::String_("today"));
    ASSERT_EQ(point.size(), 1);
}
