//
// Created by Codex on 2026/9/27.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/regressionqr.hpp>

namespace Dal::Script {
    namespace {
        constexpr size_t MAX_BASIS = 20;
        constexpr size_t PATHS_PER_BASIS = 10;
        constexpr double RANK_TOLERANCE = 1e-10;

        Vector_<std::array<unsigned char, 3>> BasisPowers(size_t features, int degree) {
            Vector_<std::array<unsigned char, 3>> powers;
            for (int total = 0; total <= degree; ++total)
                for (int first = total; first >= 0; --first) {
                    if (features == 2) {
                        powers.push_back({static_cast<unsigned char>(first), static_cast<unsigned char>(total - first), 0});
                    } else {
                        for (int second = total - first; second >= 0; --second)
                            powers.push_back({static_cast<unsigned char>(first), static_cast<unsigned char>(second),
                                              static_cast<unsigned char>(total - first - second)});
                    }
                }
            REQUIRE2(powers.size() <= MAX_BASIS, "InvalidLsmcFeatureBudget: too many multivariate basis terms", ScriptError_);
            return powers;
        }

        void ConstantFit(ExerciseRegression_* fit, double target, const char* reason) {
            fit->coefficients_ = {target};
            fit->basisDegree_ = 0;
            fit->effectiveRank_ = fit->numCondTrue_ > 0 ? 1 : 0;
            fit->degenerate_ = true;
            fit->degenerateReason_ = reason;
            fit->fallbackReason_ = reason;
            fit->solver_ = "Constant";
        }

        using QrWorkspace_ = RegressionQR::Workspace_<MAX_BASIS>;

        QrWorkspace_ MakeWorkspace(const MultivariateRegressionRows_& rows, const ExerciseRegression_& fit) {
            QrWorkspace_ ws;
            ws.rows_ = fit.numCondTrue_;
            ws.basis_ = fit.powers_.size();
            ws.targets_.Resize(ws.rows_);
            for (size_t term = 0; term < ws.basis_; ++term) {
                ws.columns_[term].Resize(ws.rows_);
                ws.permutation_[term] = term;
            }
            size_t row = 0;
            for (size_t path = 0; path < rows.n_; ++path) {
                if (!rows.included_[path])
                    continue;
                std::array<std::array<double, 4>, 3> featurePowers{};
                for (size_t feature = 0; feature < rows.nFeatures_; ++feature) {
                    const double z = (rows.features_[feature][path] - fit.means_[feature]) / fit.sigmas_[feature];
                    featurePowers[feature][0] = 1.0;
                    for (int power = 1; power <= fit.basisDegree_; ++power)
                        featurePowers[feature][power] = featurePowers[feature][power - 1] * z;
                }
                ws.targets_[row] = rows.targets_[path];
                for (size_t term = 0; term < ws.basis_; ++term) {
                    double value = 1.0;
                    for (size_t feature = 0; feature < rows.nFeatures_; ++feature)
                        value *= featurePowers[feature][fit.powers_[term][feature]];
                    REQUIRE2(std::isfinite(value), "InvalidRegressionInput: multivariate basis overflow", ScriptError_);
                    ws.columns_[term][row] = value;
                }
                ++row;
            }
            return ws;
        }

        void ValidateRows(const MultivariateRegressionRows_& rows, int degree) {
            REQUIRE2(rows.nFeatures_ >= 2 && rows.nFeatures_ <= 3, "InvalidLsmcFeatureBudget: multivariate regression requires two or three features",
                     ScriptError_);
            REQUIRE2(degree >= 1 && degree <= 3, "InvalidLsmcFeatureBudget: multivariate basis degree must be in 1..3", ScriptError_);
            REQUIRE2(rows.targets_ && rows.included_, "InvalidRegressionInput: null multivariate target or mask", ScriptError_);
            for (size_t feature = 0; feature < rows.nFeatures_; ++feature)
                REQUIRE2(rows.features_[feature], "InvalidRegressionInput: null multivariate feature", ScriptError_);
        }

        double CollectMoments(const MultivariateRegressionRows_& rows, ExerciseRegression_* fit, Vector_<>* m2) {
            double targetMean = 0.0;
            for (size_t path = 0; path < rows.n_; ++path) {
                if (!rows.included_[path])
                    continue;
                REQUIRE2(std::isfinite(rows.targets_[path]), "InvalidRegressionInput: non-finite included target", ScriptError_);
                ++fit->numCondTrue_;
                targetMean += (rows.targets_[path] - targetMean) / static_cast<double>(fit->numCondTrue_);
                for (size_t feature = 0; feature < rows.nFeatures_; ++feature) {
                    const double value = rows.features_[feature][path];
                    REQUIRE2(std::isfinite(value), "InvalidRegressionInput: non-finite included feature", ScriptError_);
                    const double delta = value - fit->means_[feature];
                    fit->means_[feature] += delta / static_cast<double>(fit->numCondTrue_);
                    (*m2)[feature] += delta * (value - fit->means_[feature]);
                }
            }
            REQUIRE2(std::isfinite(targetMean), "InvalidRegressionInput: multivariate target mean overflow", ScriptError_);
            return targetMean;
        }

        bool NormalizeFeatures(const Vector_<>& m2, ExerciseRegression_* fit) {
            bool sigmaFloor = false;
            for (size_t feature = 0; feature < fit->means_.size(); ++feature) {
                const double sigma = std::sqrt(std::max(0.0, m2[feature] / static_cast<double>(fit->numCondTrue_)));
                REQUIRE2(std::isfinite(fit->means_[feature]) && std::isfinite(sigma), "InvalidRegressionInput: multivariate feature moments overflow",
                         ScriptError_);
                const double floor = 1e-10 * std::max(1.0, std::fabs(fit->means_[feature]));
                sigmaFloor |= sigma < floor;
                fit->sigmas_[feature] = sigma < floor ? 1.0 : sigma;
            }
            return sigmaFloor;
        }

        bool TryPolynomialFit(const MultivariateRegressionRows_& rows, int degree, bool sigmaFloor, ExerciseRegression_* fit) {
            for (int candidate = degree; candidate >= 1; --candidate) {
                const auto powers = BasisPowers(rows.nFeatures_, candidate);
                if (fit->numCondTrue_ < PATHS_PER_BASIS * powers.size())
                    continue;
                fit->basisDegree_ = candidate;
                fit->powers_ = powers;
                auto ws = MakeWorkspace(rows, *fit);
                REQUIRE2(RegressionQR::Normalize(&ws, true) == ws.basis_, "InvalidRegressionInput: multivariate basis norm overflow", ScriptError_);
                const size_t rank = RegressionQR::Factorize(&ws, RANK_TOLERANCE);
                if (rank <= 1)
                    continue;
                fit->coefficients_ = RegressionQR::Recover(ws, rank);
                for (double value : fit->coefficients_)
                    REQUIRE2(std::isfinite(value), "InvalidRegressionInput: non-finite multivariate coefficients", ScriptError_);
                fit->effectiveRank_ = rank;
                fit->solver_ = "PivotedQR";
                if (rank < powers.size())
                    fit->fallbackReason_ = sigmaFloor ? "SigmaFloor" : "RankDeficient";
                else if (candidate < degree)
                    fit->fallbackReason_ = "ConditionPathsBelowMin";
                return true;
            }
            return false;
        }
    } // namespace

    ExerciseRegression_ SolveMultivariateExerciseRegression(const MultivariateRegressionRows_& rows, int degree) {
        ValidateRows(rows, degree);
        ExerciseRegression_ fit;
        fit.means_.Resize(rows.nFeatures_);
        fit.sigmas_ = Vector_<>(rows.nFeatures_, 1.0);
        Vector_<> m2(rows.nFeatures_, 0.0);
        const double targetMean = CollectMoments(rows, &fit, &m2);
        if (!fit.numCondTrue_) {
            ConstantFit(&fit, 0.0, "ConditionPathsBelowMin");
            return fit;
        }
        const bool sigmaFloor = NormalizeFeatures(m2, &fit);
        if (TryPolynomialFit(rows, degree, sigmaFloor, &fit))
            return fit;
        fit.powers_.clear();
        ConstantFit(&fit, targetMean,
                    fit.numCondTrue_ < PATHS_PER_BASIS * BasisPowers(rows.nFeatures_, 1).size() ? "ConditionPathsBelowMin"
                                                                                                : (sigmaFloor ? "SigmaFloor" : "IllConditioned"));
        return fit;
    }
} // namespace Dal::Script
