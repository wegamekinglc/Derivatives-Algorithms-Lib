#include <gtest/gtest.h>

#include "../../../benchmarks/pde_risk_perf/reference.hpp"

TEST(PdeRiskBenchmarkTest, TestIndependentScenarioLoopMatchesFixedPlanAndDiagnostics) {
    auto request = Dal::PdeRiskBench::Request("Bermudan17");
    request.valuation_.settings_.gridPoints_ = 41;
    request.valuation_.settings_.ordinarySteps_ = 2;
    request.valuation_.settings_.rannacherIntervals_ = 0;
    request.valuation_.contract_.exerciseDates_ = {request.valuation_.contract_.maturityDate_};
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

TEST(PdeRiskBenchmarkTest, TestReferenceRejectsUnsupportedCaseAndAxisSelection) {
    ASSERT_THROW(Dal::PdeRiskBench::Request("unsupported"), Dal::Exception_);
    auto request = Dal::PdeRiskBench::Request("Bermudan17");
    request.risk_.carryRisk_ = false;
    ASSERT_THROW(Dal::PdeRiskBench::Reference(request), Dal::Exception_);
}
