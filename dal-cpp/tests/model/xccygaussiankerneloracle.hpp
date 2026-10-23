#pragma once

#include <array>
#include <cmath>
#include <limits>

#include <dal/model/xccygaussiandata.hpp>

namespace XccyGaussianTestSupport {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);

    struct Wide_ {
        double high_, low_;
        explicit Wide_(long double value = 0.L) : high_(static_cast<double>(value)), low_(static_cast<double>(value - high_)) {}
        Wide_(double high, double low) : high_(high), low_(low) {}
        explicit operator long double() const { return static_cast<long double>(high_) + low_; }
    };

    inline Wide_ Renormalize(double high, double low) {
        const double sum = high + low;
        return {sum, low - (sum - high)};
    }

    inline Wide_ operator+(const Wide_& lhs, const Wide_& rhs) {
        const double sum = lhs.high_ + rhs.high_, rhsPart = sum - lhs.high_;
        const double error = (lhs.high_ - (sum - rhsPart)) + (rhs.high_ - rhsPart) + lhs.low_ + rhs.low_;
        return Renormalize(sum, error);
    }

    inline Wide_ operator-(const Wide_& value) { return {-value.high_, -value.low_}; }
    inline Wide_ operator-(const Wide_& lhs, const Wide_& rhs) { return lhs + -rhs; }

    inline Wide_ operator*(const Wide_& lhs, const Wide_& rhs) {
        const double product = lhs.high_ * rhs.high_;
        const double error = std::fma(lhs.high_, rhs.high_, -product) + lhs.high_ * rhs.low_ + lhs.low_ * rhs.high_ + lhs.low_ * rhs.low_;
        return Renormalize(product, error);
    }

    inline Wide_ operator/(const Wide_& lhs, const Wide_& rhs) {
        const Wide_ first(lhs.high_ / rhs.high_);
        const Wide_ residual = lhs - rhs * first, second(residual.high_ / rhs.high_);
        const Wide_ remainder = residual - rhs * second;
        return first + second + Wide_(remainder.high_ / rhs.high_);
    }

    inline Wide_ SmallExpm1(const Wide_& x) {
        Wide_ sum = x, term = x;
        for (int n = 2; n <= 40; ++n) {
            term = term * x / Wide_(n);
            sum = sum + term;
            if (std::abs(term.high_) <= 1e-32 * std::abs(sum.high_))
                break;
        }
        return sum;
    }

    inline Wide_ Exp(const Wide_& x) {
        Wide_ reduced = x;
        int halvings = 0;
        while (std::abs(reduced.high_) > .125) {
            reduced = reduced * Wide_(.5L);
            ++halvings;
        }
        Wide_ result = Wide_(1.L) + SmallExpm1(reduced);
        for (int i = 0; i < halvings; ++i)
            result = result * result;
        return result;
    }

    inline Wide_ Expm1(const Wide_& x) { return std::abs(x.high_) <= .125 ? SmallExpm1(x) : Exp(x) - Wide_(1.L); }

    inline Dal::Matrix_<> Correlation() {
        Dal::Matrix_<> result(3, 3, 0.0);
        const double values[3][3] = {{1., .30, -.20}, {.30, 1., -.25}, {-.20, -.25, 1.}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result(i, j) = values[i][j];
        return result;
    }

    inline Wide_ Kernel(int slot, const Wide_& remaining, const Wide_& ad, const Wide_& af) {
        const Wide_ a = slot == 0 || slot == 2 ? ad : af;
        if (slot < 2)
            return Exp(-a * remaining);
        if (slot == 4)
            return Wide_(1.L);
        return a.high_ == 0. ? remaining : -Expm1(-a * remaining) / a;
    }

    inline std::pair<Wide_, Wide_> Legendre(int order, const Wide_& root) {
        Wide_ p(1.L), prior(0.L);
        for (int n = 1; n <= order; ++n) {
            const Wide_ next = (Wide_(2 * n - 1) * root * p - Wide_(n - 1) * prior) / Wide_(n);
            prior = p;
            p = next;
        }
        return {p, Wide_(order) * (root * p - prior) / (root * root - Wide_(1.L))};
    }

    inline std::pair<Wide_, Wide_> GaussNode(int order, int k) {
        Wide_ root(std::cos(std::acos(-1.) * (k - .25) / (order + .5)));
        bool converged = false;
        for (int iteration = 0; iteration < 30; ++iteration) {
            const auto polynomial = Legendre(order, root);
            const Wide_ step = polynomial.first / polynomial.second;
            root = root - step;
            if (std::abs(step.high_) < 1e-30) {
                converged = true;
                break;
            }
        }
        REQUIRE(converged, "Independent Gaussian quadrature root did not converge");
        const Wide_ derivative = Legendre(order, root).second;
        return {root, Wide_(2.L) / ((Wide_(1.L) - root * root) * derivative * derivative)};
    }

    template <int Order_> const auto& GaussRule() {
        static const auto rule = [] {
            std::array<std::pair<Wide_, Wide_>, Order_> result;
            for (int k = 0; k < Order_; ++k)
                result[k] = GaussNode(Order_, k + 1);
            return result;
        }();
        return rule;
    }

    template <class F_, size_t Order_>
    Wide_ IntegrateRule(const F_& function, const Wide_& from, const Wide_& to, const std::array<std::pair<Wide_, Wide_>, Order_>& rule) {
        const Wide_ midpoint = (to + from) * Wide_(.5L), halfWidth = (to - from) * Wide_(.5L);
        Wide_ sum;
        for (const auto& node : rule)
            sum = sum + node.second * Wide_(function(midpoint + halfWidth * node.first)) * halfWidth;
        return sum;
    }

    template <class F_> long double Integrate(const F_& function, long double from, long double to, int order) {
        REQUIRE(order == 48 || order == 64, "Independent kernel oracle requires the frozen 48/64 rules");
        const Wide_ result = order == 48 ? IntegrateRule(function, Wide_(from), Wide_(to), GaussRule<48>())
                                         : IntegrateRule(function, Wide_(from), Wide_(to), GaussRule<64>());
        return static_cast<long double>(result);
    }

    inline long double KernelIntegral(int i, int j, long double ad, long double af, long double h, int order) {
        return Integrate([=](const Wide_& r) { return Kernel(i, r, Wide_(ad), Wide_(af)) * Kernel(j, r, Wide_(ad), Wide_(af)); }, 0.L, h, order);
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
                const auto kernel = [&](const Wide_& u) { return Kernel(i, Wide_(to) - u, Wide_(dynamics.domesticA_), Wide_(dynamics.foreignA_)); };
                if (driver[i] == 1)
                    result.meanD_[i] -= dynamics.correlation_(1, 2) * sigma[1] * sigma[2] * Integrate(kernel, left, right, order);
                if (driver[i] == 0)
                    result.meanF_[i] += dynamics.correlation_(0, 2) * sigma[0] * sigma[2] * Integrate(kernel, left, right, order);
                for (int j = 0; j < 5; ++j) {
                    const auto product = [&](const Wide_& u) {
                        return kernel(u) * Kernel(j, Wide_(to) - u, Wide_(dynamics.domesticA_), Wide_(dynamics.foreignA_));
                    };
                    result.covariance_[i][j] +=
                        sigma[driver[i]] * sigma[driver[j]] * dynamics.correlation_(driver[i], driver[j]) * Integrate(product, left, right, order);
                }
            }
        }
        return result;
    }
} // namespace XccyGaussianTestSupport
