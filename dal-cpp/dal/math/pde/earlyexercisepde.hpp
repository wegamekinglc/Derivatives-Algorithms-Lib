#pragma once

#include <limits>
#include <optional>

#include <dal/protocol/optiontype.hpp>
#include <dal/time/date.hpp>

/*IF--------------------------------------------------------------------------
enumeration EarlyExerciseStyle
    Exercise opportunity policy
switchable
alternative BERMUDAN
alternative AMERICAN
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeStatus
    Early exercise rollback outcome
switchable
alternative SUCCESS
alternative MAX_ITERATIONS
alternative LCP_RESIDUAL_LIMIT
alternative LINEAR_SOLVE_FAILURE
alternative NON_MONOTONE_OPERATOR
alternative NONFINITE
alternative WORK_LIMIT
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration EarlyExercisePdeLevelKind
    Exercise diagnostic layer policy
switchable
alternative BERMUDAN_PROJECTION
alternative AMERICAN_LCP
alternative TERMINAL
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_EarlyExercisePdeLevelKind_enum.hpp>
#include <dal/auto/MG_EarlyExercisePdeStatus_enum.hpp>
#include <dal/auto/MG_EarlyExerciseStyle_enum.hpp>
} // namespace Dal

namespace Dal::PDE {
    struct EarlyExercisePdeCarrySegment_ {
        Date_ endDate_;
        double rate_ = std::numeric_limits<double>::quiet_NaN();
        double dividendYield_ = std::numeric_limits<double>::quiet_NaN();
    };
    struct EarlyExercisePdeMarket_ {
        double spot_ = std::numeric_limits<double>::quiet_NaN();
        double volatility_ = std::numeric_limits<double>::quiet_NaN();
        Vector_<EarlyExercisePdeCarrySegment_> carry_;
    };
    struct EarlyExercisePdeContract_ {
        Date_ evaluationDate_, maturityDate_;
        double strike_ = std::numeric_limits<double>::quiet_NaN();
        OptionType_ optionType_;
        EarlyExerciseStyle_ exerciseStyle_;
        Vector_<Date_> exerciseDates_;
    };
    struct EarlyExercisePdeSettings_ {
        int gridPoints_ = 401;
        std::optional<double> upper_;
        int ordinarySteps_ = 500;
        double theta_ = 1.0;
        int rannacherIntervals_ = 2;
        double penaltyFactor_ = 1e8;
        double absoluteTolerance_ = 1e-8;
        double relativeTolerance_ = 1e-10;
        double linearBackwardErrorLimit_ = 1e-12;
        int maxIterationsPerStep_ = 100;
        int maxActualSteps_ = 100000;
        size_t maxWorkUnits_ = 100000000;
        size_t numericPayloadBudgetBytes_ = 67108864;
        size_t workspaceBudgetBytes_ = 268435456;
    };
    struct EarlyExercisePdeRequest_ {
        EarlyExercisePdeContract_ contract_;
        EarlyExercisePdeMarket_ market_;
        EarlyExercisePdeSettings_ settings_;
    };
    struct EarlyExercisePdeTimeNode_ {
        double timeYears_ = 0.0;
        std::optional<Date_> calendarDate_;
        bool isExerciseDate_ = false, isCarryKnot_ = false, isMaturity_ = false;
    };
    struct EarlyExercisePdeStepDiagnostic_ {
        int rollbackStep_ = 0, sourceTimeIndex_ = 0, timeIndex_ = 0, solveCount_ = 0;
        double dt_ = 0.0, theta_ = 1.0, iterationChange_ = 0.0;
        bool damped_ = false, projection_ = false, completed_ = false;
        std::optional<double> linearBackwardError_;
        std::optional<double> projectionChange_, projectionResidual_, obstacleViolation_;
        std::optional<double> dualViolation_, complementarityResidual_;
        std::optional<double> normalizedObstacleViolation_, normalizedDualViolation_, normalizedComplementarityResidual_;
    };
    struct EarlyExercisePdeExerciseLevel_ {
        int timeIndex_ = 0;
        std::optional<Date_> calendarDate_;
        EarlyExercisePdeLevelKind_ kind_;
        size_t regionOffset_ = 0, regionCount_ = 0;
        double epsilon_ = 0.0;
    };
    struct EarlyExercisePdeExerciseRegion_ {
        int timeIndex_ = 0, regionIndex_ = 0, firstNode_ = 0, lastNode_ = 0;
        bool leftClipped_ = false, rightClipped_ = false;
    };
    struct EarlyExercisePdeExecution_ {
        int plannedSteps_ = 0, actualSteps_ = 0, completedSteps_ = 0;
        int baseSteps_ = 0, dampingAddedSteps_ = 0, stabilityAddedSteps_ = 0, effectiveRannacherIntervals_ = 0;
        int maxIterations_ = 0, upwindRows_ = 0;
        size_t totalSolves_ = 0, workUnits_ = 0, plannedRegionCapacity_ = 0, regionCapacity_ = 0;
        size_t plannedNumericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0;
        size_t numericPayloadBytes_ = 0, peakWorkspaceBytes_ = 0;
        double epsilon_ = 0.0;
        std::optional<double> maxLinearBackwardError_, maxObstacleViolation_, maxDualViolation_, maxComplementarityResidual_;
    };
    struct EarlyExercisePdeFailure_ {
        EarlyExercisePdeStatus_ status_;
        int timeIndex_ = 0, iteration_ = 0;
        std::optional<Date_> calendarDate_;
        std::optional<int> node_;
        std::optional<double> residual_, threshold_;
        size_t workUnits_ = 0;
        String_ reason_;
    };

    class EarlyExercisePdeResult_ {
        EarlyExercisePdeResult_() = default;
        EarlyExercisePdeRequest_ request_;
        EarlyExercisePdeStatus_ status_{EarlyExercisePdeStatus_::Value_::SUCCESS};
        std::optional<double> price_;
        Vector_<double> grid_, values_;
        Vector_<EarlyExercisePdeTimeNode_> timeGrid_;
        Vector_<EarlyExercisePdeStepDiagnostic_> steps_;
        Vector_<EarlyExercisePdeExerciseLevel_> levels_;
        Vector_<EarlyExercisePdeExerciseRegion_> regions_;
        EarlyExercisePdeExecution_ execution_;
        std::optional<EarlyExercisePdeFailure_> failure_;
        friend EarlyExercisePdeResult_ EvaluateEarlyExercisePde(const EarlyExercisePdeRequest_&);

    public:
        [[nodiscard]] EarlyExercisePdeStatus_ Status() const { return status_; }
        [[nodiscard]] bool IsSuccess() const { return status_ == EarlyExercisePdeStatus_::Value_::SUCCESS; }
        [[nodiscard]] double Price() const;
        [[nodiscard]] const Vector_<double>& Values() const;
        [[nodiscard]] const EarlyExercisePdeRequest_& Request() const { return request_; }
        [[nodiscard]] const Vector_<double>& Grid() const { return grid_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeTimeNode_>& TimeGrid() const { return timeGrid_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeStepDiagnostic_>& StepDiagnostics() const { return steps_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeExerciseLevel_>& ExerciseLevels() const { return levels_; }
        [[nodiscard]] const Vector_<EarlyExercisePdeExerciseRegion_>& ExerciseRegions() const { return regions_; }
        [[nodiscard]] const EarlyExercisePdeExecution_& Execution() const { return execution_; }
        [[nodiscard]] const std::optional<EarlyExercisePdeFailure_>& Failure() const { return failure_; }
        [[nodiscard]] String_ Method() const;
    };

    void ValidateEarlyExercisePdeSettings(const EarlyExercisePdeSettings_& settings);
    void ValidateEarlyExercisePdeRequest(const EarlyExercisePdeRequest_& request);
    [[nodiscard]] EarlyExercisePdeResult_ EvaluateEarlyExercisePde(const EarlyExercisePdeRequest_& request);
} // namespace Dal::PDE
