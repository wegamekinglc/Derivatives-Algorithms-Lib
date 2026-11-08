#include <gtest/gtest.h>

#include "../../../benchmarks/pde_risk_perf/reference.hpp"

namespace {
    Dal::PDE::EarlyExercisePdeRiskRequest_ SmallRequest() {
        auto request = Dal::PdeRiskBench::Request("Bermudan17");
        request.valuation_.settings_.gridPoints_ = 41;
        request.valuation_.settings_.ordinarySteps_ = 2;
        request.valuation_.settings_.rannacherIntervals_ = 0;
        request.valuation_.contract_.exerciseDates_ = {request.valuation_.contract_.maturityDate_};
        return request;
    }
} // namespace

TEST(PdeRiskBenchmarkTest, TestIndependentScenarioLoopMatchesFixedPlanAndDiagnostics) {
    const auto request = SmallRequest();
    const auto reference = Dal::PdeRiskBench::Reference(request);
    const auto risk = Dal::PDE::EvaluateEarlyExercisePdeRisk(request);
    ASSERT_EQ(reference.scenarios_.size(), 17);
    ASSERT_EQ(reference.work_, 1326);
    ASSERT_EQ(reference.solves_, 34);
    ASSERT_EQ(reference.scenarios_[1].scenarioId_, "Spot:C-");
    ASSERT_EQ(reference.scenarios_[1].offset_, -40.0);
    ASSERT_EQ(reference.scenarios_[16].scenarioId_, "DividendYield[0]:F+");
    ASSERT_NO_THROW(Dal::PdeRiskBench::CheckIdentity(reference, risk));
}

TEST(PdeRiskBenchmarkTest, TestIdentityChecksRejectPlanAndDiagnosticDrift) {
    const auto request = SmallRequest();
    auto reference = Dal::PdeRiskBench::Reference(request);
    const auto risk = Dal::PDE::EvaluateEarlyExercisePdeRisk(request);
    const auto scenario = reference.scenarios_[1];
    reference.scenarios_[1].futureContactChanged_ = !scenario.futureContactChanged_.value();
    ASSERT_THROW(Dal::PdeRiskBench::CheckIdentity(reference, risk), Dal::Exception_);
    reference.scenarios_[1] = scenario;
    reference.scenarios_[1].maxLinearBackwardError_ = 1.0;
    ASSERT_THROW(Dal::PdeRiskBench::CheckIdentity(reference, risk), Dal::Exception_);
    {
        auto plan = risk.Plan();
        plan.steps_[0].sourceTimeIndex_ += 1;
        ASSERT_THROW(Dal::PdeRiskBench::CheckPlan(*reference.plan_, plan), Dal::Exception_);
    }
    {
        auto plan = risk.Plan();
        plan.timeGrid_[0].isCarryKnot_ = !plan.timeGrid_[0].isCarryKnot_;
        ASSERT_THROW(Dal::PdeRiskBench::CheckPlan(*reference.plan_, plan), Dal::Exception_);
    }
}

TEST(PdeRiskBenchmarkTest, TestReferenceRejectsUnsupportedCaseAndAxisSelection) {
    ASSERT_THROW(Dal::PdeRiskBench::Request("unsupported"), Dal::Exception_);
    auto request = Dal::PdeRiskBench::Request("Bermudan17");
    request.risk_.carryRisk_ = false;
    ASSERT_THROW(Dal::PdeRiskBench::Reference(request), Dal::Exception_);
}
