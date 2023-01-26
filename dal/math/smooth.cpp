//
// Created by wegam on 2022/4/3.
//

#include <dal/math/smooth.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    Vector_<> SmoothedVals(const Vector_<>& x, const Vector_<>& y, const Vector_<>& weight, double lambda) {
        static constexpr double DX_MIN = 1.e-9;
        REQUIRE(IsMonotonic(x, std::less_equal<double>()), "x values should monotonic ascending");
        const int n = x.size();
        REQUIRE(y.size() == n && n > 1, "y values size should be same with x and size should bigger than 1");
        Vector_<> coupling(n);
        for (int i = 1; i < n; ++i)
            coupling[i] = lambda / max(x[i] - x[i-1], DX_MIN);
        coupling.back() = 0.0;

        Vector_<> beta(n), gamma(n);
        // initialize forward recursion
        beta[0] = (weight.empty() ? 1.0 : weight[0]) + coupling[0];
        gamma[0] = (weight.empty() ? 1.0 : weight[0]) * y[0];
        for (int ii = 1; ii < n; ++ii)
        {
            const double w = weight.empty() ? 1.0 : weight[ii];
            gamma[ii] = w * y[ii] + coupling[ii - 1] * gamma[ii - 1] / beta[ii - 1];
            beta[ii] = w + coupling[ii - 1] + coupling[ii] - Square(coupling[ii - 1]) / beta[ii - 1]; // at end of loop, coupling[ii] is zero because it doesn't exist
        }
        // back-substitution for final result
        Vector_<> z(n);
        z.back() = gamma.back() / beta.back();
        for (int jj = n - 2; jj >= 0; --jj)
            z[jj] = (gamma[jj] + coupling[jj] * z[jj + 1]) / beta[jj];
        return z;

    }
}
