#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/pde/earlyexercisepde.hpp>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    EarlyExercisePdeRequest_ Request(const char* style = "BERMUDAN") {
        EarlyExercisePdeRequest_ r;
        r.contract_ = {Date_(2026, 10, 18), Date_(2027, 10, 18), 100.0, OptionType_("PUT"), EarlyExerciseStyle_(style), {}};
        if (r.contract_.exerciseStyle_ == EarlyExerciseStyle_::Value_::BERMUDAN)
            r.contract_.exerciseDates_ = {Date_(2027, 3, 4), r.contract_.maturityDate_};
        r.market_ = {80.0, 0.0, {{r.contract_.maturityDate_, 0.1, 0.1}}};
        r.settings_.ordinarySteps_ = 6000;
        r.settings_.gridPoints_ = 41;
        return r;
    }
} // namespace

TEST(EarlyExercisePdeTest, TestIrregularDateProjectionHasIndependentDeterministicPrice) {
    const auto result = EvaluateEarlyExercisePde(Request());
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_NEAR(result.Price(), 20.0 * std::exp(-0.1 * 137.0 / 365.0), 1e-4);
    ASSERT_EQ(result.ExerciseLevels().size(), 2);
    const auto& level = result.ExerciseLevels().front();
    ASSERT_EQ(*level.calendarDate_, Date_(2027, 3, 4));
    ASSERT_DOUBLE_EQ(result.TimeGrid()[level.timeIndex_].timeYears_, 137.0 / 365.0);
    ASSERT_FALSE(result.TimeGrid().front().isExerciseDate_);
    ASSERT_EQ(level.regionCount_, 1);
    const auto& region = result.ExerciseRegions()[level.regionOffset_];
    ASSERT_EQ(region.firstNode_, 0);
    ASSERT_EQ(region.lastNode_, 9);
    ASSERT_TRUE(region.leftClipped_);
    ASSERT_FALSE(region.rightClipped_);
    ASSERT_DOUBLE_EQ(result.Grid()[region.lastNode_], 90.0);
    ASSERT_DOUBLE_EQ(result.Grid()[region.lastNode_ + 1], 100.0);
}

TEST(EarlyExercisePdeTest, TestAmericanEveryActualLayerIsAnExerciseOpportunity) {
    auto r = Request("AMERICAN");
    r.settings_.ordinarySteps_ = 500;
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_NEAR(result.Price(), 20.0, 1e-6);
    for (const auto& time : result.TimeGrid())
        ASSERT_TRUE(time.isExerciseDate_);
}

TEST(EarlyExercisePdeTest, TestCarryOrderAndMaturityOnlyHaveDistinctAnalyticValues) {
    auto r = Request();
    const auto mid = r.contract_.exerciseDates_.front();
    r.market_.carry_ = {{mid, -0.2, -0.2}, {r.contract_.maturityDate_, 0.2, 0.2}};
    for (const char* style : {"BERMUDAN", "AMERICAN"}) {
        auto x = r;
        x.contract_.exerciseStyle_ = EarlyExerciseStyle_(style);
        if (x.contract_.exerciseStyle_ == EarlyExerciseStyle_::Value_::AMERICAN)
            x.contract_.exerciseDates_.clear();
        const auto result = EvaluateEarlyExercisePde(x);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_NEAR(result.Price(), 20.0 * std::exp(0.2 * 137.0 / 365.0), 1e-4);
        const auto it = std::find_if(result.TimeGrid().begin(), result.TimeGrid().end(), [&](const auto& t) { return t.calendarDate_ == mid; });
        ASSERT_NE(it, result.TimeGrid().end());
        ASSERT_TRUE(it->isCarryKnot_);
    }
    r = Request();
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    const auto european = EvaluateEarlyExercisePde(r);
    ASSERT_NEAR(european.Price(), 20.0 * std::exp(-0.1), 1e-4);
    ASSERT_EQ(european.ExerciseLevels().size(), 1);
    ASSERT_FALSE(european.Execution().maxComplementarityResidual_);
}

TEST(EarlyExercisePdeTest, TestPenaltyFailureUsesOriginalLcpAndNeverExposesPartialPrice) {
    auto r = Request("AMERICAN");
    r.settings_.ordinarySteps_ = 500;
    r.settings_.penaltyFactor_ = 1.0;
    const auto knot = Date_(2027, 3, 4);
    r.market_.carry_ = {{knot, 0.1, 0.1}, {r.contract_.maturityDate_, -0.01, -0.01}};
    const auto failed = EvaluateEarlyExercisePde(r);
    ASSERT_EQ(failed.Status(), EarlyExercisePdeStatus_("LCP_RESIDUAL_LIMIT"));
    ASSERT_THROW(failed.Price(), Exception_);
    ASSERT_THROW(failed.Values(), Exception_);
    ASSERT_GT(*failed.StepDiagnostics().front().complementarityResidual_, failed.Execution().epsilon_);
    ASSERT_LT(*failed.StepDiagnostics().front().linearBackwardError_, 1e-12);
    ASSERT_FALSE(failed.StepDiagnostics().front().completed_);
    ASSERT_EQ(failed.ExerciseLevels().size(), failed.Execution().completedSteps_ + 1);
    const double dt = failed.StepDiagnostics().front().dt_;
    const double payoff = 100.0 - failed.Grid()[1];
    double source = payoff;
    for (int i = static_cast<int>(failed.TimeGrid().size()) - 2; i > failed.Failure()->timeIndex_; --i) {
        const auto& times = failed.TimeGrid();
        const double rate = times[i + 1].timeYears_ > 137.0 / 365.0 ? -0.01 : 0.1;
        source /= 1.0 + rate * (times[i + 1].timeYears_ - times[i].timeYears_);
    }
    const double gap = (payoff - source / (1.0 + 0.1 * dt)) / 2.0;
    ASSERT_NEAR(*failed.StepDiagnostics().front().obstacleViolation_, gap, 1e-10);
    r.settings_.penaltyFactor_ = 1e8;
    const auto passed = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(passed.IsSuccess());
    ASSERT_LE(*passed.Execution().maxComplementarityResidual_, passed.Execution().epsilon_);
    r.settings_.maxIterationsPerStep_ = 1;
    ASSERT_EQ(EvaluateEarlyExercisePde(r).Status(), EarlyExercisePdeStatus_("MAX_ITERATIONS"));
}

TEST(EarlyExercisePdeTest, TestBudgetEnvelopeExactBoundaryAndWorkLimit) {
    auto r = Request("AMERICAN");
    r.market_ = {100.0, 0.5, {{r.contract_.maturityDate_, 0.05, 0.0}}};
    r.settings_ = {};
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(result.IsSuccess());
    const auto e = result.Execution();
    ASSERT_EQ(e.plannedSteps_, 500);
    ASSERT_EQ(e.plannedRegionCapacity_, 100500);
    ASSERT_EQ(e.plannedNumericPayloadBytes_, 3418672);
    ASSERT_EQ(e.plannedWorkspaceBytes_, 138880);
    ASSERT_EQ(e.regionCapacity_, e.plannedRegionCapacity_);
    ASSERT_LE(e.numericPayloadBytes_, e.plannedNumericPayloadBytes_);
    ASSERT_LE(e.peakWorkspaceBytes_, e.plannedWorkspaceBytes_);
    ASSERT_GE(e.numericPayloadBytes_,
              e.regionCapacity_ * sizeof(EarlyExercisePdeExerciseRegion_) + size_t(500) * sizeof(EarlyExercisePdeStepDiagnostic_) + size_t(401) * 16);
    ASSERT_GE(e.peakWorkspaceBytes_, size_t(401) * 10 * sizeof(double) + size_t(500) * 24);
    r.settings_.numericPayloadBudgetBytes_ = e.plannedNumericPayloadBytes_;
    r.settings_.workspaceBudgetBytes_ = e.plannedWorkspaceBytes_;
    ASSERT_TRUE(EvaluateEarlyExercisePde(r).IsSuccess());
    --r.settings_.numericPayloadBudgetBytes_;
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    ++r.settings_.numericPayloadBudgetBytes_;
    --r.settings_.workspaceBudgetBytes_;
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r.settings_ = {};
    r.settings_.maxActualSteps_ = 499;
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r.settings_ = {};
    r.settings_.maxWorkUnits_ = 399;
    const auto work = EvaluateEarlyExercisePde(r);
    ASSERT_EQ(work.Status(), EarlyExercisePdeStatus_("WORK_LIMIT"));
    ASSERT_EQ(work.Execution().workUnits_, 399);
    ASSERT_THROW(work.Price(), Exception_);
    ASSERT_THROW(work.Values(), Exception_);
    r.settings_ = {};
    r.settings_.theta_ = 0.5;
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r.settings_.numericPayloadBudgetBytes_ = 217277488;
    r.settings_.maxWorkUnits_ = 1271852400;
    const auto cn = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(cn.IsSuccess());
    ASSERT_EQ(cn.Execution().plannedSteps_, 31876);
    ASSERT_EQ(cn.Execution().plannedNumericPayloadBytes_, 217277488);
    r.settings_ = {};
    r.settings_.gridPoints_ = 801;
    r.market_.volatility_ = 0.2;
    const auto refined = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(refined.IsSuccess());
    ASSERT_EQ(refined.Execution().plannedRegionCapacity_, 200500);
    ASSERT_EQ(refined.Execution().plannedNumericPayloadBytes_, 6625072);
    ASSERT_EQ(refined.Execution().plannedWorkspaceBytes_, 241280);
}

TEST(EarlyExercisePdeTest, TestNegativeRateSplitsWithoutCreatingBermudanEvents) {
    auto r = Request();
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    r.market_.carry_ = {{r.contract_.maturityDate_, -2.0, -2.0}};
    r.settings_.ordinarySteps_ = 1;
    r.settings_.rannacherIntervals_ = 0;
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_EQ(result.Execution().plannedSteps_, 4);
    ASSERT_EQ(result.Execution().stabilityAddedSteps_, 3);
    ASSERT_EQ(result.ExerciseLevels().size(), 1);
    for (size_t i = 0; i + 1 < result.TimeGrid().size(); ++i)
        ASSERT_FALSE(result.TimeGrid()[i].isExerciseDate_);
}

TEST(EarlyExercisePdeTest, TestAmericanBoundaryIncludesInteriorStationaryPoint) {
    auto r = Request("AMERICAN");
    r.contract_.optionType_ = OptionType_("CALL");
    r.contract_.maturityDate_ = Date_(2036, 10, 15);
    r.market_ = {100.0, 0.0, {{r.contract_.maturityDate_, -0.2, -0.1}}};
    r.settings_.ordinarySteps_ = 1000;
    r.settings_.upper_ = 400.0;
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_NEAR(result.Values().back(), 400.0, 1e-10);
    ASSERT_GT(result.Execution().upwindRows_, 0);
    r.contract_.optionType_ = OptionType_("PUT");
    const auto put = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(put.IsSuccess());
    ASSERT_NEAR(put.Values().front(), 100.0 * std::exp(2.0), 1e-8);
}

namespace {
    double Analytic(const EarlyExercisePdeRequest_& r) {
        double rate = 0.0, yield = 0.0;
        auto start = r.contract_.evaluationDate_;
        for (const auto& x : r.market_.carry_) {
            const double dt = (x.endDate_ - start) / 365.0;
            rate += x.rate_ * dt;
            yield += x.dividendYield_ * dt;
            start = x.endDate_;
        }
        const double sd = r.market_.volatility_ * std::sqrt((r.contract_.maturityDate_ - r.contract_.evaluationDate_) / 365.0);
        const double a = r.market_.spot_ * std::exp(-yield), b = r.contract_.strike_ * std::exp(-rate);
        if (sd < 1e-12)
            return std::max(r.contract_.optionType_ == OptionType_::Value_::CALL ? a - b : b - a, 0.0);
        const double d1 = std::log(a / b) / sd + 0.5 * sd, d2 = d1 - sd;
        const auto normal = [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); };
        return r.contract_.optionType_ == OptionType_::Value_::CALL ? a * normal(d1) - b * normal(d2) : b * normal(-d2) - a * normal(-d1);
    }
    double Crr(const EarlyExercisePdeRequest_& r, int steps, bool american, const Vector_<int>& exercise = {}) {
        const double dt = (r.contract_.maturityDate_ - r.contract_.evaluationDate_) / (365.0 * steps);
        const double u = std::exp(r.market_.volatility_ * std::sqrt(dt));
        const auto& carry = r.market_.carry_.front();
        const double probability = (std::exp((carry.rate_ - carry.dividendYield_) * dt) - 1.0 / u) / (u - 1.0 / u);
        if (!(probability >= 0.0 && probability <= 1.0))
            throw std::runtime_error("invalid CRR probability");
        const double discount = std::exp(-carry.rate_ * dt);
        const auto payoff = [&](double spot) {
            const double x = spot - r.contract_.strike_;
            return std::max(r.contract_.optionType_ == OptionType_::Value_::CALL ? x : -x, 0.0);
        };
        std::vector<double> values(steps + 1);
        for (int i = 0; i <= steps; ++i)
            values[i] = payoff(r.market_.spot_ * std::pow(u, 2 * i - steps));
        for (int step = steps - 1; step >= 0; --step) {
            const bool project = american || std::find(exercise.begin(), exercise.end(), step) != exercise.end();
            double spot = r.market_.spot_ * std::pow(u, -step);
            for (int i = 0; i <= step; ++i) {
                values[i] = discount * ((1.0 - probability) * values[i] + probability * values[i + 1]);
                if (project)
                    values[i] = std::max(values[i], payoff(spot));
                spot *= u * u;
            }
        }
        return values[0];
    }
} // namespace

TEST(EarlyExercisePdeTest, TestStochasticPutAndYieldCallMatchIndependentConvergedTrees) {
    for (const char* option : {"PUT", "CALL"}) {
        auto r = Request("AMERICAN");
        r.contract_.optionType_ = OptionType_(option);
        r.market_ = {100.0, 0.2, {{r.contract_.maturityDate_, 0.05, option == std::string("PUT") ? 0.0 : 0.10}}};
        r.settings_ = {};
        r.settings_.gridPoints_ = 801;
        r.settings_.ordinarySteps_ = 1500;
        const double coarse = Crr(r, 2048, true), even = Crr(r, 4096, true), odd = Crr(r, 4097, true);
        ASSERT_LT(std::max({coarse, even, odd}) - std::min({coarse, even, odd}), 0.003);
        ASSERT_GT(even - Analytic(r), 0.05);
        const auto result = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_NEAR(result.Price(), even, 0.01);
        ASSERT_NEAR(result.Price(), odd, 0.01);
    }
    auto b = Request();
    b.market_ = {100.0, 0.2, {{b.contract_.maturityDate_, 0.05, 0.0}}};
    b.settings_.gridPoints_ = 801;
    b.settings_.ordinarySteps_ = 2000;
    const double tree1 = Crr(b, 2920, false, {137 * 8}), tree2 = Crr(b, 5840, false, {137 * 16});
    ASSERT_NEAR(tree1, tree2, 0.003);
    ASSERT_NEAR(EvaluateEarlyExercisePde(b).Price(), tree2, 0.01);
}

TEST(EarlyExercisePdeTest, TestEuropeanDegenerationNearZeroAndSeparatedConvergence) {
    auto r = Request();
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    r.market_ = {103.7, 0.2, {{Date_(2027, 3, 4), -0.01, 0.02}, {r.contract_.maturityDate_, 0.05, 0.01}}};
    r.settings_.theta_ = 0.5;
    r.settings_.ordinarySteps_ = 8000;
    for (double strike : {100.0, 101.3}) {
        r.contract_.strike_ = strike;
        double previous = 1.0;
        for (int nodes : {201, 401, 801}) {
            r.settings_.gridPoints_ = nodes;
            r.settings_.upper_ = 400.0;
            const auto result = EvaluateEarlyExercisePde(r);
            ASSERT_TRUE(result.IsSuccess()) << "N=" << nodes << " failure=" << result.Failure()->reason_.c_str()
                                            << " residual=" << result.Failure()->residual_.value_or(-1.0) << " time=" << result.Failure()->timeIndex_;
            const double error = std::abs(result.Price() - Analytic(r));
            ASSERT_LT(error, previous);
            previous = error;
        }
        ASSERT_LT(previous, 0.005);
    }
    r.contract_.strike_ = 100.0;
    r.settings_.theta_ = 1.0;
    double previous = 1.0;
    for (int steps : {500, 1000, 2000}) {
        r.settings_.ordinarySteps_ = steps;
        const double error = std::abs(EvaluateEarlyExercisePde(r).Price() - Analytic(r));
        ASSERT_LT(error, previous);
        previous = error;
    }
    ASSERT_LT(previous, 0.005);
    r.market_ = {80.0, 1e-10, {{r.contract_.maturityDate_, 0.1, 0.1}}};
    r.settings_.ordinarySteps_ = 6000;
    ASSERT_NEAR(EvaluateEarlyExercisePde(r).Price(), Analytic(r), 1e-4);
}

TEST(EarlyExercisePdeTest, TestNestedSchedulesShareGridAndTruncationIsSeparate) {
    auto r = Request();
    const auto middle = r.contract_.exerciseDates_.front();
    r.market_ = {100.0, 0.2, {{middle, 0.05, 0.0}, {r.contract_.maturityDate_, 0.05, 0.0}}};
    r.settings_.ordinarySteps_ = 1000;
    r.settings_.gridPoints_ = 401;
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    const auto euro = EvaluateEarlyExercisePde(r);
    r.contract_.exerciseDates_ = {middle, r.contract_.maturityDate_};
    const auto bermudan = EvaluateEarlyExercisePde(r);
    r.contract_.exerciseDates_ = {r.contract_.evaluationDate_, middle, r.contract_.maturityDate_};
    const auto today = EvaluateEarlyExercisePde(r);
    r.contract_.exerciseDates_.clear();
    r.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
    const auto american = EvaluateEarlyExercisePde(r);
    ASSERT_EQ(euro.TimeGrid().size(), american.TimeGrid().size());
    for (size_t i = 0; i < euro.TimeGrid().size(); ++i)
        ASSERT_DOUBLE_EQ(euro.TimeGrid()[i].timeYears_, american.TimeGrid()[i].timeYears_);
    ASSERT_GE(bermudan.Price(), euro.Price());
    ASSERT_GE(today.Price(), bermudan.Price());
    ASSERT_GE(american.Price() + 1e-5, today.Price());
    double previous = american.Price();
    for (int upper : {600, 800}) {
        r.settings_.upper_ = upper;
        r.settings_.gridPoints_ = upper + 1;
        const auto extended = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(extended.IsSuccess());
        ASSERT_NEAR(extended.Price(), previous, 0.005);
        ASSERT_EQ(extended.ExerciseRegions().back().firstNode_, american.ExerciseRegions().back().firstNode_);
        previous = extended.Price();
    }
}

TEST(EarlyExercisePdeTest, TestRejectsInvalidDatesTypesDomainsAndOwnsSnapshot) {
    auto r = Request();
    const auto result = EvaluateEarlyExercisePde(r);
    r.market_.spot_ = 5.0;
    r.contract_.exerciseDates_.clear();
    ASSERT_DOUBLE_EQ(result.Request().market_.spot_, 80.0);
    ASSERT_EQ(result.Request().contract_.exerciseDates_.size(), 2);
    ASSERT_TRUE(result.Request().settings_.upper_);
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r = Request();
    r.contract_.exerciseDates_.push_back(r.contract_.maturityDate_);
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r = Request();
    r.market_.carry_.back().endDate_ = Date_(2027, 10, 17);
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r = Request();
    r.contract_.evaluationDate_ = Date_();
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r = Request();
    r.contract_.optionType_ = OptionType_("STRADDLE");
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
    r = Request();
    r.market_.volatility_ = std::numeric_limits<double>::infinity();
    ASSERT_THROW(EvaluateEarlyExercisePde(r), Exception_);
}

TEST(EarlyExercisePdeTest, TestLeapDayShortExpiryAndBothDriftDirectionsAtZeroVolatility) {
    auto r = Request();
    r.contract_.evaluationDate_ = Date_(2028, 2, 28);
    r.contract_.maturityDate_ = Date_(2028, 3, 1);
    r.contract_.exerciseDates_ = {Date_(2028, 2, 29), r.contract_.maturityDate_};
    r.market_.carry_ = {{r.contract_.maturityDate_, 0.1, 0.1}};
    r.settings_.ordinarySteps_ = 500;
    const auto leap = EvaluateEarlyExercisePde(r);
    ASSERT_NEAR(leap.Price(), 20.0 * std::exp(-0.1 / 365.0), 1e-6);
    ASSERT_DOUBLE_EQ(leap.TimeGrid().back().timeYears_, 2.0 / 365.0);
    r.contract_.exerciseDates_ = {r.contract_.maturityDate_};
    r.settings_.gridPoints_ = 801;
    r.settings_.upper_ = 400.0;
    for (double rate : {-0.05, 0.05}) {
        r.market_.carry_ = {{r.contract_.maturityDate_, rate, 0.0}};
        const auto result = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_GT(result.Execution().upwindRows_, 0);
        ASSERT_NEAR(result.Price(), Analytic(r), 1e-5);
    }
}

TEST(EarlyExercisePdeTest, TestAmericanSeparatedSpaceAndTimeConvergenceAndNoYieldCall) {
    auto r = Request("AMERICAN");
    r.market_ = {100.0, 0.2, {{r.contract_.maturityDate_, 0.05, 0.0}}};
    r.settings_ = {};
    r.settings_.upper_ = 400.0;
    const double reference = 0.5 * (Crr(r, 4096, true) + Crr(r, 4097, true));
    r.settings_.theta_ = 0.5;
    r.settings_.ordinarySteps_ = 5000;
    double previousError = 1.0;
    for (int nodes : {101, 201, 401}) {
        r.settings_.gridPoints_ = nodes;
        const auto result = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(result.IsSuccess());
        ASSERT_EQ(result.Execution().plannedSteps_, 5002);
        const double error = std::abs(result.Price() - reference);
        ASSERT_LT(error, previousError);
        previousError = error;
    }
    ASSERT_LT(previousError, 0.005);
    r.settings_.theta_ = 1.0;
    r.settings_.gridPoints_ = 801;
    previousError = 1.0;
    for (int steps : {500, 1000, 2000}) {
        r.settings_.ordinarySteps_ = steps;
        const auto result = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(result.IsSuccess());
        const double error = std::abs(result.Price() - reference);
        ASSERT_LT(error, previousError);
        previousError = error;
    }
    ASSERT_LT(previousError, 0.005);
    r.contract_.optionType_ = OptionType_("CALL");
    const auto call = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(call.IsSuccess());
    ASSERT_NEAR(call.Price(), Analytic(r), 0.01);
    for (const auto& level : call.ExerciseLevels())
        ASSERT_EQ(level.regionCount_, 0);
}

TEST(EarlyExercisePdeTest, TestLinearAccuracyFailurePreservesMeasuredMaximumAndContext) {
    auto r = Request("AMERICAN");
    r.market_.volatility_ = 0.2;
    r.settings_.ordinarySteps_ = 500;
    r.settings_.linearBackwardErrorLimit_ = 1e-30;
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_EQ(result.Status(), EarlyExercisePdeStatus_("LINEAR_SOLVE_FAILURE"));
    ASSERT_GT(*result.Execution().maxLinearBackwardError_, r.settings_.linearBackwardErrorLimit_);
    ASSERT_EQ(result.Execution().maxLinearBackwardError_, result.Failure()->residual_);
    ASSERT_FALSE(result.StepDiagnostics().front().completed_);
    ASSERT_EQ(result.Execution().workUnits_, static_cast<size_t>(r.settings_.gridPoints_ - 2));
    ASSERT_THROW(static_cast<void>(result.Price()), Exception_);
}

TEST(EarlyExercisePdeTest, TestHighVolatilityDeepMoneynessAndShortExpiryHaveIndependentReferences) {
    for (double spot : {60.0, 140.0}) {
        auto r = Request("AMERICAN");
        r.market_ = {spot, 0.5, {{r.contract_.maturityDate_, 0.05, 0.0}}};
        r.settings_ = {};
        r.settings_.gridPoints_ = 801;
        r.settings_.ordinarySteps_ = 2000;
        r.settings_.upper_ = 400.0;
        const double even = Crr(r, 8192, true), odd = Crr(r, 8193, true);
        ASSERT_NEAR(even, odd, 0.003);
        const auto base = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(base.IsSuccess());
        ASSERT_NEAR(base.Price(), even, 0.01);
        r.settings_.upper_ = 800.0;
        r.settings_.gridPoints_ = 1601;
        const auto extended = EvaluateEarlyExercisePde(r);
        ASSERT_TRUE(extended.IsSuccess());
        ASSERT_NEAR(extended.Price(), base.Price(), 0.005);
    }
    auto r = Request("AMERICAN");
    r.contract_.maturityDate_ = Date_(2026, 10, 25);
    r.contract_.optionType_ = OptionType_("CALL");
    r.market_ = {100.0, 0.5, {{r.contract_.maturityDate_, 0.05, 0.1}}};
    r.settings_.gridPoints_ = 801;
    r.settings_.ordinarySteps_ = 2000;
    const double even = Crr(r, 4096, true), odd = Crr(r, 4097, true);
    ASSERT_NEAR(even, odd, 0.003);
    const auto result = EvaluateEarlyExercisePde(r);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_NEAR(result.Price(), even, 0.01);
}

TEST(EarlyExercisePdeTest, TestInvalidScalarReportsFieldValueAndConstraint) {
    auto r = Request();
    r.contract_.strike_ = -1.0;
    try {
        static_cast<void>(EvaluateEarlyExercisePde(r));
        FAIL() << "expected invalid strike";
    } catch (const Exception_& error) {
        const std::string text(error.what());
        ASSERT_NE(text.find("InvalidEarlyExercisePdeRequest"), std::string::npos);
        ASSERT_NE(text.find("contract.strike"), std::string::npos);
        ASSERT_NE(text.find("-1"), std::string::npos);
        ASSERT_NE(text.find("positive"), std::string::npos);
    }
}
