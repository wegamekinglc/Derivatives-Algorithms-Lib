//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/operators.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    namespace CovarianceDetail {
        template <class T_> double Validate(const Matrix_<T_>& covariance) {
            const int n = covariance.Rows();
            REQUIRE(n > 0 && covariance.Cols() == n, "InvalidCovariance: matrix must be nonempty and square");
            double scale = 0.0;
            for (int i = 0; i < n; ++i)
                scale = std::max(scale, std::abs(Value(covariance(i, i))));
            const double tolerance = 64.0 * n * std::numeric_limits<double>::epsilon() * scale;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j <= i; ++j) {
                    REQUIRE(std::isfinite(Value(covariance(i, j))) && std::isfinite(Value(covariance(j, i))),
                            "InvalidCovariance: matrix must be finite");
                    REQUIRE(std::abs(Value(covariance(i, j)) - Value(covariance(j, i))) <= tolerance, "InvalidCovariance: matrix must be symmetric");
                }
            return tolerance;
        }
    } // namespace CovarianceDetail

    template <class T_> Matrix_<T_> CovarianceFactor(const Matrix_<T_>& covariance) {
        const double tolerance = CovarianceDetail::Validate(covariance);
        const int n = covariance.Rows();
        Matrix_<T_> lower(n, n, T_(0.0));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                T_ residual = covariance(i, j);
                for (int k = 0; k < j; ++k)
                    residual -= lower(i, k) * lower(j, k);
                if (i == j) {
                    REQUIRE(Value(residual) >= -tolerance, "InvalidCovariance: matrix must be positive semidefinite");
                    lower(i, j) = Value(residual) > 0.0 ? T_(Dal::sqrt(residual)) : T_(0.0);
                } else if (Value(lower(j, j)) > 0.0)
                    lower(i, j) = residual / lower(j, j);
                else
                    REQUIRE(std::abs(Value(residual)) <= tolerance, "InvalidCovariance: inconsistent null pivot");
            }
        }
        return lower;
    }
} // namespace Dal::AAD
