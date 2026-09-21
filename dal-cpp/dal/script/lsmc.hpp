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

    //  Horner evaluation of the frozen coefficients on the z-normalized basis; the
    //  arithmetic is identical for T_ = double (Phase B/C decisions) and AAD number
    //  types (the fuzzy replay differentiates through the frozen continuation)
    template <class T_> T_ RegressionPredict(const ExerciseRegression_& regression, const T_& x) {
        const T_ z = (x - regression.mean_) / regression.sigma_;
        T_ value(0.0);
        for (size_t j = regression.coefficients_.size(); j-- > 0;)
            value = value * z + regression.coefficients_[j];
        return value;
    }

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

    //  Double-mode tree-walk LSMC (S3/S4 hard decisions): Phase A stores payments,
    //  exercise triples and the terminal payoff per path over the fixed
    //  thread-independent batch layout, Phase B runs the backward induction with the
    //  continuation regressions, Phase C values the frozen strategy from the recorded
    //  rows and aggregates the path payoffs.
    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics = nullptr);

    //  Fuzzy (AAD) LSMC (S9/N6): Phases A/B run exactly as the double driver (the
    //  frozen policy is thread-count independent by construction), then each worker
    //  replays its batch on its own tape, blending the recursive fuzzy decisions over
    //  the frozen coefficients and harvesting parameter and constant-variable adjoints
    //  with the same batch-index reduction (bitwise thread invariant).
    SimResults_ MCLsmcAadSimulation(const PreparedScript_& prepared, const Handle_<ModelData_>& modelData, size_t nPaths);
} // namespace Dal::Script
