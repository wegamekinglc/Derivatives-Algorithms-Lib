//
// Created by dal-implementer on 2026/9/20.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/preparation.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>

namespace Dal::Script {
    struct SimResults_;

    //  Frozen continuation regression of one exercise date: coefficients live on the
    //  z-normalized monomial basis [1, z, ..., z^d], z = (x - mean_) / sigma_. A
    //  degenerate day carries the constant fit (intercept only) and one of the
    //  PascalCase tokens ConditionPathsBelowMin / SigmaFloor / IllConditioned.
    struct ExerciseRegression_ {
        Vector_<> coefficients_;
        int basisDegree_ = 0;
        bool degenerate_ = false;
        String_ degenerateReason_;
        double mean_ = 0.0;
        double sigma_ = 1.0;
        size_t numCondTrue_ = 0;
    };

    ExerciseRegression_ SolveExerciseRegression(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree);

    double RegressionPredict(const ExerciseRegression_& regression, double x);

    //  Per-exercise-event diagnostics projected into dal.script-simulation/1
    struct ExerciseEventStats_ {
        size_t eventId_ = 0;
        Date_ date_;
        int requestedDegree_ = 0;
        int basisDegree_ = 0;    //  0 marks the degenerate constant basis
        String_ regressorIndex_; //  canonical index name; empty when no model binding exists
        size_t numCondTruePaths_ = 0;
        Vector_<> coefficients_;
        bool degenerate_ = false;
        String_ degenerateReason_; //  "" | ConditionPathsBelowMin | SigmaFloor | IllConditioned
        double exerciseRate_ = 0.0;
    };

    struct LsmcDiagnostics_ {
        Vector_<ExerciseEventStats_> events_;
        double payoffSum_ = 0.0;
        double payoffSumSq_ = 0.0;
        size_t nPaths_ = 0;

        [[nodiscard]] double StandardError() const {
            if (nPaths_ == 0)
                return 0.0;
            const double n = static_cast<double>(nPaths_);
            const double mean = payoffSum_ / n;
            const double variance = std::max(0.0, payoffSumSq_ / n - mean * mean);
            return std::sqrt(variance / n);
        }
    };

    //  Double-mode tree-walk LSMC (S3/S4 hard decisions): Phase A stores payments and
    //  exercise triples over the fixed thread-independent batch layout, Phase B runs
    //  the backward induction with the continuation regressions, Phase C replays the
    //  frozen strategy on regenerated paths and aggregates the path payoffs.
    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics = nullptr);
} // namespace Dal::Script
