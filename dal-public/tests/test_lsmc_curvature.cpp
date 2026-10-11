//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <dal/platform/initall.hpp>

#include <dal-public/src/lsmccurvature.hpp>
#include <dal-public/src/script.hpp>

#include <script_test_observers.hpp>

namespace {
    auto Valuation() {
        Dal::Script::ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Dal::Date_(2026, 10, 10);
        return settings;
    }

    auto Plan(const Dal::String_& name = "original") {
        auto simulation = Dal::DefaultRiskMonteCarloSettings();
        simulation.compiled_ = true;
        simulation.smooth_ = 2.0;
        simulation.lsmcTrainingPaths_ = 64;
        return Dal::PlanBlackScholesLsmc(
            Dal::NewScriptProduct(name, {Dal::Cell_("K"), Dal::Cell_(Dal::Date_(2027, 4, 10)), Dal::Cell_(Dal::Date_(2027, 10, 10))},
                                  {"45", "EXERCISE 2 * K - spot()", "EXERCISE 2 * K - spot()"}),
            simulation, Valuation());
    }

    auto Bumps() {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, 5, 0.0);
        request.directions_(0, 0) = 1.0;
        request.directions_(0, 4) = 0.2;
        request.steps_ = {0.1};
        return request;
    }

    struct MutateInputs_ : Dal::Script::Detail::SimulationObserver_ {
        Dal::BlackScholesLsmcPlan_* plan_;
        Dal::Vector_<>* point_;
        Dal::AAD::BumpOverAADRequest_* bumps_;
        Dal::BlackScholesLsmcPlan_ replacement_ = Plan("replacement");
        size_t calls_ = 0;
        MutateInputs_(Dal::BlackScholesLsmcPlan_* plan, Dal::Vector_<>* point, Dal::AAD::BumpOverAADRequest_* bumps)
            : plan_(plan), point_(point), bumps_(bumps) {}
        void AfterSubmission() override {
            ++calls_;
            *plan_ = replacement_;
            *point_ = {1.0};
            bumps_->directions_ = Dal::Matrix_<>(0, 1);
            bumps_->steps_.clear();
        }
    };
} // namespace

TEST(BlackScholesLsmcBoundaryTest, TestClosedOwningPreparationAndEmptyDirections) {
    Dal::RegisterAll_::Init();
    Dal::Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Dal::Date_(2026, 10, 10);
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.compiled_ = true;
    simulation.smooth_ = 2.0;
    simulation.lsmcTrainingPaths_ = 64;
    const auto product = Dal::NewScriptProduct("", {Dal::Cell_(Dal::Date_(2027, 4, 10)), Dal::Cell_(Dal::Date_(2027, 10, 10))},
                                               {"EXERCISE 100 - spot()", "EXERCISE 100 - spot()"});
    const auto plan = Dal::PlanBlackScholesLsmc(product, simulation, valuation);
    Dal::AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(0, 4);
    const auto result = Dal::ValueByBlackScholesLsmcWithCurvature(plan, {100.0, 0.2, 0.05, 0.0}, 35, bumps);
    ASSERT_EQ(result.Plan().ParameterLabels(), (Dal::Vector_<Dal::String_>{"spot", "vol", "rate", "div"}));
    ASSERT_EQ(result.Curvature().Gradient().size(), 4);
    ASSERT_EQ(result.Curvature().HessianProducts().Rows(), 0);
    ASSERT_EQ(result.Curvature().HessianProducts().Cols(), 4);
    ASSERT_EQ(result.Curvature().BasePolicy().size(), 2);
    ASSERT_EQ(result.Curvature().Execution().gradientEvaluations_, 1);
    ASSERT_EQ(result.Plan().Contract().EventTexts(), product->EventTexts());
}

TEST(BlackScholesLsmcBoundaryTest, TestInputMutationAfterSubmissionCannotChangeStoredEstimator) {
    Dal::RegisterAll_::Init();
    auto plan = Plan();
    Dal::Vector_<> point{100, 0.2, 0.05, 0, 50};
    auto bumps = Bumps();
    const auto reference = Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 35, bumps);
    MutateInputs_ mutate(&plan, &point, &bumps);
    const Dal::Script::Detail::ScopedSimulationObserver_ observer(&mutate);
    const auto result = Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 35, bumps);
    ASSERT_GT(mutate.calls_, 0);
    ASSERT_EQ(result.Plan().Contract().Name(), Dal::String_("original"));
    ASSERT_EQ(result.Curvature().Point(), reference.Curvature().Point());
    ASSERT_EQ(result.Curvature().Steps(), reference.Curvature().Steps());
    ASSERT_DOUBLE_EQ(result.Curvature().Directions()(0, 4), 0.2);
    ASSERT_DOUBLE_EQ(result.Curvature().Value(), reference.Curvature().Value());
    ASSERT_EQ(result.Curvature().Gradient(), reference.Curvature().Gradient());
    for (int j = 0; j < 5; ++j)
        ASSERT_DOUBLE_EQ(result.Curvature().HessianProducts()(0, j), reference.Curvature().HessianProducts()(0, j));
    ASSERT_EQ(plan.Contract().Name(), Dal::String_("replacement"));
    ASSERT_EQ(point.size(), 1);
    ASSERT_TRUE(bumps.steps_.empty());
}

TEST(BlackScholesLsmcBoundaryTest, TestInvalidRequestsRejectedBeforeWorkersAndRecover) {
    Dal::RegisterAll_::Init();
    const auto plan = Plan();
    const Dal::Vector_<> point{100, 0.2, 0.05, 0, 50};
    {
        Dal::Script::TestSupport::RejectSubmissions_ submissions;
        const Dal::Script::Detail::ScopedSimulationObserver_ observer(&submissions);
        ASSERT_THROW((void)Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 0, Bumps()), Dal::Exception_);
        ASSERT_THROW((void)Dal::ValueByBlackScholesLsmcWithCurvature(plan, {}, 35, Bumps()), Dal::Exception_);
        auto bumps = Bumps();
        bumps.numericPayloadBudgetBytes_ = 175;
        ASSERT_THROW((void)Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 35, bumps), Dal::Exception_);
        bumps.numericPayloadBudgetBytes_.reset();
        bumps.directions_(0, 1) = 1.0;
        bumps.steps_ = {0.3};
        ASSERT_THROW((void)Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 35, bumps), Dal::Exception_);
        ASSERT_EQ(submissions.calls_, 0);
    }
    auto bumps = Bumps();
    bumps.numericPayloadBudgetBytes_ = 176;
    const auto result = Dal::ValueByBlackScholesLsmcWithCurvature(plan, point, 35, bumps);
    ASSERT_EQ(result.Curvature().Execution().numericPayloadBytes_, 176);
    ASSERT_EQ(result.Curvature().Execution().gradientEvaluations_, 3);
    ASSERT_EQ(result.Plan().ScriptConstants(), (Dal::Vector_<>{45}));
}

TEST(BlackScholesLsmcBoundaryTest, TestHistoricalConstantReplayUsesFrozenFixingSnapshot) {
    Dal::RegisterAll_::Init();
    auto valuation = Valuation();
    const Dal::Date_ past(2026, 10, 9);
    valuation.fixings_ =
        Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_({{"EQ[LSMC_PUBLIC_HISTORY]", {{Dal::DateTime_(past, 0.0), 80.0}}}}));
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.lsmcTrainingPaths_ = 64;
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[LSMC_PUBLIC_HISTORY]";
    const auto product = Dal::NewScriptProduct("history", {Dal::Cell_("K"), Dal::Cell_(past), Dal::Cell_(Dal::Date_(2027, 4, 10))},
                                               {"45", "x = K * FIX(EQ[LSMC_PUBLIC_HISTORY]) / 40", "EXERCISE x"}, settings);
    const auto plan = Dal::PlanBlackScholesLsmc(product, simulation, valuation);
    simulation.lsmcTrainingPaths_ = 0;
    valuation.evaluationDate_ = Dal::Date_(2040, 1, 1);
    valuation.fixings_.reset();
    Dal::Script::TestSupport::RejectFixingReads_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observer(&reads);
    Dal::AAD::BumpOverAADRequest_ bumps;
    bumps.directions_ = Dal::Matrix_<>(0, 5);
    const auto result = Dal::ValueByBlackScholesLsmcWithCurvature(plan, {100, 0.2, 0, 0, 50}, 35, bumps);
    ASSERT_NEAR(result.Curvature().Value(), 100.0, 1e-10);
    ASSERT_NEAR(result.Curvature().Gradient()[4], 2.0, 1e-10);
    ASSERT_EQ(result.Plan().Valuation().evaluationDate_, Valuation().evaluationDate_);
    ASSERT_EQ(result.Plan().Simulation().lsmcTrainingPaths_, 64);
    ASSERT_EQ(result.Plan().ScriptConstants(), (Dal::Vector_<>{45}));
    ASSERT_EQ(result.Plan().Observations().size(), 1);
    ASSERT_EQ(result.Plan().Observations()[0].value_, 80.0);
    ASSERT_EQ(reads.historyCalls_, 0);
    ASSERT_EQ(reads.fixingCalls_, 0);
}

TEST(BlackScholesLsmcBoundaryTest, TestFactoryRequiresNativeAadAndLiveExercise) {
    Dal::RegisterAll_::Init();
    ASSERT_THROW((void)Dal::PlanBlackScholesLsmc({}, {}, Valuation()), Dal::Exception_);
    const auto product = Dal::NewScriptProduct("exercise", {Dal::Cell_(Dal::Date_(2027, 4, 10))}, {"EXERCISE 100 - spot()"});
    ASSERT_THROW((void)Dal::PlanBlackScholesLsmc(product, {}, Valuation()), Dal::Exception_);
    ASSERT_THROW((void)Dal::PlanBlackScholesLsmc(Dal::NewScriptProduct("no-exercise", {Dal::Cell_(Dal::Date_(2027, 4, 10))}, {"pay PAYS spot()"}),
                                                 Dal::DefaultRiskMonteCarloSettings(), Valuation()),
                 Dal::Exception_);
    auto expired = Valuation();
    expired.evaluationDate_ = Dal::Date_(2040, 1, 1);
    ASSERT_THROW((void)Dal::PlanBlackScholesLsmc(product, Dal::DefaultRiskMonteCarloSettings(), expired), Dal::Exception_);
}
