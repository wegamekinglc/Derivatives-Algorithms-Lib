#pragma once

#include <dal/math/pde/earlyexercisepde.hpp>

/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskStatus
    Early exercise risk status
switchable
alternative COMPLETE
alternative PARTIAL
alternative BASE_FAILED
alternative WORK_LIMIT
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskAvailability
    Early exercise risk availability
switchable
alternative AVAILABLE
alternative AVAILABLE_EXERCISE_REGION
alternative UNAVAILABLE_BASE_FAILED
alternative UNAVAILABLE_WORK_LIMIT
alternative UNAVAILABLE_SCENARIO_FAILED
alternative UNAVAILABLE_DEGENERATE_DIFFUSION
alternative UNAVAILABLE_DOMAIN_BOUNDARY
alternative UNAVAILABLE_NEAR_EXERCISE_BOUNDARY
alternative UNAVAILABLE_EXERCISE_SWITCH
alternative UNAVAILABLE_NONFINITE
alternative UNAVAILABLE_STEP_UNSTABLE
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskScenarioStatus
    Early exercise risk scenariostatus
switchable
alternative PLANNED
alternative SUCCESS
alternative NUMERICAL_FAILURE
alternative NOT_RUN_WORK_LIMIT
alternative NOT_RUN_BASE_FAILED
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskWorkLimitSource
    Early exercise risk worklimitsource
switchable
alternative NONE
alternative SCENARIO
alternative GLOBAL
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskMembership
    Early exercise risk membership
switchable
alternative NOT_EXERCISABLE
alternative CONTINUATION
alternative EXERCISE
alternative BOUNDARY
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeRiskBumpOrigin
    Early exercise risk bumporigin
switchable
alternative DEFAULT
alternative EXPLICIT
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_EarlyExercisePdeRiskAvailability_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeRiskBumpOrigin_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeRiskMembership_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeRiskScenarioStatus_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeRiskStatus_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeRiskWorkLimitSource_enum.hpp>
} // namespace Dal

namespace Dal::PDE {
    namespace EarlyExercisePdeRiskInternal {
        struct Evaluator_;
    } // namespace EarlyExercisePdeRiskInternal
    struct EarlyExercisePdeRiskSettings_ {
        bool delta_ = true, gamma_ = true, vega_ = true, carryRisk_ = true;
        std::optional<double> spotBump_;
        double volatilityBump_ = 1e-3;
        Vector_<double> rateBumps_, dividendYieldBumps_;
        double deltaStabilityTolerance_ = 2e-3, gammaStabilityTolerance_ = 2e-2, vegaStabilityTolerance_ = 3e-3;
        double rateStabilityTolerance_ = 3e-3, dividendYieldStabilityTolerance_ = 3e-3;
        size_t maxScenarios_ = 129, maxWorkUnits_ = 100000000;
        size_t numericPayloadBudgetBytes_ = 67108864, workspaceBudgetBytes_ = 268435456;
    };
    struct EarlyExercisePdeRiskRequest_ {
        EarlyExercisePdeRequest_ valuation_;
        EarlyExercisePdeRiskSettings_ risk_;
    };
    struct EarlyExercisePdeRiskStencilEntry_ {
        size_t scenarioIndex_ = 0;
        double weight_ = 0.0;
    };
    struct EarlyExercisePdeRiskRecord_ {
        size_t riskIndex_ = 0;
        String_ riskId_, parameterId_;
        int derivativeOrder_ = 1;
        std::optional<size_t> segmentIndex_;
        std::optional<Date_> startDate_, endDate_;
        double parameterValue_ = 0.0;
        String_ unit_;
        std::optional<double> value_;
        EarlyExercisePdeRiskAvailability_ status_{EarlyExercisePdeRiskAvailability_::Value_::AVAILABLE};
        String_ reason_, method_, interpretation_;
        double fineBump_ = 0.0, coarseBump_ = 0.0;
        EarlyExercisePdeRiskBumpOrigin_ bumpOrigin_;
        std::optional<double> fineEstimate_, coarseEstimate_, stepDifference_, normalizedStepDifference_;
        double stabilityTolerance_ = 0.0;
        Vector_<EarlyExercisePdeRiskStencilEntry_> fineStencil_, coarseStencil_;
    };
    struct EarlyExercisePdeRiskPlanStep_ {
        int sourceTimeIndex_ = 0, timeIndex_ = 0;
        double dt_ = 0.0, theta_ = 1.0;
        int carryIndex_ = 0;
        bool damped_ = false, projection_ = false;
    };
    struct EarlyExercisePdeRiskPlan_ {
        int schemaVersion_ = 1;
        String_ planId_;
        Date_ evaluationDate_, maturityDate_;
        double upper_ = 0.0;
        int gridPoints_ = 0;
        double gridSpacing_ = 0.0;
        Vector_<double> grid_;
        Vector_<EarlyExercisePdeTimeNode_> timeGrid_;
        Vector_<EarlyExercisePdeRiskPlanStep_> steps_;
        double epsilon_ = 0.0, theta_ = 1.0;
        int effectiveRannacherIntervals_ = 0;
        double penaltyFactor_ = 0.0;
        int maxIterationsPerStep_ = 0;
        double linearBackwardErrorLimit_ = 0.0;
        String_ interpolationPolicy_ = "LINEAR_SPOT";
        EarlyExercisePdeRiskSettings_ resolvedRisk_;
        EarlyExercisePdeRiskBumpOrigin_ spotBumpOrigin_, volatilityBumpOrigin_;
        Vector_<EarlyExercisePdeRiskBumpOrigin_> rateBumpOrigins_, dividendYieldBumpOrigins_;
        size_t plannedScenarios_ = 0;
        int basePlannedSteps_ = 0, plannedSteps_ = 0;
        size_t plannedNumericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0, plannedPeakNativeBytes_ = 0;
    };
    struct EarlyExercisePdeRiskScenario_ {
        size_t scenarioIndex_ = 0;
        String_ scenarioId_, planId_;
        std::optional<String_> parameterId_;
        std::optional<size_t> segmentIndex_;
        std::optional<double> parameterValue_, offset_;
        EarlyExercisePdeRiskScenarioStatus_ status_{EarlyExercisePdeRiskScenarioStatus_::Value_::PLANNED};
        std::optional<EarlyExercisePdeStatus_> solverStatus_;
        EarlyExercisePdeRiskWorkLimitSource_ workLimitSource_{EarlyExercisePdeRiskWorkLimitSource_::Value_::NONE};
        bool priceAvailable_ = false;
        std::optional<double> price_;
        double epsilon_ = 0.0;
        int plannedSteps_ = 0, actualSteps_ = 0, completedSteps_ = 0;
        size_t totalSolves_ = 0, workUnits_ = 0, numericPayloadBytes_ = 0, peakWorkspaceBytes_ = 0;
        std::optional<double> maxIterationChange_, maxLinearBackwardError_, maxObstacleViolation_, maxDualViolation_, maxComplementarityResidual_;
        std::optional<EarlyExercisePdeFailure_> failure_;
        std::optional<EarlyExercisePdeRiskMembership_> evaluationMembership_;
        std::optional<bool> evaluationBoundaryIntersectsSpotStencil_, futureContactChanged_, upwindChanged_;
    };
    struct EarlyExercisePdeRiskExecution_ {
        size_t plannedScenarios_ = 0, attemptedScenarios_ = 0, successfulScenarios_ = 0, failedScenarios_ = 0, notRunScenarios_ = 0;
        size_t totalSolves_ = 0, workUnits_ = 0;
        size_t plannedNumericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0, plannedPeakNativeBytes_ = 0;
        size_t numericPayloadBytes_ = 0, peakWorkspaceBytes_ = 0, peakNativeBytes_ = 0;
        std::optional<size_t> workLimitScenarioIndex_;
        EarlyExercisePdeRiskWorkLimitSource_ workLimitSource_{EarlyExercisePdeRiskWorkLimitSource_::Value_::NONE};
    };
    class EarlyExercisePdeRiskResult_ {
        EarlyExercisePdeRiskResult_() = default;
        EarlyExercisePdeRiskRequest_ request_;
        std::optional<EarlyExercisePdeResult_> base_;
        EarlyExercisePdeRiskPlan_ plan_;
        Vector_<EarlyExercisePdeRiskRecord_> risks_;
        Vector_<EarlyExercisePdeRiskScenario_> scenarios_;
        EarlyExercisePdeRiskExecution_ execution_;
        EarlyExercisePdeRiskStatus_ status_{EarlyExercisePdeRiskStatus_::Value_::PARTIAL};
        friend struct EarlyExercisePdeRiskInternal::Evaluator_;

    public:
        [[nodiscard]] int SchemaVersion() const { return 1; }
        [[nodiscard]] EarlyExercisePdeRiskStatus_ Status() const { return status_; }
        [[nodiscard]] bool IsComplete() const { return status_ == EarlyExercisePdeRiskStatus_::Value_::COMPLETE; }
        [[nodiscard]] bool PriceAvailable() const;
        [[nodiscard]] double Price() const;
        [[nodiscard]] double RiskValue(const String_& riskId) const;
        [[nodiscard]] const EarlyExercisePdeRiskRequest_& Request() const { return request_; }
        [[nodiscard]] const EarlyExercisePdeResult_& BaseResult() const { return *base_; }
        [[nodiscard]] const EarlyExercisePdeRiskPlan_& Plan() const { return plan_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeRiskRecord_>& Risks() const { return risks_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeRiskScenario_>& Scenarios() const { return scenarios_; }
        [[nodiscard]] const EarlyExercisePdeRiskExecution_& Execution() const { return execution_; }
        [[nodiscard]] String_ Method() const { return "BlackScholesEarlyExerciseFullRepriceFixedPlan"; }
        [[nodiscard]] String_ RiskBasis() const { return "NATIVE_MARKET_PARAMETER_PARTIAL"; }
        [[nodiscard]] bool AadUsed() const { return false; }
    };
    void ValidateEarlyExercisePdeRiskSettings(const EarlyExercisePdeRiskSettings_&);
    void ValidateEarlyExercisePdeRiskRequest(const EarlyExercisePdeRiskRequest_&);
    [[nodiscard]] EarlyExercisePdeRiskResult_ EvaluateEarlyExercisePdeRisk(const EarlyExercisePdeRiskRequest_&);
} // namespace Dal::PDE
