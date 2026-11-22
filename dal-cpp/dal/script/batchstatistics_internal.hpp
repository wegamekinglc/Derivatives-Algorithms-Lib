#pragma once

#include <cmath>
#include <dal/calibration/validation_internal.hpp>

namespace Dal::BatchStatisticsInternal {
    inline Vector_<> Means(const Vector_<Vector_<>>& rows, size_t width) {
        Vector_<> means(width, 0.);
        for (const auto& row : rows)
            for (size_t i = 0; i < width; ++i)
                means[i] += row[i] / rows.size();
        return means;
    }

    inline Matrix_<> Covariance(
        const Vector_<Vector_<>>& rows, const Vector_<>& means, const char* field, const char* expected, const CalibrationDiagnostic_& context = {}) {
        const int width = static_cast<int>(means.size());
        Matrix_<> covariance(width, width, 0.);
        for (const auto& row : rows)
            for (int i = 0; i < width; ++i)
                for (int j = 0; j < width; ++j)
                    covariance(i, j) += (row[i] - means[i]) * (row[j] - means[j]) / (rows.size() - 1);
        for (int i = 0; i < width; ++i)
            for (int j = 0; j < width; ++j)
                if (!std::isfinite(covariance(i, j)))
                    CalibrationInternal::Fail("CalibrationNumericalFailure", field, expected, "", context);
        return covariance;
    }

    inline double Project(const Vector_<>& row, const Vector_<>& means, const Vector_<>& weights) {
        double influence = 0.;
        for (size_t i = 0; i < weights.size(); ++i)
            influence += weights[i] * (row[i] - means[i]);
        return influence;
    }

    template <class F_> double StandardError(size_t blocks, const F_& deviation) {
        double norm = 0.;
        const double scale = std::sqrt(static_cast<double>(blocks) * (blocks - 1));
        for (size_t block = 0; block < blocks; ++block)
            norm = std::hypot(norm, deviation(block) / scale);
        return norm;
    }

    inline double ProjectedError(const Vector_<Vector_<>>& rows, const Vector_<>& means, const Vector_<>& weights) {
        return StandardError(rows.size(), [&](size_t block) { return Project(rows[block], means, weights); });
    }
} // namespace Dal::BatchStatisticsInternal
