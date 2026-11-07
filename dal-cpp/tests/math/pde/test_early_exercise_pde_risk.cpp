#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <future>
#include <limits>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/pde/earlyexercisepdeinternal.hpp>
#include <dal/math/pde/earlyexercisepderisk.hpp>
#include <dal/math/pde/earlyexercisepderiskinternal.hpp>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    EarlyExercisePdeRiskRequest_ RiskRequest() {
        EarlyExercisePdeRiskRequest_ r;
        const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
        r.valuation_.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {maturity}};
        r.valuation_.market_ = {100.0, 0.2, {{maturity, 0.05, 0.03}}};
        r.valuation_.settings_.gridPoints_ = 41;
        r.valuation_.settings_.ordinarySteps_ = 2;
        r.valuation_.settings_.upper_ = 400.0;
        r.valuation_.settings_.rannacherIntervals_ = 0;
        return r;
    }
    template <class F_> void Error(F_ operation, const char* prefix, const char* field) {
        try {
            operation();
            FAIL() << "expected " << prefix;
        } catch (const Exception_& e) {
            ASSERT_NE(std::string(e.what()).find(prefix), std::string::npos) << e.what();
            ASSERT_NE(std::string(e.what()).find(field), std::string::npos) << e.what();
        }
    }
    void Counts(const EarlyExercisePdeRiskResult_& result) {
        const auto& e = result.Execution();
        ASSERT_EQ(e.attemptedScenarios_, e.successfulScenarios_ + e.failedScenarios_);
        ASSERT_EQ(e.plannedScenarios_, e.attemptedScenarios_ + e.notRunScenarios_);
        size_t work = 0, solves = 0;
        for (const auto& row : result.Scenarios()) {
            ASSERT_NE(row.status_, EarlyExercisePdeRiskScenarioStatus_("PLANNED"));
            work += row.workUnits_;
            solves += row.totalSolves_;
            if (row.status_ == EarlyExercisePdeRiskScenarioStatus_("NOT_RUN_WORK_LIMIT") ||
                row.status_ == EarlyExercisePdeRiskScenarioStatus_("NOT_RUN_BASE_FAILED")) {
                ASSERT_EQ(row.workUnits_, 0);
                ASSERT_EQ(row.totalSolves_, 0);
                ASSERT_EQ(row.actualSteps_, 0);
                ASSERT_EQ(row.completedSteps_, 0);
                ASSERT_EQ(row.numericPayloadBytes_, 0);
                ASSERT_EQ(row.peakWorkspaceBytes_, 0);
                ASSERT_FALSE(row.solverStatus_);
                ASSERT_FALSE(row.failure_);
                ASSERT_FALSE(row.price_);
                ASSERT_FALSE(row.evaluationMembership_);
                ASSERT_FALSE(row.futureContactChanged_);
                ASSERT_FALSE(row.upwindChanged_);
            }
        }
        ASSERT_EQ(e.workUnits_, work);
        ASSERT_EQ(e.totalSolves_, solves);
        ASSERT_LE(e.workUnits_, result.Request().risk_.maxWorkUnits_);
        ASSERT_LE(e.numericPayloadBytes_, e.plannedNumericPayloadBytes_);
        ASSERT_LE(e.peakWorkspaceBytes_, e.plannedWorkspaceBytes_);
        ASSERT_LE(e.peakNativeBytes_, e.plannedPeakNativeBytes_);
    }
    void IncompleteWorkResult(const EarlyExercisePdeRiskResult_& result, size_t cap, const EarlyExercisePdeRiskResult_& control) {
        if (cap < 78) {
            ASSERT_EQ(result.Status(), EarlyExercisePdeRiskStatus_("BASE_FAILED"));
            ASSERT_FALSE(result.PriceAvailable());
            ASSERT_EQ(result.Execution().workLimitScenarioIndex_, 0);
            for (const auto& risk : result.Risks())
                ASSERT_EQ(risk.status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_BASE_FAILED"));
            ASSERT_NO_FATAL_FAILURE(Error([&]() { static_cast<void>(result.Price()); }, "EarlyExercisePdeRiskPriceUnavailable", "WORK_LIMIT"));
            return;
        }
        ASSERT_EQ(result.Status(), EarlyExercisePdeRiskStatus_("WORK_LIMIT"));
        ASSERT_TRUE(result.PriceAvailable());
        if (cap == 1325) {
            ASSERT_EQ(result.Scenarios().back().status_, EarlyExercisePdeRiskScenarioStatus_("NUMERICAL_FAILURE"));
            ASSERT_EQ(result.Scenarios().back().solverStatus_, EarlyExercisePdeStatus_("WORK_LIMIT"));
            ASSERT_EQ(result.Scenarios().back().workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("GLOBAL"));
            ASSERT_EQ(result.Scenarios().back().workUnits_, 39);
        } else {
            const size_t stopped = cap / 78;
            ASSERT_EQ(result.Scenarios()[stopped].status_, EarlyExercisePdeRiskScenarioStatus_("NOT_RUN_WORK_LIMIT"));
            ASSERT_EQ(result.Scenarios()[stopped].workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("GLOBAL"));
        }
        if (cap == 390)
            for (size_t i : {0, 1}) {
                ASSERT_EQ(result.Risks()[i].status_, control.Risks()[i].status_);
                ASSERT_EQ(result.Risks()[i].fineEstimate_, control.Risks()[i].fineEstimate_);
            }
    }
    void ScenarioCap(int axes) {
        namespace I = EarlyExercisePdeRiskInternal;
        auto r = RiskRequest();
        if (axes == 1)
            r.valuation_.market_.carry_ = {{Date_(2027, 3, 4), .02, .01}, {Date_(2027, 10, 18), .06, .03}};
        else if (axes == 2)
            r.risk_.vega_ = r.risk_.carryRisk_ = false;
        const size_t count = axes == 0 ? 17 : (axes == 1 ? 25 : 5);
        r.risk_.maxScenarios_ = count;
        const auto result = EvaluateEarlyExercisePdeRisk(r);
        ASSERT_EQ(result.Scenarios().size(), count);
        size_t rollbacks = 0;
        I::TestControl_ seam;
        seam.rollbacksStarted_ = &rollbacks;
        --r.risk_.maxScenarios_;
        ASSERT_NO_FATAL_FAILURE(Error([&]() { static_cast<void>(I::Evaluate(r, seam)); }, "EarlyExercisePdeRiskBudgetExceeded", "max_scenarios"));
        ASSERT_EQ(rollbacks, 0);
    }
    void InvalidSwitchAndBump(int field, EarlyExercisePdeRiskRequest_* r) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        if (field == 0)
            r->risk_.delta_ = r->risk_.gamma_ = r->risk_.vega_ = r->risk_.carryRisk_ = false;
        else if (field == 1)
            r->risk_.rateBumps_ = {1e-4, 1e-4};
        else if (field == 2) {
            r->risk_.vega_ = false;
            r->risk_.volatilityBump_ = nan;
        } else if (field == 3) {
            r->risk_.carryRisk_ = false;
            r->risk_.dividendYieldBumps_ = {nan};
        } else if (field == 4)
            r->risk_.spotBump_ = 10;
    }
    void InvalidPrecision(int field, EarlyExercisePdeRiskRequest_* r) {
        if (field == 5)
            r->risk_.spotBump_ = 20.1;
        else if (field == 6)
            r->risk_.rateBumps_ = {std::numeric_limits<double>::denorm_min()};
        else if (field == 7)
            r->risk_.gammaStabilityTolerance_ = std::numeric_limits<double>::quiet_NaN();
        else
            r->valuation_.market_.carry_[0].rate_ = 1e100;
    }
    EarlyExercisePdeMarket_ ScenarioMarket(const EarlyExercisePdeMarket_& base, const EarlyExercisePdeRiskScenario_& row) {
        auto market = base;
        if (*row.parameterId_ == "Spot")
            market.spot_ = *row.parameterValue_;
        else if (*row.parameterId_ == "Volatility")
            market.volatility_ = *row.parameterValue_;
        else if (std::string(row.parameterId_->data(), row.parameterId_->size()).find("Rate[") == 0)
            market.carry_[*row.segmentIndex_].rate_ = *row.parameterValue_;
        else
            market.carry_[*row.segmentIndex_].dividendYield_ = *row.parameterValue_;
        return market;
    }
    void CarryMetadata(const EarlyExercisePdeRiskResult_& result, const EarlyExercisePdeRiskRequest_& r) {
        for (size_t j = 0; j < 2; ++j)
            for (size_t k = 0; k < 2; ++k) {
                const auto& risk = result.Risks()[3 + 2 * j + k];
                ASSERT_EQ(risk.riskIndex_, 3 + 2 * j + k);
                ASSERT_EQ(risk.segmentIndex_, j);
                ASSERT_EQ(risk.startDate_, j == 0 ? r.valuation_.contract_.evaluationDate_ : r.valuation_.market_.carry_[0].endDate_);
                ASSERT_EQ(risk.endDate_, r.valuation_.market_.carry_[j].endDate_);
                ASSERT_EQ(risk.riskId_, String_(k == 0 ? "Rate[" : "DividendYield[") + String_(std::to_string(j)) + "]");
            }
    }
} // namespace

TEST(EarlyExercisePdeRiskTest, TestCompleteRepricingSharesSpotScenariosAndRecordsStencils) {
    const auto r = RiskRequest();
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_TRUE(result.PriceAvailable());
    ASSERT_EQ(result.Scenarios().size(), 17);
    ASSERT_EQ(result.Risks().size(), 5);
    ASSERT_EQ(result.Execution().totalSolves_, 34);
    ASSERT_EQ(result.Execution().workUnits_, 1326);
    ASSERT_EQ(result.Plan().plannedSteps_, 2);
    ASSERT_EQ(result.Method(), "BlackScholesEarlyExerciseFullRepriceFixedPlan");
    ASSERT_FALSE(result.AadUsed());
    const auto& delta = result.Risks()[0];
    const auto& gamma = result.Risks()[1];
    ASSERT_EQ(delta.riskId_, "Delta");
    ASSERT_EQ(gamma.riskId_, "Gamma");
    ASSERT_EQ(delta.fineStencil_[0].scenarioIndex_, gamma.fineStencil_[0].scenarioIndex_);
    ASSERT_EQ(gamma.fineStencil_[1].scenarioIndex_, 0);
    ASSERT_EQ(result.Scenarios()[1].scenarioId_, "Spot:C-");
    ASSERT_DOUBLE_EQ(*delta.fineEstimate_, (*result.Scenarios()[4].price_ - *result.Scenarios()[3].price_) / 40.0);
    ASSERT_DOUBLE_EQ(*gamma.fineEstimate_, (*result.Scenarios()[4].price_ - 2.0 * result.Price() + *result.Scenarios()[3].price_) / 400.0);
    for (const auto& scenario : result.Scenarios()) {
        ASSERT_EQ(scenario.planId_, result.Plan().planId_);
        ASSERT_EQ(scenario.plannedSteps_, 2);
        ASSERT_EQ(scenario.status_, EarlyExercisePdeRiskScenarioStatus_("SUCCESS"));
        ASSERT_DOUBLE_EQ(scenario.epsilon_, result.Plan().epsilon_);
    }
}

TEST(EarlyExercisePdeRiskTest, TestPeakCountsSimultaneousLifetimesInsteadOfUnrelatedPhaseMaxima) {
    auto r = RiskRequest();
    r.valuation_.settings_.gridPoints_ = 5;
    r.valuation_.settings_.ordinarySteps_ = 2000;
    r.valuation_.market_.volatility_ = 0.0;
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    const auto& e = result.Execution();
    ASSERT_LT(e.peakNativeBytes_, e.numericPayloadBytes_ + e.peakWorkspaceBytes_);
    ASSERT_LE(e.peakNativeBytes_, e.plannedPeakNativeBytes_);
}

TEST(EarlyExercisePdeRiskTest, TestPrivateFirstSolveFailurePreservesIndependentRisks) {
    const auto r = RiskRequest();
    const auto control = EvaluateEarlyExercisePdeRisk(r);
    EarlyExercisePdeRiskInternal::TestControl_ seam;
    seam.scenarioIndex_ = 12;
    seam.failFirstSolve_ = true;
    const auto failed = EarlyExercisePdeRiskInternal::Evaluate(r, seam);
    ASSERT_EQ(failed.Status(), EarlyExercisePdeRiskStatus_("PARTIAL"));
    ASSERT_TRUE(failed.PriceAvailable());
    ASSERT_EQ(failed.Scenarios()[12].scenarioId_, "Rate[0]:F+");
    ASSERT_EQ(failed.Scenarios()[12].solverStatus_, EarlyExercisePdeStatus_("LINEAR_SOLVE_FAILURE"));
    ASSERT_EQ(failed.Scenarios()[12].totalSolves_, 1);
    ASSERT_EQ(failed.Scenarios()[12].completedSteps_, 0);
    ASSERT_FALSE(failed.Scenarios()[12].price_);
    ASSERT_EQ(failed.Risks()[3].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_SCENARIO_FAILED"));
    for (size_t i : {0, 1, 2, 4}) {
        ASSERT_EQ(failed.Risks()[i].status_, control.Risks()[i].status_);
        ASSERT_EQ(failed.Risks()[i].fineEstimate_, control.Risks()[i].fineEstimate_);
        ASSERT_EQ(failed.Risks()[i].value_, control.Risks()[i].value_);
    }
}

TEST(EarlyExercisePdeRiskTest, TestLocalSettingsRejectOverflowingCoarseSpotBump) {
    auto s = RiskRequest().risk_;
    s.spotBump_ = std::numeric_limits<double>::max();
    ASSERT_THROW(ValidateEarlyExercisePdeRiskSettings(s), Exception_);
}

TEST(EarlyExercisePdeRiskTest, TestExactCumulativeWorkBoundariesAndPreservedCompletedRisks) {
    const auto control = EvaluateEarlyExercisePdeRisk(RiskRequest());
    for (size_t cap : {1326, 1325, 390, 78, 77, 0}) {
        auto r = RiskRequest();
        r.risk_.maxWorkUnits_ = cap;
        const auto result = EvaluateEarlyExercisePdeRisk(r);
        ASSERT_NO_FATAL_FAILURE(Counts(result));
        const size_t expected = cap == 1326 ? 1326 : (cap == 1325 ? 1287 : (cap == 77 ? 39 : cap));
        ASSERT_EQ(result.Execution().workUnits_, expected);
        if (cap < 1326) {
            ASSERT_NO_FATAL_FAILURE(IncompleteWorkResult(result, cap, control));
        } else {
            ASSERT_NE(result.Status(), EarlyExercisePdeRiskStatus_("WORK_LIMIT"));
            ASSERT_FALSE(result.Execution().workLimitScenarioIndex_);
        }
    }
}

TEST(EarlyExercisePdeRiskTest, TestScenarioCapAndIndependentScenarioWorkCap) {
    auto r = RiskRequest();
    r.valuation_.settings_.maxWorkUnits_ = 77;
    const auto baseFailed = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(baseFailed.Status(), EarlyExercisePdeRiskStatus_("BASE_FAILED"));
    ASSERT_EQ(baseFailed.Execution().workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("SCENARIO"));
    ASSERT_NO_FATAL_FAILURE(Counts(baseFailed));
    r.valuation_.settings_.maxWorkUnits_ = 78;
    EarlyExercisePdeRiskInternal::TestControl_ seam;
    seam.scenarioIndex_ = 9;
    seam.extraFirstSolve_ = true;
    const auto partial = EarlyExercisePdeRiskInternal::Evaluate(r, seam);
    ASSERT_EQ(partial.Status(), EarlyExercisePdeRiskStatus_("PARTIAL"));
    ASSERT_TRUE(partial.PriceAvailable());
    ASSERT_EQ(partial.Scenarios()[9].workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("SCENARIO"));
    ASSERT_EQ(partial.Scenarios()[9].workUnits_, 78);
    ASSERT_EQ(partial.Risks()[3].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_SCENARIO_FAILED"));
    ASSERT_EQ(partial.Scenarios().back().status_, EarlyExercisePdeRiskScenarioStatus_("SUCCESS"));
    ASSERT_NO_FATAL_FAILURE(Counts(partial));
}

TEST(EarlyExercisePdeRiskTest, TestGlobalWinsWhenBothRemainingCapsBlockTheSameSolve) {
    auto r = RiskRequest();
    r.risk_.maxWorkUnits_ = 77;
    r.valuation_.settings_.maxWorkUnits_ = 76;
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(result.Execution().workUnits_, 39);
    ASSERT_EQ(result.Execution().workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("GLOBAL"));
    ASSERT_EQ(result.Scenarios()[0].workLimitSource_, EarlyExercisePdeRiskWorkLimitSource_("GLOBAL"));
}

TEST(EarlyExercisePdeRiskTest, TestPreflightExactScenarioAndByteEnvelopesAndOverflow) {
    namespace I = EarlyExercisePdeRiskInternal;
    for (int axes = 0; axes < 3; ++axes)
        ASSERT_NO_FATAL_FAILURE(ScenarioCap(axes));
    auto r = RiskRequest();
    const auto reference = EvaluateEarlyExercisePdeRisk(r);
    for (bool payload : {true, false}) {
        size_t& cap = payload ? r.risk_.numericPayloadBudgetBytes_ : r.risk_.workspaceBudgetBytes_;
        cap = payload ? reference.Plan().plannedNumericPayloadBytes_ : reference.Plan().plannedWorkspaceBytes_;
        const auto exact = EvaluateEarlyExercisePdeRisk(r);
        ASSERT_EQ(exact.Plan().planId_, reference.Plan().planId_);
        ASSERT_EQ(exact.Plan().grid_, reference.Plan().grid_);
        ASSERT_EQ(exact.Execution().workUnits_, reference.Execution().workUnits_);
        --cap;
        size_t rollbacks = 0;
        I::TestControl_ seam;
        seam.rollbacksStarted_ = &rollbacks;
        ASSERT_NO_FATAL_FAILURE(Error([&]() { static_cast<void>(I::Evaluate(r, seam)); }, "EarlyExercisePdeRiskBudgetExceeded", "budget_bytes"));
        ASSERT_EQ(rollbacks, 0);
        r = RiskRequest();
    }
    const auto maximum = std::numeric_limits<size_t>::max();
    ASSERT_THROW(static_cast<void>(I::CheckedAdd(maximum, 1)), Exception_);
    ASSERT_THROW(static_cast<void>(I::CheckedMultiply(maximum, 2)), Exception_);
    ASSERT_EQ(I::CheckedMultiply(maximum, 0), 0);
    r.valuation_.settings_.gridPoints_ = 100001;
    r.valuation_.settings_.ordinarySteps_ = std::numeric_limits<int>::max();
    r.valuation_.settings_.maxActualSteps_ = std::numeric_limits<int>::max();
    r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = 0;
    ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Exception_);
    r = RiskRequest();
    r.valuation_.settings_.numericPayloadBudgetBytes_ = reference.BaseResult().Execution().plannedNumericPayloadBytes_ - 1;
    ASSERT_NO_FATAL_FAILURE(Error([&]() { static_cast<void>(EvaluateEarlyExercisePdeRisk(r)); }, "EarlyExercisePdeRiskBudgetExceeded", "valuation"));
    r = RiskRequest();
    r.valuation_.settings_.maxActualSteps_ = 1;
    ASSERT_NO_FATAL_FAILURE(
        Error([&]() { static_cast<void>(EvaluateEarlyExercisePdeRisk(r)); }, "EarlyExercisePdeRiskBudgetExceeded", "max_actual_steps"));
    r = RiskRequest();
    r.valuation_.settings_.workspaceBudgetBytes_ = reference.BaseResult().Execution().plannedWorkspaceBytes_ - 1;
    ASSERT_NO_FATAL_FAILURE(
        Error([&]() { static_cast<void>(EvaluateEarlyExercisePdeRisk(r)); }, "EarlyExercisePdeRiskBudgetExceeded", "workspace_budget_bytes"));
    r = RiskRequest();
    r.risk_.maxScenarios_ = r.risk_.maxWorkUnits_ = r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = maximum;
    ASSERT_EQ(EvaluateEarlyExercisePdeRisk(r).Execution().workUnits_, reference.Execution().workUnits_);
}

TEST(EarlyExercisePdeRiskTest, TestDegenerateZeroWorkDomainRejectionAndBaseFailurePrecedence) {
    auto r = RiskRequest();
    r.valuation_.market_.volatility_ = 0;
    r.valuation_.market_.carry_[0].rate_ = r.valuation_.market_.carry_[0].dividendYield_ = 0;
    r.valuation_.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.valuation_.contract_.exerciseDates_.clear();
    r.risk_.maxScenarios_ = 1;
    r.risk_.maxWorkUnits_ = r.valuation_.settings_.maxWorkUnits_ = 0;
    const auto zero = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(zero.Status(), EarlyExercisePdeRiskStatus_("PARTIAL"));
    ASSERT_TRUE(zero.PriceAvailable());
    ASSERT_EQ(zero.Execution().attemptedScenarios_, 1);
    ASSERT_EQ(zero.Execution().workUnits_, 0);
    ASSERT_NO_FATAL_FAILURE(Counts(zero));
    for (const auto& risk : zero.Risks()) {
        ASSERT_EQ(risk.status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_DEGENERATE_DIFFUSION"));
        ASSERT_TRUE(risk.coarseStencil_.empty());
        ASSERT_FALSE(risk.value_);
    }
    r.risk_.maxScenarios_ = 0;
    ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Exception_);
    r = RiskRequest();
    r.valuation_.market_.spot_ = .25;
    r.valuation_.market_.volatility_ = 1e-8;
    const auto domain = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(domain.Scenarios().size(), 9);
    for (size_t i : {0, 1, 2})
        ASSERT_EQ(domain.Risks()[i].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_DOMAIN_BOUNDARY"));
    r.valuation_.settings_.maxWorkUnits_ = 0;
    const auto failed = EvaluateEarlyExercisePdeRisk(r);
    for (const auto& risk : failed.Risks())
        ASSERT_EQ(risk.status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_BASE_FAILED"));
}

TEST(EarlyExercisePdeRiskTest, TestNonfinitePriceAndOriginalLcpFailuresNeverPublishValues) {
    namespace I = EarlyExercisePdeRiskInternal;
    I::TestControl_ seam;
    seam.nonfinitePrice_ = true;
    const auto nonfinite = I::Evaluate(RiskRequest(), seam);
    ASSERT_EQ(nonfinite.Status(), EarlyExercisePdeRiskStatus_("BASE_FAILED"));
    ASSERT_EQ(nonfinite.BaseResult().Status(), EarlyExercisePdeStatus_("NONFINITE"));
    ASSERT_THROW(static_cast<void>(nonfinite.BaseResult().Price()), Exception_);
    ASSERT_FALSE(nonfinite.Scenarios()[0].price_);
    auto r = RiskRequest();
    r.valuation_.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.valuation_.contract_.exerciseDates_.clear();
    r.valuation_.market_ = {80, 0, {{Date_(2027, 3, 4), .1, .1}, {Date_(2027, 10, 18), -.01, -.01}}};
    r.valuation_.settings_.ordinarySteps_ = 500;
    r.valuation_.settings_.penaltyFactor_ = 1;
    const auto failed = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(failed.BaseResult().Status(), EarlyExercisePdeStatus_("LCP_RESIDUAL_LIMIT"));
    ASSERT_EQ(failed.Status(), EarlyExercisePdeRiskStatus_("BASE_FAILED"));
    ASSERT_FALSE(failed.BaseResult().StepDiagnostics().empty());
    ASSERT_NO_FATAL_FAILURE(Counts(failed));
    r.valuation_.settings_.penaltyFactor_ = 1e8;
    r.valuation_.settings_.maxIterationsPerStep_ = 1;
    ASSERT_EQ(EvaluateEarlyExercisePdeRisk(r).BaseResult().Status(), EarlyExercisePdeStatus_("MAX_ITERATIONS"));
    r = RiskRequest();
    r.valuation_.settings_.linearBackwardErrorLimit_ = 1e-30;
    ASSERT_EQ(EvaluateEarlyExercisePdeRisk(r).BaseResult().Status(), EarlyExercisePdeStatus_("LINEAR_SOLVE_FAILURE"));
}

TEST(EarlyExercisePdeRiskTest, TestStrictValidationAlsoCoversDisabledFields) {
    for (int field = 0; field < 9; ++field) {
        auto r = RiskRequest();
        if (field < 5)
            InvalidSwitchAndBump(field, &r);
        else
            InvalidPrecision(field, &r);
        ASSERT_NO_FATAL_FAILURE(Error([&]() { ValidateEarlyExercisePdeRiskRequest(r); }, "InvalidEarlyExercisePdeRiskRequest", "risk."));
    }
    auto r = RiskRequest();
    r.valuation_.settings_.gridPoints_ = 4;
    ASSERT_NO_FATAL_FAILURE(Error([&]() { ValidateEarlyExercisePdeRiskRequest(r); }, "InvalidEarlyExercisePdeRiskRequest", "grid_points"));
    r = RiskRequest();
    r.risk_.maxScenarios_ = r.risk_.maxWorkUnits_ = r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = 0;
    ASSERT_NO_THROW(ValidateEarlyExercisePdeRiskRequest(r));
}

TEST(EarlyExercisePdeRiskTest, TestOwningSnapshotsDetachedCopiesPureGettersAndCaseSensitiveIds) {
    auto r = RiskRequest();
    r.valuation_.settings_.upper_.reset();
    auto result = EvaluateEarlyExercisePdeRisk(r);
    const auto planId = result.Plan().planId_;
    const double price = result.Price();
    const size_t work = result.Execution().workUnits_;
    r.valuation_.market_.carry_[0].rate_ = 10;
    r.risk_.rateBumps_ = {10};
    ASSERT_EQ(result.Request().valuation_.market_.carry_[0].rate_, .05);
    ASSERT_FALSE(result.Request().valuation_.settings_.upper_);
    ASSERT_TRUE(result.Request().risk_.rateBumps_.empty());
    ASSERT_EQ(result.Plan().resolvedRisk_.rateBumps_[0], 1e-4);
    ASSERT_DOUBLE_EQ(*result.BaseResult().Request().settings_.upper_, 400);
    auto copy = result;
    auto moved = std::move(copy);
    auto detached = moved.Risks();
    detached[0].reason_ = "detached";
    ASSERT_NE(moved.Risks()[0].reason_, detached[0].reason_);
    for (int read = 0; read < 100; ++read) {
        ASSERT_DOUBLE_EQ(moved.Price(), price);
        ASSERT_EQ(moved.Plan().planId_, planId);
        ASSERT_EQ(moved.Execution().workUnits_, work);
        ASSERT_EQ(moved.Scenarios().size(), 17);
    }
    ASSERT_NO_FATAL_FAILURE(Error([&]() { static_cast<void>(moved.RiskValue("delta")); }, "EarlyExercisePdeRiskUnknownRiskId", "Delta"));
    for (const auto& risk : moved.Risks()) {
        if (risk.value_)
            ASSERT_EQ(moved.RiskValue(risk.riskId_), risk.value_);
        else
            ASSERT_NO_FATAL_FAILURE(
                Error([&]() { static_cast<void>(moved.RiskValue(risk.riskId_)); }, "EarlyExercisePdeRiskValueUnavailable", risk.status_.String()));
    }
}

TEST(EarlyExercisePdeRiskTest, TestExistingRecordingAndReadyGraphSurviveSuccessAndFailures) {
    using namespace AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2);
    scope.StartRecording();
    Number_ square = x * x;
    const auto nodes = Tape()->nodes_.OccupiedSlots();
    const auto ders = Tape()->ders_.OccupiedSlots();
    const auto args = Tape()->argPtrs_.OccupiedSlots();
    const auto request = RiskRequest();
    ASSERT_TRUE(EvaluateEarlyExercisePdeRisk(request).PriceAvailable());
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    Number_ cube = square * x;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(cube, 3, 1);
    const auto finishedNodes = Tape()->nodes_.OccupiedSlots();
    for (int kind = 0; kind < 4; ++kind) {
        auto r = request;
        if (kind == 0) {
            r.risk_.spotBump_ = 1;
            ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Exception_);
        } else if (kind == 1) {
            r.risk_.maxScenarios_ = 0;
            ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Exception_);
        } else {
            if (kind == 2)
                r.risk_.maxWorkUnits_ = 0;
            const auto result = EvaluateEarlyExercisePdeRisk(r);
            ASSERT_EQ(result.PriceAvailable(), kind == 3);
        }
    }
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), finishedNodes);
    ASSERT_EQ(Tape()->ders_.OccupiedSlots(), ders + 2);
    ASSERT_EQ(Tape()->argPtrs_.OccupiedSlots(), args + 2);
    ASSERT_EQ(Tape()->numAdj_, 2);
    ASSERT_TRUE(Tape()->multi_);
    ASSERT_EQ(NativeOperations_::ReadAdjoint(cube, 1), 3);
    scope.Reverse();
    ASSERT_EQ(NativeOperations_::ReadAdjoint(x, 1), 36);
    scope.Close();
}

TEST(EarlyExercisePdeRiskTest, TestIndependentConcurrentEvaluationsMatchSerialResults) {
    const auto r = RiskRequest();
    auto a = std::async(std::launch::async, [r]() { return EvaluateEarlyExercisePdeRisk(r); });
    auto b = std::async(std::launch::async, [r]() { return EvaluateEarlyExercisePdeRisk(r); });
    const auto serial = EvaluateEarlyExercisePdeRisk(r), left = a.get(), right = b.get();
    for (const auto* result : {&left, &right}) {
        ASSERT_EQ(result->Price(), serial.Price());
        ASSERT_EQ(result->Plan().planId_, serial.Plan().planId_);
        ASSERT_EQ(result->Execution().workUnits_, serial.Execution().workUnits_);
        for (size_t i = 0; i < serial.Risks().size(); ++i) {
            ASSERT_EQ(result->Risks()[i].status_, serial.Risks()[i].status_);
            ASSERT_EQ(result->Risks()[i].value_, serial.Risks()[i].value_);
        }
    }
}

TEST(EarlyExercisePdeRiskTest, TestCompleteSmoothControlAndEqualityToStabilityTolerance) {
    auto r = RiskRequest();
    r.valuation_.settings_.gridPoints_ = 401;
    r.valuation_.settings_.ordinarySteps_ = 500;
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_TRUE(result.IsComplete());
    ASSERT_EQ(result.Status(), EarlyExercisePdeRiskStatus_("COMPLETE"));
    for (const auto& risk : result.Risks()) {
        ASSERT_TRUE(risk.value_);
        ASSERT_TRUE(std::isfinite(*risk.value_));
        ASSERT_EQ(result.RiskValue(risk.riskId_), risk.value_);
    }
    const double difference = *result.Risks()[0].normalizedStepDifference_;
    ASSERT_GT(difference, 0);
    r.risk_.deltaStabilityTolerance_ = difference;
    ASSERT_EQ(EvaluateEarlyExercisePdeRisk(r).Risks()[0].status_, EarlyExercisePdeRiskAvailability_("AVAILABLE"));
    r.risk_.deltaStabilityTolerance_ = std::nextafter(difference, 0.0);
    const auto unstable = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(unstable.Risks()[0].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_STEP_UNSTABLE"));
    ASSERT_FALSE(unstable.Risks()[0].value_);
    ASSERT_TRUE(unstable.Risks()[0].fineEstimate_);
    ASSERT_TRUE(unstable.Risks()[0].coarseEstimate_);
}

TEST(EarlyExercisePdeRiskTest, TestTwoCarryAxesOwnDatesAndUseActualCommonCnPlan) {
    auto r = RiskRequest();
    r.valuation_.contract_.exerciseDates_ = {Date_(2027, 4, 18), Date_(2027, 10, 18)};
    r.valuation_.market_.carry_ = {{Date_(2027, 3, 4), .02, .01}, {Date_(2027, 10, 18), .06, .03}};
    r.valuation_.market_.spot_ = 120;
    r.valuation_.market_.volatility_ = .8;
    r.valuation_.settings_.theta_ = .5;
    r.valuation_.settings_.ordinarySteps_ = 4;
    r.valuation_.settings_.rannacherIntervals_ = 1;
    r.risk_.volatilityBump_ = .5;
    r.risk_.rateBumps_ = {.001, .002};
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(result.Risks().size(), 7);
    ASSERT_EQ(result.Scenarios().size(), 25);
    ASSERT_GT(result.Plan().plannedSteps_, result.Plan().basePlannedSteps_);
    ASSERT_DOUBLE_EQ(result.Plan().epsilon_, 2e-8);
    ASSERT_EQ(result.Plan().rateBumpOrigins_[0], EarlyExercisePdeRiskBumpOrigin_("EXPLICIT"));
    ASSERT_EQ(result.Plan().dividendYieldBumpOrigins_[0], EarlyExercisePdeRiskBumpOrigin_("DEFAULT"));
    ASSERT_EQ(result.Plan().volatilityBumpOrigin_, EarlyExercisePdeRiskBumpOrigin_("EXPLICIT"));
    Vector_<EarlyExercisePdeMarket_> markets;
    for (size_t i = 1; i < result.Scenarios().size(); ++i)
        markets.push_back(ScenarioMarket(r.valuation_.market_, result.Scenarios()[i]));
    const auto plan = EarlyExercisePdeInternal::BuildPlan(r.valuation_, markets);
    ASSERT_EQ(plan.Id(), result.Plan().planId_);
    ASSERT_EQ(plan.Grid(), result.Plan().grid_);
    for (size_t i = 0; i < result.Scenarios().size(); ++i) {
        auto scenario = r.valuation_;
        if (i > 0)
            scenario.market_ = markets[i - 1];
        const auto independent = EarlyExercisePdeInternal::Rollback(scenario, plan);
        ASSERT_EQ(result.Scenarios()[i].price_, independent.Price());
        ASSERT_EQ(result.Scenarios()[i].workUnits_, independent.Execution().workUnits_);
    }
    ASSERT_NO_FATAL_FAILURE(CarryMetadata(result, r));
    ASSERT_EQ(result.Plan().timeGrid_.size(), result.Plan().steps_.size() + 1);
    for (size_t i = 0; i < result.Plan().steps_.size(); ++i) {
        const auto& step = result.Plan().steps_[i];
        ASSERT_EQ(step.timeIndex_, i);
        ASSERT_EQ(step.sourceTimeIndex_, i + 1);
        ASSERT_DOUBLE_EQ(step.dt_, result.Plan().timeGrid_[i + 1].timeYears_ - result.Plan().timeGrid_[i].timeYears_);
    }
}

TEST(EarlyExercisePdeRiskTest, TestClosedExerciseBoundaryBracketsAndDeepExerciseRegion) {
    auto r = RiskRequest();
    r.valuation_.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.valuation_.contract_.exerciseDates_.clear();
    r.valuation_.market_.carry_[0].dividendYield_ = 0;
    r.valuation_.settings_.gridPoints_ = 401;
    r.valuation_.settings_.ordinarySteps_ = 500;
    const auto control = EvaluateEarlyExercisePde(r.valuation_);
    ASSERT_TRUE(control.IsSuccess());
    ASSERT_EQ(control.ExerciseLevels().front().timeIndex_, 0);
    const auto& level = control.ExerciseLevels().front();
    ASSERT_GT(level.regionCount_, 0);
    const auto& region = control.ExerciseRegions()[level.regionOffset_];
    ASSERT_FALSE(region.rightClipped_);
    const double edge = control.Grid()[region.lastNode_], next = control.Grid()[region.lastNode_ + 1];
    for (double spot : {0.5 * (edge + next), edge - 4.0, next + 4.0}) {
        r.valuation_.market_.spot_ = spot;
        const auto result = EvaluateEarlyExercisePdeRisk(r);
        ASSERT_TRUE(result.PriceAvailable());
        ASSERT_TRUE(*result.Scenarios()[0].evaluationBoundaryIntersectsSpotStencil_);
        ASSERT_EQ(result.Risks()[0].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_NEAR_EXERCISE_BOUNDARY"));
        ASSERT_EQ(result.Risks()[1].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_NEAR_EXERCISE_BOUNDARY"));
        ASSERT_FALSE(result.Risks()[0].value_);
        ASSERT_TRUE(result.Risks()[0].fineEstimate_);
        if (spot == 0.5 * (edge + next)) {
            ASSERT_EQ(result.Scenarios()[0].evaluationMembership_, EarlyExercisePdeRiskMembership_("BOUNDARY"));
            for (size_t i : {2, 3, 4})
                ASSERT_EQ(result.Risks()[i].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_EXERCISE_SWITCH"));
        }
    }
    r.valuation_.market_.spot_ = 40;
    const auto deep = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_TRUE(deep.IsComplete());
    for (const auto& risk : deep.Risks())
        ASSERT_EQ(risk.status_, EarlyExercisePdeRiskAvailability_("AVAILABLE_EXERCISE_REGION"));
    ASSERT_NEAR(deep.RiskValue("Delta"), -1, 1e-8);
    ASSERT_NEAR(deep.RiskValue("Gamma"), 0, 1e-8);
}

namespace {
    struct SmallLcp_ {
        std::array<std::array<double, 3>, 3> matrix_{};
        std::array<double, 3> rhs_{110, 10, 0};
    };
    SmallLcp_ SmallLcp(double volatility) {
        const double dt = 1.0 / 365.0;
        SmallLcp_ system;
        for (int i = 0; i < 3; ++i) {
            const double node = i + 1, diffusion = .5 * volatility * volatility * node * node, drift = .03 * node;
            system.matrix_[i][i] = 1 + dt * (2 * diffusion + .1);
            if (i > 0)
                system.matrix_[i][i - 1] = -dt * (diffusion - drift);
            else
                system.rhs_[i] += dt * (diffusion - drift) * 210;
            if (i < 2)
                system.matrix_[i][i + 1] = -dt * (diffusion + drift);
        }
        return system;
    }
    using SmallDense_ = std::array<std::array<double, 4>, 3>;
    SmallDense_ ActiveSystem(const SmallLcp_& system, int mask) {
        const double payoff[3]{110, 10, 0};
        SmallDense_ a{};
        for (int i = 0; i < 3; ++i) {
            const bool exercise = mask & (1 << i);
            for (int j = 0; j < 3; ++j)
                a[i][j] = exercise ? double(i == j) : system.matrix_[i][j];
            a[i][3] = exercise ? payoff[i] : system.rhs_[i];
        }
        return a;
    }
    std::array<double, 3> SolveSmall(SmallDense_ a) {
        std::array<double, 3> value{};
        for (int i = 0; i < 3; ++i)
            for (int j = i + 1; j < 3; ++j) {
                const double factor = a[j][i] / a[i][i];
                for (int k = i; k < 4; ++k)
                    a[j][k] -= factor * a[i][k];
            }
        for (int i = 2; i >= 0; --i) {
            value[i] = a[i][3];
            for (int j = i + 1; j < 3; ++j)
                value[i] -= a[i][j] * value[j];
            value[i] /= a[i][i];
        }
        return value;
    }
    bool Feasible(const SmallLcp_& system, const std::array<double, 3>& value) {
        const double payoff[3]{110, 10, 0};
        bool valid = true;
        for (int i = 0; i < 3; ++i) {
            double dual = -system.rhs_[i];
            for (int j = 0; j < 3; ++j)
                dual += system.matrix_[i][j] * value[j];
            valid = valid && value[i] >= payoff[i] - 1e-10 && dual >= -1e-10 && std::abs(std::min(value[i] - payoff[i], dual)) < 1e-10;
        }
        return valid;
    }
    double SmallLcpPrice(double volatility) {
        const auto system = SmallLcp(volatility);
        for (int mask = 0; mask < 8; ++mask) {
            const auto value = SolveSmall(ActiveSystem(system, mask));
            if (Feasible(system, value))
                return value[0];
        }
        throw std::runtime_error("small independent LCP has no feasible active set");
    }
} // namespace

TEST(EarlyExercisePdeRiskTest, TestEverySmallAmericanScenarioAgainstIndependentActiveSets) {
    auto r = RiskRequest();
    r.valuation_.contract_.strike_ = 210;
    r.valuation_.contract_.maturityDate_ = Date_(2026, 10, 19);
    r.valuation_.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    r.valuation_.contract_.exerciseDates_.clear();
    r.valuation_.market_.volatility_ = .3;
    r.valuation_.market_.carry_ = {{r.valuation_.contract_.maturityDate_, .1, .04}};
    r.valuation_.settings_.gridPoints_ = 5;
    r.valuation_.settings_.ordinarySteps_ = 1;
    r.risk_.delta_ = r.risk_.gamma_ = r.risk_.carryRisk_ = false;
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_EQ(result.Scenarios().size(), 5);
    ASSERT_TRUE(result.IsComplete());
    for (const auto& row : result.Scenarios()) {
        const double volatility = row.parameterValue_.value_or(.3);
        ASSERT_TRUE(row.priceAvailable_);
        ASSERT_NEAR(*row.price_, SmallLcpPrice(volatility), result.Plan().epsilon_ + 1e-8);
        ASSERT_LE(*row.maxComplementarityResidual_, result.Plan().epsilon_);
        ASSERT_LE(*row.maxLinearBackwardError_, r.valuation_.settings_.linearBackwardErrorLimit_);
        ASSERT_EQ(row.evaluationMembership_, EarlyExercisePdeRiskMembership_("EXERCISE"));
    }
}

TEST(EarlyExercisePdeRiskTest, TestNonfiniteDifferenceIsUnavailableWithSuccessfulScenarios) {
    auto r = RiskRequest();
    r.valuation_.market_.carry_[0].rate_ = 0;
    r.risk_.rateBumps_ = {1e-320};
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_TRUE(result.PriceAvailable());
    ASSERT_EQ(result.Risks()[3].status_, EarlyExercisePdeRiskAvailability_("UNAVAILABLE_NONFINITE"));
    ASSERT_FALSE(result.Risks()[3].value_);
    for (const auto& row : result.Scenarios())
        ASSERT_TRUE(row.priceAvailable_);
}

TEST(EarlyExercisePdeRiskTest, TestUpwindChoicesAreDiagnosticsAndDoNotAutomaticallyRejectRisk) {
    auto r = RiskRequest();
    r.valuation_.market_.volatility_ = std::sqrt(.05);
    r.valuation_.market_.carry_[0].dividendYield_ = 0;
    r.valuation_.settings_.gridPoints_ = 401;
    r.valuation_.settings_.ordinarySteps_ = 500;
    const auto result = EvaluateEarlyExercisePdeRisk(r);
    ASSERT_TRUE(result.IsComplete());
    ASSERT_FALSE(*result.Scenarios()[0].upwindChanged_);
    ASSERT_FALSE(*result.Scenarios()[0].futureContactChanged_);
    bool changed = false;
    for (size_t i : {5, 6, 7, 8}) {
        changed = changed || *result.Scenarios()[i].upwindChanged_;
        ASSERT_FALSE(*result.Scenarios()[i].futureContactChanged_);
    }
    ASSERT_TRUE(changed);
    ASSERT_EQ(result.Risks()[2].status_, EarlyExercisePdeRiskAvailability_("AVAILABLE"));
}
