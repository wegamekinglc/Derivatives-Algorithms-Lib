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

        struct QrWorkspace_ {
            size_t rows_ = 0;
            size_t basis_ = 0;
            std::array<Vector_<>, MAX_BASIS> columns_;
            std::array<double, MAX_BASIS> scales_{};
            std::array<size_t, MAX_BASIS> permutation_{};
            std::array<std::array<double, MAX_BASIS>, MAX_BASIS> upper_{};
            std::array<double, MAX_BASIS> projection_{};
            Vector_<> targets_;
        };

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

        void Normalize(QrWorkspace_* ws) {
            for (size_t term = 0; term < ws->basis_; ++term) {
                double norm = 0.0;
                for (double value : ws->columns_[term])
                    norm = std::hypot(norm, value);
                REQUIRE2(std::isfinite(norm), "InvalidRegressionInput: multivariate basis norm overflow", ScriptError_);
                ws->scales_[term] = norm == 0.0 ? 1.0 : norm;
                if (norm != 0.0)
                    for (double& value : ws->columns_[term])
                        value /= norm;
            }
        }

        std::pair<size_t, double> SelectPivot(const QrWorkspace_& ws, size_t step) {
            size_t pivot = step;
            double best = 0.0;
            for (size_t term = step; term < ws.basis_; ++term) {
                double squared = 0.0;
                for (double value : ws.columns_[term])
                    squared += value * value;
                if (squared > best) {
                    best = squared;
                    pivot = term;
                }
            }
            return {pivot, best};
        }

        void SwapPivot(QrWorkspace_* ws, size_t step, size_t pivot) {
            if (pivot == step)
                return;
            std::swap(ws->columns_[step], ws->columns_[pivot]);
            std::swap(ws->scales_[step], ws->scales_[pivot]);
            std::swap(ws->permutation_[step], ws->permutation_[pivot]);
            for (size_t earlier = 0; earlier < step; ++earlier)
                std::swap(ws->upper_[earlier][step], ws->upper_[earlier][pivot]);
        }

        void OrthogonalizeRemaining(QrWorkspace_* ws, size_t step) {
            for (size_t term = step + 1; term < ws->basis_; ++term)
                for (int pass = 0; pass < 2; ++pass) {
                    double dot = 0.0;
                    for (size_t row = 0; row < ws->rows_; ++row)
                        dot += ws->columns_[step][row] * ws->columns_[term][row];
                    ws->upper_[step][term] += dot;
                    for (size_t row = 0; row < ws->rows_; ++row)
                        ws->columns_[term][row] -= dot * ws->columns_[step][row];
                }
        }

        size_t PivotedQr(QrWorkspace_* ws) {
            size_t rank = 0;
            for (size_t step = 0; step < ws->basis_; ++step) {
                const auto [pivot, best] = SelectPivot(*ws, step);
                if (!std::isfinite(best) || best < RANK_TOLERANCE * RANK_TOLERANCE)
                    break;
                SwapPivot(ws, step, pivot);
                ws->upper_[step][step] = std::sqrt(best);
                for (double& value : ws->columns_[step])
                    value /= ws->upper_[step][step];
                for (size_t row = 0; row < ws->rows_; ++row)
                    ws->projection_[step] += ws->columns_[step][row] * ws->targets_[row];
                OrthogonalizeRemaining(ws, step);
                ++rank;
            }
            return rank;
        }

        Vector_<> Recover(const QrWorkspace_& ws, size_t rank) {
            std::array<double, MAX_BASIS> pivoted{};
            for (size_t step = rank; step-- > 0;) {
                double residual = ws.projection_[step];
                for (size_t term = step + 1; term < rank; ++term)
                    residual -= ws.upper_[step][term] * pivoted[term];
                pivoted[step] = residual / ws.upper_[step][step];
            }
            Vector_<> coefficients(ws.basis_, 0.0);
            for (size_t step = 0; step < rank; ++step)
                coefficients[ws.permutation_[step]] = pivoted[step] / ws.scales_[step];
            for (double value : coefficients)
                REQUIRE2(std::isfinite(value), "InvalidRegressionInput: non-finite multivariate coefficients", ScriptError_);
            return coefficients;
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
                Normalize(&ws);
                const size_t rank = PivotedQr(&ws);
                if (rank <= 1)
                    continue;
                fit->coefficients_ = Recover(ws, rank);
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
