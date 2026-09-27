//
// Created by dal-implementer on 2026/9/20.
//

#pragma once

#include <array>
#include <optional>

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/preparation.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>

namespace Dal::Script {
    struct SimResults_;

    //  Frozen continuation regression of one exercise date. Scalar fits use
    //  [1, z, ..., z^d], z = (x - mean_) / sigma_; multivariate fits use
    //  total-degree monomials with per-coordinate means_/sigmas_ and powers_.
    //  A degenerate day carries only the constant continuation.
    struct ExerciseRegression_ {
        Vector_<> coefficients_;
        Vector_<> means_;
        Vector_<> sigmas_;
        Vector_<std::array<unsigned char, 3>> powers_;
        int basisDegree_ = 0;
        bool degenerate_ = false;
        String_ degenerateReason_;
        double mean_ = 0.0;
        double sigma_ = 1.0;
        size_t numCondTrue_ = 0;
        size_t effectiveRank_ = 0;
        String_ solver_ = "Constant";
        String_ fallbackReason_;
        std::optional<double> validationMse_;
    };

    ExerciseRegression_ SolveExerciseRegression(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree);

    struct MultivariateRegressionRows_ {
        std::array<const double*, 3> features_{};
        const double* targets_ = nullptr;
        const char* included_ = nullptr;
        size_t n_ = 0;
        size_t nFeatures_ = 0;
    };

    ExerciseRegression_ SolveMultivariateExerciseRegression(const MultivariateRegressionRows_& rows, int degree);

    //  Horner evaluation of frozen coefficients on the z-normalized basis; the
    //  arithmetic is identical for T_ = double (Phase B/C decisions) and AAD number
    //  types (the fuzzy replay differentiates through the frozen continuation)
    template <class T_> T_ PredictContinuation(const double* coefficients, size_t nCoefficients, double mean, double sigma, const T_& x) {
        if (nCoefficients == 1)
            return T_(coefficients[0]);
        const T_ z = (x - mean) / sigma;
        T_ value(0.0);
        for (size_t j = nCoefficients; j-- > 0;)
            value = value * z + coefficients[j];
        return value;
    }

    //  A default regression has no coefficients and predicts zero
    template <class T_> T_ RegressionPredict(const ExerciseRegression_& regression, const T_& x) {
        const auto& coefficients = regression.coefficients_;
        return PredictContinuation(coefficients.empty() ? nullptr : &coefficients[0], coefficients.size(), regression.mean_, regression.sigma_, x);
    }

    template <class T_> T_ RegressionPredict(const ExerciseRegression_& regression, const std::array<T_, 3>& features) {
        if (regression.powers_.empty())
            return RegressionPredict(regression, features[0]);
        std::array<std::array<T_, 4>, 3> featurePowers{};
        for (size_t feature = 0; feature < regression.means_.size(); ++feature) {
            featurePowers[feature][0] = T_(1.0);
            const T_ normalized = (features[feature] - regression.means_[feature]) / regression.sigmas_[feature];
            for (size_t power = 1; power <= static_cast<size_t>(regression.basisDegree_); ++power)
                featurePowers[feature][power] = featurePowers[feature][power - 1] * normalized;
        }
        T_ result(0.0);
        for (size_t term = 0; term < regression.coefficients_.size(); ++term) {
            T_ value(regression.coefficients_[term]);
            for (size_t feature = 0; feature < regression.means_.size(); ++feature)
                value *= featurePowers[feature][regression.powers_[term][feature]];
            result += value;
        }
        return result;
    }

    //  Per-exercise-event diagnostics projected into dal.script-simulation/1
    struct ExerciseEventStats_ {
        size_t eventId_ = 0;
        Date_ date_;
        int requestedDegree_ = 0;
        int basisDegree_ = 0;    //  0 marks the degenerate constant basis
        String_ regressorIndex_; //  canonical index name; empty when no model binding exists
        Vector_<String_> regressionFeatures_;
        Vector_<> normalizationMeans_;
        Vector_<> normalizationSigmas_;
        Vector_<std::array<unsigned char, 3>> basisPowers_;
        size_t numCondTruePaths_ = 0;
        Vector_<> coefficients_;
        bool degenerate_ = false;
        String_ degenerateReason_; //  "" | ConditionPathsBelowMin | SigmaFloor | IllConditioned
        size_t effectiveRank_ = 0;
        String_ solver_;
        String_ fallbackReason_;
        std::optional<double> validationMse_;
        double exerciseRate_ = 0.0;
    };

    struct LsmcDiagnostics_ {
        Vector_<ExerciseEventStats_> events_;
        double payoffSum_ = 0.0;
        double payoffSumSq_ = 0.0;
        size_t nPaths_ = 0;
        size_t trainingPaths_ = 0;
        size_t validationPaths_ = 0;
        size_t pricingPathsPerReplicate_ = 0;
        size_t replicateCount_ = 1;
        std::optional<int> trainingSeed_;
        std::optional<int> pricingSeed_;
        String_ scrambleIdentity_;
        Vector_<> replicateMeans_;

        [[nodiscard]] double StandardError() const {
            if (nPaths_ == 0)
                return 0.0;
            const double n = static_cast<double>(nPaths_);
            const double mean = payoffSum_ / n;
            const double variance = std::max(0.0, payoffSumSq_ / n - mean * mean);
            return std::sqrt(variance / n);
        }

        [[nodiscard]] std::optional<double> ReplicateMeanStandardError() const {
            if (replicateMeans_.size() < 2)
                return std::nullopt;
            double mean = 0.0;
            for (double value : replicateMeans_)
                mean += value;
            mean /= static_cast<double>(replicateMeans_.size());
            double squared = 0.0;
            for (double value : replicateMeans_)
                squared += (value - mean) * (value - mean);
            const double count = static_cast<double>(replicateMeans_.size());
            return std::sqrt(squared / ((count - 1.0) * count));
        }
    };

    //  Train on lsmcTrainingPaths_ Sobol points (default nPaths), optionally select
    //  degree on the following lsmcValidationPaths_ points, then price on nPaths
    //  further points. Release training and validation rows before pricing;
    //  batches and reductions are independent of the worker count.
    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics = nullptr);

    //  Fuzzy (AAD) LSMC: Phase A/B fit a thread-invariant hard policy, then workers
    //  replay disjoint pricing batches on their own tapes. Frozen mode returns the
    //  adjoint of that policy's fuzzy price. RetrainedBump also adds a common-path
    //  finite secant of regenerated policies for model and script-constant inputs.
    SimResults_ MCLsmcAadSimulation(const PreparedScript_& prepared, const Handle_<ModelData_>& modelData, size_t nPaths);
} // namespace Dal::Script
