//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include <dal/math/matrix/covariance.hpp>
#include <dal/model/gsrcalibration.hpp>

namespace Dal::GSRCalibrationInternal {
    template <class Problem_> Matrix_<> DifferenceJacobian(const Problem_& problem, const Vector_<>& x, size_t rows, double relativeStep) {
        Matrix_<> result(rows, x.size(), 0.0);
        for (size_t col = 0; col < x.size(); ++col) {
            auto up = x, down = x;
            const double step = relativeStep * std::max(1.0, std::abs(x[col]));
            up[col] = std::min(problem.Bound(col, true), x[col] + step);
            down[col] = std::max(problem.Bound(col, false), x[col] - step);
            REQUIRE(std::isfinite(step) && up[col] > down[col], "InvalidGSRCalibration: difference step is unresolved or nonfinite");
            const auto high = problem.Residuals(up), low = problem.Residuals(down);
            for (size_t row = 0; row < rows; ++row)
                result(row, col) = (high[row] - low[row]) / (up[col] - down[col]);
        }
        return result;
    }
    inline double NormSquared(const Vector_<>& values) {
        const double norm = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
        REQUIRE(std::isfinite(norm), "InvalidGSRCalibration: residual or derivative norm overflow");
        return norm;
    }

    inline Vector_<> Gradient(const Matrix_<>& jacobian, const Vector_<>& residuals) {
        Vector_<> gradient(jacobian.Cols(), 0.0);
        for (int col = 0; col < jacobian.Cols(); ++col)
            for (int row = 0; row < jacobian.Rows(); ++row)
                gradient[col] += jacobian(row, col) * residuals[row];
        return gradient;
    }

    inline double GradientScale(const Matrix_<>& jacobian) {
        double scale = 1.0;
        for (int col = 0; col < jacobian.Cols(); ++col) {
            double norm = 0.0;
            for (int row = 0; row < jacobian.Rows(); ++row)
                norm += jacobian(row, col) * jacobian(row, col);
            scale = std::max(scale, std::sqrt(norm));
        }
        return scale;
    }

    template <class Problem_> double ProjectedGradient(const Problem_& problem, const Vector_<>& x, const Vector_<>& gradient) {
        double norm = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            const bool blocked = (x[i] <= problem.Bound(i, false) && gradient[i] > 0.0) || (x[i] >= problem.Bound(i, true) && gradient[i] < 0.0);
            if (!blocked)
                norm = std::max(norm, std::abs(gradient[i]));
        }
        return norm;
    }

    inline Vector_<> DampedStep(const Matrix_<>& jacobian, const Vector_<>& gradient, double damping) {
        const int n = jacobian.Cols();
        Matrix_<> hessian(n, n, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j)
                for (int row = 0; row < jacobian.Rows(); ++row)
                    hessian(i, j) += jacobian(row, i) * jacobian(row, j);
            hessian(i, i) += damping * std::max(1.0, hessian(i, i));
        }
        const auto lower = AAD::CovarianceFactor(hessian);
        Vector_<> step(n, 0.0);
        for (int i = 0; i < n; ++i) {
            double value = -gradient[i];
            for (int j = 0; j < i; ++j)
                value -= lower(i, j) * step[j];
            step[i] = value / lower(i, i);
        }
        for (int i = n; i-- > 0;) {
            for (int j = i + 1; j < n; ++j)
                step[i] -= lower(j, i) * step[j];
            step[i] /= lower(i, i);
        }
        return step;
    }

    inline void Orthogonalize(const Vector_<>& direction, Vector_<Vector_<>>* columns, size_t start) {
        for (size_t col = start; col < columns->size(); ++col)
            for (int pass = 0; pass < 2; ++pass) {
                const double dot = std::inner_product(direction.begin(), direction.end(), (*columns)[col].begin(), 0.0);
                for (size_t row = 0; row < (*columns)[col].size(); ++row)
                    (*columns)[col][row] -= dot * direction[row];
            }
    }

    template <class Result_> void RankDiagnostics(const Matrix_<>& jacobian, Result_* result) {
        Vector_<Vector_<>> columns;
        double largest = 0.0, smallest = std::numeric_limits<double>::infinity();
        for (int col = 0; col < jacobian.Cols(); ++col) {
            Vector_<> column(jacobian.Rows(), 0.0);
            for (int row = 0; row < jacobian.Rows(); ++row)
                column[row] = jacobian(row, col);
            largest = std::max(largest, std::sqrt(NormSquared(column)));
            columns.push_back(column);
        }
        for (size_t step = 0; step < columns.size(); ++step) {
            const auto best =
                std::max_element(columns.begin() + step, columns.end(), [](const auto& a, const auto& b) { return NormSquared(a) < NormSquared(b); });
            std::swap(columns[step], *best);
            const double norm = std::sqrt(NormSquared(columns[step]));
            if (norm <= largest * 1e-8)
                break;
            ++result->jacobianRank_;
            smallest = std::min(smallest, norm);
            for (auto& value : columns[step])
                value /= norm;
            Orthogonalize(columns[step], &columns, step + 1);
        }
        result->jacobianConditionEstimate_ = result->jacobianRank_ == jacobian.Cols() ? largest / smallest : std::numeric_limits<double>::infinity();
    }

    template <class Problem_, class Result_>
    void Fit(const Problem_& problem, const GSRCalibrationSettings_& settings, Vector_<>* x, Result_* result) {
        auto residuals = problem.Residuals(*x);
        ++result->evaluations_;
        double objective = NormSquared(residuals), damping = 1e-3;
        result->terminationReason_ = "MaxIterations";
        for (int iteration = 0; iteration < settings.maxIterations_; ++iteration) {
            result->iterations_ = iteration + 1;
            const auto jacobian = problem.Jacobian(*x, residuals.size());
            result->evaluations_ += 2 * static_cast<int>(x->size());
            const auto gradient = Gradient(jacobian, residuals);
            if (ProjectedGradient(problem, *x, gradient) <=
                settings.gradientTolerance_ * GradientScale(jacobian) * std::max(1.0, std::sqrt(objective))) {
                result->converged_ = true;
                result->terminationReason_ = "ProjectedGradient";
                break;
            }
            bool accepted = false;
            for (int trial = 0; trial < 16; ++trial) {
                const auto step = DampedStep(jacobian, gradient, damping);
                auto candidate = *x;
                double distance = 0.0;
                for (size_t i = 0; i < x->size(); ++i) {
                    candidate[i] = std::clamp((*x)[i] + step[i], problem.Bound(i, false), problem.Bound(i, true));
                    distance = std::max(distance, std::abs(candidate[i] - (*x)[i]));
                }
                const auto nextResiduals = problem.Residuals(candidate);
                ++result->evaluations_;
                const double nextObjective = NormSquared(nextResiduals);
                if (nextObjective < objective) {
                    *x = std::move(candidate);
                    residuals = nextResiduals;
                    objective = nextObjective;
                    damping = std::max(damping / 3.0, 1e-12);
                    accepted = true;
                    if (distance <= settings.stepTolerance_) {
                        result->terminationReason_ = "StepStalled";
                        result->objective_ = objective;
                        return;
                    }
                    break;
                }
                damping *= 10.0;
            }
            if (!accepted) {
                result->terminationReason_ = "NoDescent";
                break;
            }
        }
        result->objective_ = objective;
    }
} // namespace Dal::GSRCalibrationInternal
