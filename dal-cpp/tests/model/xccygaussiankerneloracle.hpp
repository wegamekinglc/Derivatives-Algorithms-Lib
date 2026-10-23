#pragma once

#include <array>
#include <cmath>

#include <dal/model/xccygaussiandata.hpp>

namespace XccyGaussianTestSupport {
    inline Dal::Matrix_<> Correlation() {
        Dal::Matrix_<> result(3, 3, 0.0);
        const double values[3][3] = {{1., .30, -.20}, {.30, 1., -.25}, {-.20, -.25, 1.}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result(i, j) = values[i][j];
        return result;
    }

    inline long double Kernel(int slot, long double remaining, long double ad, long double af) {
        const long double a = slot == 0 || slot == 2 ? ad : af;
        if (slot < 2)
            return std::exp(-a * remaining);
        if (slot == 4)
            return 1.L;
        return a == 0.L ? remaining : -std::expm1(-a * remaining) / a;
    }

    inline std::pair<long double, long double> GaussNode(int order, int k) {
        const long double pi = std::acos(-1.L);
        long double root = std::cos(pi * (k - .25L) / (order + .5L)), derivative = 0.L;
        for (int iteration = 0; iteration < 30; ++iteration) {
            long double p = 1.L, prior = 0.L;
            for (int n = 1; n <= order; ++n) {
                const long double next = ((2 * n - 1) * root * p - (n - 1) * prior) / n;
                prior = p;
                p = next;
            }
            derivative = order * (root * p - prior) / (root * root - 1.L);
            const long double step = p / derivative;
            root -= step;
            if (std::abs(step) < 1e-19L)
                break;
        }
        return {root, 2.L / ((1.L - root * root) * derivative * derivative)};
    }

    template <class F_> long double Integrate(const F_& function, long double from, long double to, int order) {
        long double sum = 0.L;
        for (int k = 1; k <= order; ++k) {
            const auto node = GaussNode(order, k);
            sum += node.second * function((to + from + (to - from) * node.first) / 2.L) * (to - from) / 2.L;
        }
        return sum;
    }

    inline long double KernelIntegral(int i, int j, long double ad, long double af, long double h, int order) {
        return Integrate([=](long double r) { return Kernel(i, r, ad, af) * Kernel(j, r, ad, af); }, 0.L, h, order);
    }

    struct OracleMoments_ {
        std::array<long double, 5> meanD_{}, meanF_{};
        std::array<std::array<long double, 5>, 5> covariance_{};
    };

    inline OracleMoments_ OracleMoments(const Dal::XccyGaussianModelSpec_& spec, long double from, long double to, int order = 48) {
        OracleMoments_ result;
        const auto& dynamics = spec.dynamics_;
        const int driver[5] = {0, 1, 0, 1, 2};
        long double prior = 0.L;
        for (size_t bucket = 0; bucket < dynamics.bucketEndDates_.size(); ++bucket) {
            const long double end = (dynamics.bucketEndDates_[bucket] - spec.market_.valuationTime_.Date()) / 365.L;
            const long double left = std::max(from, prior), right = std::min(to, end);
            prior = end;
            if (left >= right)
                continue;
            const long double sigma[3] = {dynamics.domesticSigma_[bucket], dynamics.foreignSigma_[bucket], dynamics.fxSigma_[bucket]};
            for (int i = 0; i < 5; ++i) {
                const auto kernel = [&](long double u) { return Kernel(i, to - u, dynamics.domesticA_, dynamics.foreignA_); };
                if (driver[i] == 1)
                    result.meanD_[i] -= dynamics.correlation_(1, 2) * sigma[1] * sigma[2] * Integrate(kernel, left, right, order);
                if (driver[i] == 0)
                    result.meanF_[i] += dynamics.correlation_(0, 2) * sigma[0] * sigma[2] * Integrate(kernel, left, right, order);
                for (int j = 0; j < 5; ++j) {
                    const auto product = [&](long double u) { return kernel(u) * Kernel(j, to - u, dynamics.domesticA_, dynamics.foreignA_); };
                    result.covariance_[i][j] +=
                        sigma[driver[i]] * sigma[driver[j]] * dynamics.correlation_(driver[i], driver[j]) * Integrate(product, left, right, order);
                }
            }
        }
        return result;
    }
} // namespace XccyGaussianTestSupport
