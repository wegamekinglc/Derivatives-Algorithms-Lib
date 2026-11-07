#include <gtest/gtest.h>

#include <cmath>

#include <dal/math/pde/earlyexercisepdeinternal.hpp>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    EarlyExercisePdeRequest_ Request() {
        EarlyExercisePdeRequest_ r;
        r.contract_ = {Date_(2026, 10, 18),
                       Date_(2027, 10, 18),
                       100.0,
                       OptionType_("PUT"),
                       EarlyExerciseStyle_("BERMUDAN"),
                       {Date_(2027, 4, 18), Date_(2027, 10, 18)}};
        r.market_ = {120.0, 0.2, {{Date_(2027, 3, 4), 0.05, 0.0}, {r.contract_.maturityDate_, -0.01, 0.02}}};
        r.settings_.gridPoints_ = 21;
        r.settings_.ordinarySteps_ = 4;
        r.settings_.theta_ = 0.5;
        r.settings_.rannacherIntervals_ = 1;
        return r;
    }
    void AssertDatedAnchors(const EarlyExercisePdeInternal::Plan_& plan, const EarlyExercisePdeRequest_& r) {
        size_t dated = 0;
        for (const auto& node : plan.TimeGrid()) {
            if (!node.calendarDate_)
                continue;
            ++dated;
            ASSERT_DOUBLE_EQ(node.timeYears_, (*node.calendarDate_ - r.contract_.evaluationDate_) / 365.0);
            ASSERT_EQ(node.isExerciseDate_, *node.calendarDate_ == Date_(2027, 4, 18) || *node.calendarDate_ == r.contract_.maturityDate_);
            ASSERT_EQ(node.isCarryKnot_, *node.calendarDate_ == Date_(2027, 3, 4) || *node.calendarDate_ == r.contract_.maturityDate_);
        }
        ASSERT_EQ(dated, 4);
    }
} // namespace

TEST(EarlyExercisePlanTest, TestAllScenariosUseStableActualSubstepsAndFixedGeometry) {
    const auto r = Request();
    auto bumped = r.market_;
    bumped.spot_ = 110.0;
    bumped.volatility_ = 0.8;
    bumped.carry_[0].rate_ = -8.0;
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r, {bumped});
    const auto single = EvaluateEarlyExercisePde(r);
    ASSERT_GT(plan.Steps().size(), single.TimeGrid().size() - 1);
    ASSERT_DOUBLE_EQ(plan.Upper(), 480.0);
    ASSERT_DOUBLE_EQ(plan.Epsilon(), r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * 110.0);
    ASSERT_EQ(plan.Counts().base_, 6);
    ASSERT_EQ(plan.Counts().damping_, 2);
    ASSERT_EQ(plan.BasePlannedSteps(), single.Execution().plannedSteps_);
    AssertDatedAnchors(plan, r);
    for (const auto& market : {r.market_, bumped}) {
        auto scenario = r;
        scenario.market_ = market;
        const auto result = EarlyExercisePdeInternal::Rollback(scenario, plan);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_EQ(result.Grid(), plan.Grid());
        ASSERT_EQ(result.TimeGrid().size(), plan.TimeGrid().size());
        ASSERT_DOUBLE_EQ(*result.Request().settings_.upper_, 480.0);
        ASSERT_DOUBLE_EQ(result.Execution().epsilon_, plan.Epsilon());
        for (size_t j = 0; j < plan.TimeGrid().size(); ++j) {
            const auto& actual = result.TimeGrid()[j];
            const auto& expected = plan.TimeGrid()[j];
            ASSERT_DOUBLE_EQ(actual.timeYears_, expected.timeYears_);
            ASSERT_EQ(actual.calendarDate_, expected.calendarDate_);
            ASSERT_EQ(actual.isExerciseDate_, expected.isExerciseDate_);
            ASSERT_EQ(actual.isCarryKnot_, expected.isCarryKnot_);
            ASSERT_EQ(actual.isMaturity_, expected.isMaturity_);
        }
        for (const auto& step : plan.Steps()) {
            const double dt = step.source_ - step.target_;
            const auto& carry = market.carry_[step.carry_];
            ASSERT_GT(dt, 0.0);
            ASSERT_GT(1.0 + step.theta_ * dt * carry.rate_, 0.0);
            for (int i = 1; i + 1 < r.settings_.gridPoints_; ++i) {
                const double diffusion = 0.5 * std::pow(market.volatility_ * i, 2);
                const double drift = (carry.rate_ - carry.dividendYield_) * i;
                const double diagonal = 2.0 * diffusion + (diffusion < 0.5 * std::abs(drift) ? std::abs(drift) : 0.0) + carry.rate_;
                ASSERT_GE(1.0 - (1.0 - step.theta_) * dt * diagonal, 0.0);
            }
        }
    }
    ASSERT_DOUBLE_EQ(single.Execution().epsilon_, r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * 120.0);
    ASSERT_DOUBLE_EQ(EvaluateEarlyExercisePde(r).Price(), single.Price());
}

TEST(EarlyExercisePlanTest, TestCanonicalPlanIdentityMatchesIndependentBinaryEncoding) {
    auto r = Request();
    r.market_ = {100.0, 0.2, {{r.contract_.maturityDate_, 0.05, 0.0}}};
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    r.settings_.gridPoints_ = 5;
    r.settings_.ordinarySteps_ = 2;
    r.settings_.theta_ = 1.0;
    r.settings_.rannacherIntervals_ = 0;
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    ASSERT_FALSE(plan.Id().empty());
    ASSERT_EQ(plan.Id().size(), 76);
    ASSERT_EQ(std::string(plan.Id().data(), plan.Id().size()), "p3v1:sha256:c9f7a5df75982b82128694a28da0d82a57478a89ecdfce77b4085c01a3c595ff");
    ASSERT_EQ(plan.CanonicalBytes(), 313);
    ASSERT_GT(plan.StorageBytes(), plan.CanonicalBytes());
    ASSERT_GT(plan.CanonicalWorkspaceBytes(), plan.CanonicalBytes());
}

TEST(EarlyExercisePlanTest, TestCommonPlanAdmitsFullScenarioStorageBeforeRollback) {
    auto r = Request();
    r.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.contract_.exerciseDates_.clear();
    r.settings_.gridPoints_ = 41;
    r.settings_.ordinarySteps_ = 8;
    r.settings_.theta_ = 1.0;
    const auto single = EvaluateEarlyExercisePde(r);
    r.settings_.numericPayloadBudgetBytes_ = single.Execution().plannedNumericPayloadBytes_;
    r.settings_.workspaceBudgetBytes_ = single.Execution().plannedWorkspaceBytes_;
    ASSERT_NO_THROW(static_cast<void>(EarlyExercisePdeInternal::BuildPlan(r)));
    --r.settings_.numericPayloadBudgetBytes_;
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::BuildPlan(r)), Exception_);
}

TEST(EarlyExercisePlanTest, TestRollbackRejectsScenarioWithLooserThanItsOwnTolerance) {
    auto r = Request();
    r.settings_.theta_ = 1.0;
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    r.market_.spot_ = 90.0;
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::Rollback(r, plan)), Exception_);
}

TEST(EarlyExercisePlanTest, TestIdentityDependsOnNumericContentAndNotBudgetsOrMarketIdentity) {
    auto r = Request();
    r.settings_.theta_ = 1.0;
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    auto extra = r.market_;
    extra.volatility_ = 0.3;
    extra.carry_[0].dividendYield_ = -0.0;
    const auto reordered = EarlyExercisePdeInternal::BuildPlan(r, {extra, r.market_, extra});
    ASSERT_EQ(reordered.Grid(), plan.Grid());
    ASSERT_EQ(reordered.TimeGrid().size(), plan.TimeGrid().size());
    ASSERT_EQ(reordered.Id(), plan.Id());
    r.settings_.maxWorkUnits_ = 0;
    r.settings_.numericPayloadBudgetBytes_ *= 2;
    r.settings_.workspaceBudgetBytes_ *= 2;
    r.settings_.maxActualSteps_ *= 2;
    ASSERT_EQ(EarlyExercisePdeInternal::BuildPlan(r).Id(), plan.Id());
    r.settings_.penaltyFactor_ *= 2.0;
    ASSERT_NE(EarlyExercisePdeInternal::BuildPlan(r).Id(), plan.Id());
    r.settings_.penaltyFactor_ /= 2.0;
    r.contract_.exerciseDates_.front() = Date_(2027, 4, 19);
    ASSERT_NE(EarlyExercisePdeInternal::BuildPlan(r).Id(), plan.Id());
}

TEST(EarlyExercisePlanTest, TestSingletonPlanPreservesPriceLayersAndResidualsForBothStyles) {
    for (const char* style : {"BERMUDAN", "AMERICAN"}) {
        for (double theta : {1.0, 0.5}) {
            auto r = Request();
            r.contract_.exerciseStyle_ = EarlyExerciseStyle_(style);
            if (r.contract_.exerciseStyle_ == EarlyExerciseStyle_::Value_::AMERICAN)
                r.contract_.exerciseDates_.clear();
            r.settings_.theta_ = theta;
            r.settings_.ordinarySteps_ = 100;
            const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
            const auto fixed = EarlyExercisePdeInternal::Rollback(r, plan);
            const auto single = EvaluateEarlyExercisePde(r);
            ASSERT_TRUE(single.IsSuccess());
            ASSERT_TRUE(fixed.IsSuccess());
            ASSERT_DOUBLE_EQ(fixed.Price(), single.Price());
            ASSERT_EQ(fixed.Values(), single.Values());
            ASSERT_EQ(fixed.Grid(), single.Grid());
            ASSERT_EQ(fixed.Execution().workUnits_, single.Execution().workUnits_);
            ASSERT_EQ(fixed.Execution().totalSolves_, single.Execution().totalSolves_);
            ASSERT_EQ(fixed.Execution().upwindRows_, single.Execution().upwindRows_);
            ASSERT_EQ(fixed.StepDiagnostics().size(), single.StepDiagnostics().size());
            for (size_t i = 0; i < fixed.StepDiagnostics().size(); ++i) {
                const auto& a = fixed.StepDiagnostics()[i];
                const auto& b = single.StepDiagnostics()[i];
                ASSERT_DOUBLE_EQ(a.dt_, b.dt_);
                ASSERT_DOUBLE_EQ(a.theta_, b.theta_);
                ASSERT_EQ(a.solveCount_, b.solveCount_);
                ASSERT_EQ(a.damped_, b.damped_);
                ASSERT_EQ(a.projection_, b.projection_);
                ASSERT_EQ(a.complementarityResidual_, b.complementarityResidual_);
                ASSERT_EQ(a.normalizedComplementarityResidual_, b.normalizedComplementarityResidual_);
                ASSERT_EQ(a.linearBackwardError_, b.linearBackwardError_);
            }
            ASSERT_EQ(fixed.ExerciseRegions().size(), single.ExerciseRegions().size());
            for (size_t i = 0; i < fixed.ExerciseRegions().size(); ++i) {
                const auto& a = fixed.ExerciseRegions()[i];
                const auto& b = single.ExerciseRegions()[i];
                ASSERT_EQ(a.timeIndex_, b.timeIndex_);
                ASSERT_EQ(a.firstNode_, b.firstNode_);
                ASSERT_EQ(a.lastNode_, b.lastNode_);
                ASSERT_EQ(a.leftClipped_, b.leftClipped_);
                ASSERT_EQ(a.rightClipped_, b.rightClipped_);
            }
        }
    }
}

TEST(EarlyExercisePlanTest, TestCommonEpsilonKeepsScenarioSpecificResidualNormalizationAndContact) {
    auto r = Request();
    r.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.contract_.exerciseDates_.clear();
    r.settings_.theta_ = 1.0;
    r.settings_.ordinarySteps_ = 100;
    auto bump = r.market_;
    bump.spot_ = 110.0;
    bump.volatility_ = 0.0;
    for (auto& carry : bump.carry_) {
        carry.rate_ = 0.1;
        carry.dividendYield_ = 0.1;
    }
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r, {bump});
    const auto base = EarlyExercisePdeInternal::Rollback(r, plan);
    r.market_ = bump;
    const auto other = EarlyExercisePdeInternal::Rollback(r, plan);
    ASSERT_TRUE(base.IsSuccess());
    ASSERT_TRUE(other.IsSuccess());
    ASSERT_NE(base.Values(), other.Values());
    ASSERT_NE(base.ExerciseRegions().back().lastNode_, other.ExerciseRegions().back().lastNode_);
    for (const auto* result : {&base, &other}) {
        const double scale = std::max(result->Request().market_.spot_, r.contract_.strike_);
        ASSERT_DOUBLE_EQ(result->Execution().epsilon_, plan.Epsilon());
        for (const auto& step : result->StepDiagnostics()) {
            ASSERT_EQ(*step.normalizedComplementarityResidual_, *step.complementarityResidual_ / scale);
            ASSERT_LE(*step.complementarityResidual_, plan.Epsilon());
        }
    }
}

TEST(EarlyExercisePlanTest, TestFixedPlanRejectsChangedLayoutSettingsAndInsufficientStepLimit) {
    const auto r = Request();
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    for (int field = 0; field < 4; ++field) {
        auto invalid = r;
        if (field == 0)
            invalid.market_.carry_[0].endDate_ = Date_(2027, 3, 5);
        else if (field == 1)
            invalid.contract_.exerciseDates_.front() = Date_(2027, 4, 19);
        else if (field == 2)
            invalid.settings_.theta_ = 1.0;
        else
            invalid.settings_.upper_ = 500.0;
        ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::Rollback(invalid, plan)), Exception_);
    }
    auto invalidMarket = r.market_;
    invalidMarket.carry_[0].endDate_ = Date_(2027, 3, 5);
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::BuildPlan(r, {invalidMarket})), Exception_);
    auto limited = r;
    limited.settings_.maxActualSteps_ = static_cast<int>(plan.Steps().size()) - 1;
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::Rollback(limited, plan)), Exception_);
    auto bump = r.market_;
    bump.volatility_ = 2.0;
    limited.settings_.maxActualSteps_ = static_cast<int>(plan.Steps().size());
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::BuildPlan(limited, {bump})), Exception_);
}

TEST(EarlyExercisePlanTest, TestFixedPlanFailureRetainsDiagnosticsAndIsReusable) {
    auto r = Request();
    r.market_ = {100.0, 0.2, {{r.contract_.maturityDate_, 0.05, 0.03}}};
    r.settings_.gridPoints_ = 41;
    r.settings_.ordinarySteps_ = 2;
    r.settings_.theta_ = 1.0;
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    ASSERT_EQ(plan.Steps().size(), 2);
    r.settings_.maxWorkUnits_ = 77;
    const auto failed = EarlyExercisePdeInternal::Rollback(r, plan);
    ASSERT_EQ(failed.Status(), EarlyExercisePdeStatus_("WORK_LIMIT"));
    ASSERT_EQ(failed.Execution().workUnits_, 39);
    ASSERT_EQ(failed.Execution().completedSteps_, 1);
    ASSERT_EQ(failed.TimeGrid().size(), plan.TimeGrid().size());
    ASSERT_THROW(static_cast<void>(failed.Price()), Exception_);
    ASSERT_THROW(static_cast<void>(failed.Values()), Exception_);
    r.settings_.maxWorkUnits_ = 78;
    const auto success = EarlyExercisePdeInternal::Rollback(r, plan);
    ASSERT_TRUE(success.IsSuccess());
    ASSERT_EQ(success.Execution().workUnits_, 78);
    ASSERT_DOUBLE_EQ(success.Price(), EvaluateEarlyExercisePde(r).Price());
}

TEST(EarlyExercisePlanTest, TestCanonicalizationWorkspaceIsAdmittedAtItsExactBoundary) {
    auto r = Request();
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    r.settings_.gridPoints_ = 5;
    r.settings_.theta_ = 1.0;
    r.settings_.ordinarySteps_ = 2000;
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
    r.settings_.workspaceBudgetBytes_ = plan.CanonicalWorkspaceBytes();
    ASSERT_EQ(EarlyExercisePdeInternal::BuildPlan(r).Id(), plan.Id());
    --r.settings_.workspaceBudgetBytes_;
    ASSERT_THROW(static_cast<void>(EarlyExercisePdeInternal::BuildPlan(r)), Exception_);
}

TEST(EarlyExercisePlanTest, TestBorrowedStepsAreExcludedFromRollbackWorkspace) {
    std::optional<size_t> rollbackBytes;
    for (int steps : {2, 500, 2000}) {
        auto r = Request();
        r.market_ = {100, 0, {{r.contract_.maturityDate_, .05, .03}}};
        r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
        r.settings_.gridPoints_ = 5;
        r.settings_.upper_ = 400;
        r.settings_.theta_ = 1;
        r.settings_.ordinarySteps_ = steps;
        r.settings_.rannacherIntervals_ = 0;
        const auto plan = EarlyExercisePdeInternal::BuildPlan(r);
        const auto owned = EvaluateEarlyExercisePde(r);
        const auto borrowed = EarlyExercisePdeInternal::RollbackForRisk(r, plan);
        const size_t sharedBytes = plan.Steps().capacity() * sizeof(EarlyExercisePdeInternal::PlanStep_);
        ASSERT_EQ(borrowed.Execution().peakWorkspaceBytes_, owned.Execution().peakWorkspaceBytes_ - sharedBytes);
        if (rollbackBytes)
            ASSERT_EQ(borrowed.Execution().peakWorkspaceBytes_, *rollbackBytes);
        rollbackBytes = borrowed.Execution().peakWorkspaceBytes_;
        ASSERT_EQ(borrowed.Execution().numericPayloadBytes_, owned.Execution().numericPayloadBytes_);
        ASSERT_EQ(borrowed.Execution().workUnits_, owned.Execution().workUnits_);
        ASSERT_DOUBLE_EQ(borrowed.Price(), owned.Price());
        ASSERT_EQ(borrowed.Values(), owned.Values());
    }
}
