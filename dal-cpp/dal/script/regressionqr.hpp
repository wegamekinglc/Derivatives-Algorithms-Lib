//
// Created by Codex on 2026/10/3.
//

#pragma once

#include <array>
#include <cmath>
#include <utility>

#include <dal/math/vectors.hpp>

namespace Dal::Script::RegressionQR {
    template <size_t N_> struct Workspace_ {
        size_t rows_ = 0;
        size_t basis_ = 0;
        std::array<Vector_<double>, N_> columns_;
        std::array<double, N_> scales_{};
        std::array<size_t, N_> permutation_{};
        std::array<std::array<double, N_>, N_> upper_{};
        std::array<double, N_> projection_{};
        Vector_<double> targets_;
    };

    inline bool NormalizeColumn(Vector_<double>* column, bool allowZeroColumn, double* scale) {
        double norm = 0.0;
        for (double value : *column)
            norm = std::hypot(norm, value);
        if (!std::isfinite(norm) || (norm == 0.0 && !allowZeroColumn))
            return false;
        *scale = norm == 0.0 ? 1.0 : norm;
        if (norm != 0.0)
            for (double& value : *column)
                value /= norm;
        return true;
    }

    template <size_t N_> size_t Normalize(Workspace_<N_>* ws, bool allowZeroColumns) {
        for (size_t term = 0; term < ws->basis_; ++term)
            if (!NormalizeColumn(&ws->columns_[term], allowZeroColumns, &ws->scales_[term]))
                return term;
        return ws->basis_;
    }

    template <size_t N_> std::pair<size_t, double> SelectPivot(const Workspace_<N_>& ws, size_t step) {
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

    template <size_t N_> void SwapPivot(Workspace_<N_>* ws, size_t step, size_t pivot) {
        if (pivot == step)
            return;
        std::swap(ws->columns_[step], ws->columns_[pivot]);
        std::swap(ws->scales_[step], ws->scales_[pivot]);
        std::swap(ws->permutation_[step], ws->permutation_[pivot]);
        for (size_t earlier = 0; earlier < step; ++earlier)
            std::swap(ws->upper_[earlier][step], ws->upper_[earlier][pivot]);
    }

    template <size_t N_> void OrthogonalizeRemaining(Workspace_<N_>* ws, size_t step) {
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

    template <size_t N_> size_t Factorize(Workspace_<N_>* ws, double rankTolerance) {
        size_t rank = 0;
        for (size_t step = 0; step < ws->basis_; ++step) {
            const auto [pivot, best] = SelectPivot(*ws, step);
            if (!std::isfinite(best) || best < rankTolerance * rankTolerance)
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

    template <size_t N_> Vector_<double> Recover(const Workspace_<N_>& ws, size_t rank) {
        std::array<double, N_> pivoted{};
        for (size_t step = rank; step-- > 0;) {
            double residual = ws.projection_[step];
            for (size_t term = step + 1; term < rank; ++term)
                residual -= ws.upper_[step][term] * pivoted[term];
            pivoted[step] = residual / ws.upper_[step][step];
        }
        Vector_<double> coefficients(ws.basis_, 0.0);
        for (size_t step = 0; step < rank; ++step)
            coefficients[ws.permutation_[step]] = pivoted[step] / ws.scales_[step];
        return coefficients;
    }
} // namespace Dal::Script::RegressionQR
