#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include <dal/math/pde/earlyexercisepdeinternal.hpp>
#include <dal/math/pde/earlyexercisepderisk.hpp>

namespace Dal::PdeRiskBench {
    namespace I = PDE::EarlyExercisePdeInternal;
    using namespace PDE;

    inline double BoundarySpot(const EarlyExercisePdeRequest_& request) {
        const auto price = EvaluateEarlyExercisePde(request);
        REQUIRE(price.IsSuccess() && !price.ExerciseLevels().empty(), "boundary fixture price failed");
        const auto& level = price.ExerciseLevels().front();
        REQUIRE(level.timeIndex_ == 0 && level.regionCount_ > 0, "boundary fixture has no evaluation contact");
        const auto& region = price.ExerciseRegions()[level.regionOffset_];
        REQUIRE(!region.rightClipped_, "boundary fixture edge is clipped");
        return .5 * (price.Grid()[region.lastNode_] + price.Grid()[region.lastNode_ + 1]);
    }

    inline EarlyExercisePdeRiskRequest_ Request(const std::string& name) {
        REQUIRE(name == "Bermudan17" || name == "American17" || name == "TwoCarry25" || name == "Boundary17", "unknown PDE risk case");
        EarlyExercisePdeRiskRequest_ r;
        const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
        r.valuation_.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {Date_(2027, 3, 4), maturity}};
        r.valuation_.market_ = {100.0, .2, {{maturity, .05, 0.0}}};
        r.valuation_.settings_.upper_ = 400.0;
        r.valuation_.settings_.maxActualSteps_ = 200000;
        r.valuation_.settings_.maxWorkUnits_ = 1000000000;
        r.valuation_.settings_.numericPayloadBudgetBytes_ = r.valuation_.settings_.workspaceBudgetBytes_ = 1073741824;
        r.risk_.maxWorkUnits_ = 10000000000ULL;
        r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = 2147483648ULL;
        if (name == "TwoCarry25") {
            r.valuation_.contract_.exerciseDates_ = {Date_(2027, 4, 18), maturity};
            r.valuation_.market_.carry_ = {{Date_(2027, 3, 4), .02, .01}, {maturity, .06, .03}};
        } else if (name != "Bermudan17") {
            r.valuation_.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
            r.valuation_.contract_.exerciseDates_.clear();
        }
        if (name == "Boundary17")
            r.valuation_.market_.spot_ = BoundarySpot(r.valuation_);
        return r;
    }

    struct Reference_ {
        EarlyExercisePdeRiskRequest_ request_;
        std::optional<I::Plan_> plan_;
        std::optional<EarlyExercisePdeResult_> base_;
        Vector_<EarlyExercisePdeRiskScenario_> scenarios_;
        size_t work_ = 0, solves_ = 0;
    };

    inline bool EvaluationExercise(const EarlyExercisePdeResult_& price) {
        return !price.ExerciseLevels().empty() && price.ExerciseLevels().front().timeIndex_ == 0;
    }

    inline bool Contains(const EarlyExercisePdeExerciseRegion_& run, int node) { return node >= run.firstNode_ && node <= run.lastNode_; }

    inline EarlyExercisePdeRiskMembership_ ContactMembership(const EarlyExercisePdeResult_& price, int lo, int hi) {
        bool contact = false;
        const auto& level = price.ExerciseLevels().front();
        for (size_t j = 0; j < level.regionCount_; ++j) {
            const auto& run = price.ExerciseRegions()[level.regionOffset_ + j];
            if (lo >= run.firstNode_ && hi <= run.lastNode_)
                return EarlyExercisePdeRiskMembership_("EXERCISE");
            contact = contact || Contains(run, lo) || Contains(run, hi);
        }
        return EarlyExercisePdeRiskMembership_(contact ? "BOUNDARY" : "CONTINUATION");
    }

    inline EarlyExercisePdeRiskMembership_ Membership(const EarlyExercisePdeResult_& price) {
        if (!EvaluationExercise(price))
            return EarlyExercisePdeRiskMembership_("NOT_EXERCISABLE");
        const double spot = price.Request().market_.spot_;
        const int hi = static_cast<int>(std::lower_bound(price.Grid().begin(), price.Grid().end(), spot) - price.Grid().begin());
        const int lo = price.Grid()[hi] == spot ? hi : hi - 1;
        return ContactMembership(price, lo, hi);
    }

    inline bool RegionBoundary(const EarlyExercisePdeExerciseRegion_& run, const Vector_<double>& grid, double lo, double hi) {
        const bool left = !run.leftClipped_ && grid[run.firstNode_] >= lo && grid[run.firstNode_ - 1] <= hi;
        const bool right = !run.rightClipped_ && grid[run.lastNode_ + 1] >= lo && grid[run.lastNode_] <= hi;
        return left || right;
    }

    inline bool Boundary(const EarlyExercisePdeResult_& price, double lo, double hi) {
        if (!EvaluationExercise(price))
            return false;
        const auto& level = price.ExerciseLevels().front();
        const auto& grid = price.Grid();
        for (size_t j = 0; j < level.regionCount_; ++j) {
            const auto& run = price.ExerciseRegions()[level.regionOffset_ + j];
            if (RegionBoundary(run, grid, lo, hi))
                return true;
        }
        return false;
    }

    inline bool LevelChanged(const EarlyExercisePdeResult_& base, const EarlyExercisePdeResult_& price, size_t index) {
        const auto& a = base.ExerciseLevels()[index];
        const auto& b = price.ExerciseLevels()[index];
        if (a.regionCount_ != b.regionCount_)
            return true;
        for (size_t j = 0; j < a.regionCount_; ++j) {
            const auto& x = base.ExerciseRegions()[a.regionOffset_ + j];
            const auto& y = price.ExerciseRegions()[b.regionOffset_ + j];
            if (x.firstNode_ != y.firstNode_ || x.lastNode_ != y.lastNode_)
                return true;
        }
        return false;
    }

    inline bool FutureChanged(const EarlyExercisePdeResult_& base, const EarlyExercisePdeResult_& price) {
        if (base.ExerciseLevels().size() != price.ExerciseLevels().size())
            return true;
        for (size_t i = 0; i < base.ExerciseLevels().size(); ++i) {
            const auto& a = base.ExerciseLevels()[i];
            if (a.timeIndex_ == 0 || a.kind_ == EarlyExercisePdeLevelKind_::Value_::TERMINAL)
                continue;
            if (LevelChanged(base, price, i))
                return true;
        }
        return false;
    }

    inline bool UpwindChanged(const EarlyExercisePdeMarket_& base, const EarlyExercisePdeMarket_& market, int nodes) {
        for (size_t j = 0; j < base.carry_.size(); ++j)
            for (int i = 1; i < nodes - 1; ++i) {
                const auto central = [j, i](const EarlyExercisePdeMarket_& m) {
                    return .5 * (m.volatility_ * i) * (m.volatility_ * i) >= .5 * std::abs((m.carry_[j].rate_ - m.carry_[j].dividendYield_) * i);
                };
                if (central(base) != central(market))
                    return true;
            }
        return false;
    }

    inline void Observe(
        EarlyExercisePdeRiskScenario_* row, const EarlyExercisePdeResult_& price, const EarlyExercisePdeResult_& base, double spot, double coarse) {
        const auto& e = price.Execution();
        row->solverStatus_ = price.Status();
        row->status_ = EarlyExercisePdeRiskScenarioStatus_(price.IsSuccess() ? "SUCCESS" : "NUMERICAL_FAILURE");
        row->priceAvailable_ = price.IsSuccess();
        if (price.IsSuccess())
            row->price_ = price.Price();
        row->actualSteps_ = e.actualSteps_;
        row->completedSteps_ = e.completedSteps_;
        row->totalSolves_ = e.totalSolves_;
        row->workUnits_ = e.workUnits_;
        row->numericPayloadBytes_ = e.numericPayloadBytes_;
        row->peakWorkspaceBytes_ = e.peakWorkspaceBytes_;
        row->maxLinearBackwardError_ = e.maxLinearBackwardError_;
        row->maxObstacleViolation_ = e.maxObstacleViolation_;
        row->maxDualViolation_ = e.maxDualViolation_;
        row->maxComplementarityResidual_ = e.maxComplementarityResidual_;
        row->failure_ = price.Failure();
        for (const auto& step : price.StepDiagnostics())
            if (std::isfinite(step.iterationChange_))
                row->maxIterationChange_ = std::max(row->maxIterationChange_.value_or(0.0), step.iterationChange_);
        if (price.IsSuccess()) {
            row->evaluationMembership_ = Membership(price);
            row->evaluationBoundaryIntersectsSpotStencil_ = Boundary(price, spot - coarse, spot + coarse);
            row->futureContactChanged_ = FutureChanged(base, price);
            row->upwindChanged_ = UpwindChanged(base.Request().market_, price.Request().market_, price.Request().settings_.gridPoints_);
        }
    }

    inline double CoarseSpot(const EarlyExercisePdeRiskRequest_& input) {
        ValidateEarlyExercisePdeRiskRequest(input);
        REQUIRE(input.risk_.delta_ && input.risk_.gamma_ && input.risk_.vega_ && input.risk_.carryRisk_ &&
                    input.valuation_.market_.volatility_ >= input.risk_.volatilityBump_,
                "reference requires the frozen full-axis fixtures");
        const double upper =
            input.valuation_.settings_.upper_.value_or(4 * std::max(input.valuation_.market_.spot_, input.valuation_.contract_.strike_));
        const double coarse = 2 * input.risk_.spotBump_.value_or(2 * upper / (input.valuation_.settings_.gridPoints_ - 1));
        REQUIRE(input.valuation_.market_.spot_ > coarse && input.valuation_.market_.spot_ + coarse < upper, "reference spot stencil outside domain");
        return coarse;
    }

    inline String_ AxisId(size_t axis, size_t segment, bool rate) {
        if (axis == 0)
            return "Spot";
        if (axis == 1)
            return "Volatility";
        return String_(rate ? "Rate[" : "DividendYield[") + String_(std::to_string(segment)) + "]";
    }

    inline double AxisBump(const EarlyExercisePdeRiskSettings_& risk, size_t axis, size_t segment, bool rate, double coarseSpot) {
        if (axis == 0)
            return coarseSpot;
        if (axis == 1)
            return risk.volatilityBump_;
        const auto& bumps = rate ? risk.rateBumps_ : risk.dividendYieldBumps_;
        return bumps.empty() ? 1e-4 : bumps[segment];
    }

    inline double* AxisParameter(EarlyExercisePdeMarket_* market, size_t axis, size_t segment, bool rate) {
        if (axis == 0)
            return &market->spot_;
        if (axis == 1)
            return &market->volatility_;
        return rate ? &market->carry_[segment].rate_ : &market->carry_[segment].dividendYield_;
    }

    struct Axis_ {
        size_t index_, segment_;
        bool rate_;
        String_ id_;
        double bump_;
    };

    inline void AppendScenario(const EarlyExercisePdeRiskRequest_& input,
                               const Axis_& axis,
                               size_t k,
                               Vector_<EarlyExercisePdeMarket_>* markets,
                               Vector_<EarlyExercisePdeRiskScenario_>* rows) {
        auto market = input.valuation_.market_;
        double* parameter = AxisParameter(&market, axis.index_, axis.segment_, axis.rate_);
        const double original = *parameter;
        *parameter += axis.bump_ * (k < 2 ? 1.0 : .5) * (k % 2 ? 1.0 : -1.0);
        EarlyExercisePdeRiskScenario_ row;
        row.scenarioIndex_ = rows->size();
        static const char* suffix[] = {":C-", ":C+", ":F-", ":F+"};
        row.scenarioId_ = axis.id_ + suffix[k];
        row.parameterId_ = axis.id_;
        if (axis.index_ >= 2)
            row.segmentIndex_ = axis.segment_;
        row.parameterValue_ = *parameter;
        row.offset_ = *parameter - original;
        rows->push_back(std::move(row));
        markets->push_back(std::move(market));
    }

    inline Vector_<EarlyExercisePdeMarket_>
    ScenarioMarkets(const EarlyExercisePdeRiskRequest_& input, double coarseSpot, Vector_<EarlyExercisePdeRiskScenario_>* rows) {
        const size_t axes = 2 + 2 * input.valuation_.market_.carry_.size();
        Vector_<EarlyExercisePdeMarket_> markets;
        markets.reserve(4 * axes);
        rows->reserve(1 + 4 * axes);
        rows->emplace_back();
        rows->back().scenarioId_ = "Base";
        for (size_t axis = 0; axis < axes; ++axis) {
            const size_t segment = axis < 2 ? 0 : (axis - 2) / 2;
            const bool rate = axis >= 2 && axis % 2 == 0;
            const Axis_ descriptor{axis, segment, rate, AxisId(axis, segment, rate), AxisBump(input.risk_, axis, segment, rate, coarseSpot)};
            for (size_t k = 0; k < 4; ++k)
                AppendScenario(input, descriptor, k, &markets, rows);
        }
        return markets;
    }

    inline void RunScenarios(const EarlyExercisePdeRiskRequest_& input,
                             const Vector_<EarlyExercisePdeMarket_>& markets,
                             const I::Plan_& plan,
                             double coarseSpot,
                             Reference_* out) {
        for (size_t i = 0; i < out->scenarios_.size(); ++i) {
            auto request = input.valuation_;
            if (i > 0)
                request.market_ = markets[i - 1];
            auto price = I::RollbackForRisk(request, plan);
            auto& row = out->scenarios_[i];
            row.planId_ = plan.Id();
            row.epsilon_ = plan.Epsilon();
            row.plannedSteps_ = plan.Counts().steps_;
            Observe(&row, price, i == 0 ? price : *out->base_, input.valuation_.market_.spot_, coarseSpot);
            REQUIRE(price.IsSuccess(), "reference rollback failed");
            out->work_ += row.workUnits_;
            out->solves_ += row.totalSolves_;
            if (i == 0)
                out->base_ = std::move(price);
        }
    }

    // This bounded reference supports only the four frozen full-axis benchmark fixtures.
    inline Reference_ Reference(const EarlyExercisePdeRiskRequest_& input) {
        const double coarseSpot = CoarseSpot(input);
        Reference_ out;
        out.request_ = input;
        const auto markets = ScenarioMarkets(input, coarseSpot, &out.scenarios_);
        const auto preview = I::PreviewPlan(input.valuation_, markets);
        const auto plan = I::BuildPlan(input.valuation_, markets, &preview);
        REQUIRE(preview.counts_.steps_ == plan.Counts().steps_, "reference preview mismatch");
        out.plan_ = plan;
        RunScenarios(input, markets, plan, coarseSpot, &out);
        return out;
    }

    inline void CheckTimeGrid(const I::Plan_& plan, const EarlyExercisePdeRiskPlan_& risk) {
        REQUIRE(plan.TimeGrid().size() == risk.timeGrid_.size(), "reference time grid count mismatch");
        for (size_t i = 0; i < risk.timeGrid_.size(); ++i) {
            const auto& a = plan.TimeGrid()[i];
            const auto& b = risk.timeGrid_[i];
            REQUIRE(a.timeYears_ == b.timeYears_ && a.calendarDate_ == b.calendarDate_ && a.isExerciseDate_ == b.isExerciseDate_ &&
                        a.isCarryKnot_ == b.isCarryKnot_ && a.isMaturity_ == b.isMaturity_,
                    "reference time grid mismatch");
        }
    }

    inline void CheckStep(const I::PlanStep_& a, const EarlyExercisePdeRiskPlan_& risk, size_t i) {
        const auto& b = risk.steps_[i];
        REQUIRE(a.source_ - a.target_ == b.dt_ && a.theta_ == b.theta_ && a.carry_ == b.carryIndex_ && a.damped_ == b.damped_,
                "reference step mismatch");
        REQUIRE(b.timeIndex_ == static_cast<int>(i) && b.sourceTimeIndex_ == static_cast<int>(i + 1) && a.target_ == risk.timeGrid_[i].timeYears_ &&
                    a.source_ == risk.timeGrid_[i + 1].timeYears_,
                "reference step mismatch");
    }

    inline void CheckPlan(const I::Plan_& plan, const EarlyExercisePdeRiskPlan_& risk) {
        REQUIRE(plan.Id() == risk.planId_ && plan.Grid() == risk.grid_, "reference plan/grid mismatch");
        CheckTimeGrid(plan, risk);
        REQUIRE(plan.Steps().size() == risk.steps_.size(), "reference step count mismatch");
        for (size_t i = 0; i < risk.steps_.size(); ++i)
            CheckStep(plan.Steps()[risk.steps_.size() - 1 - i], risk, i);
    }

    inline void CheckScenario(const EarlyExercisePdeRiskScenario_& a, const EarlyExercisePdeRiskScenario_& b) {
#define DAL_BENCH_MATCH(field) REQUIRE(a.field == b.field, "reference scenario mismatch: " #field)
        DAL_BENCH_MATCH(scenarioIndex_);
        DAL_BENCH_MATCH(scenarioId_);
        DAL_BENCH_MATCH(planId_);
        DAL_BENCH_MATCH(parameterId_);
        DAL_BENCH_MATCH(segmentIndex_);
        DAL_BENCH_MATCH(parameterValue_);
        DAL_BENCH_MATCH(offset_);
        DAL_BENCH_MATCH(status_);
        DAL_BENCH_MATCH(solverStatus_);
        DAL_BENCH_MATCH(workLimitSource_);
        DAL_BENCH_MATCH(priceAvailable_);
        DAL_BENCH_MATCH(price_);
        DAL_BENCH_MATCH(epsilon_);
        DAL_BENCH_MATCH(plannedSteps_);
        DAL_BENCH_MATCH(actualSteps_);
        DAL_BENCH_MATCH(completedSteps_);
        DAL_BENCH_MATCH(totalSolves_);
        DAL_BENCH_MATCH(workUnits_);
        DAL_BENCH_MATCH(numericPayloadBytes_);
        DAL_BENCH_MATCH(peakWorkspaceBytes_);
        DAL_BENCH_MATCH(maxIterationChange_);
        DAL_BENCH_MATCH(maxLinearBackwardError_);
        DAL_BENCH_MATCH(maxObstacleViolation_);
        DAL_BENCH_MATCH(maxDualViolation_);
        DAL_BENCH_MATCH(maxComplementarityResidual_);
        DAL_BENCH_MATCH(evaluationMembership_);
        DAL_BENCH_MATCH(evaluationBoundaryIntersectsSpotStencil_);
        DAL_BENCH_MATCH(futureContactChanged_);
        DAL_BENCH_MATCH(upwindChanged_);
#undef DAL_BENCH_MATCH
        REQUIRE(!a.failure_ && !b.failure_, "benchmark fixture contains numerical failure");
    }

    inline void CheckIdentity(const Reference_& reference, const EarlyExercisePdeRiskResult_& risk) {
        CheckPlan(*reference.plan_, risk.Plan());
        REQUIRE(reference.work_ == risk.Execution().workUnits_ && reference.solves_ == risk.Execution().totalSolves_,
                "reference work/solve mismatch");
        REQUIRE(reference.scenarios_.size() == risk.Scenarios().size(), "reference scenario count mismatch");
        for (size_t i = 0; i < reference.scenarios_.size(); ++i)
            CheckScenario(reference.scenarios_[i], risk.Scenarios()[i]);
    }
} // namespace Dal::PdeRiskBench
