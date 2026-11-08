#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace PdeRiskReference {
    struct Carry_ {
        double end_, rate_, yield_;
    };
    struct Input_ {
        double spot_ = 100.0, strike_ = 100.0, volatility_ = .2, days_ = 365.0;
        bool call_ = false, american_ = false;
        std::vector<Carry_> carry_{{365.0, .05, .03}};
        std::vector<double> exercise_{365.0};
    };
    inline double Payoff(const Input_& r, double spot) { return std::max(r.call_ ? spot - r.strike_ : r.strike_ - spot, 0.0); }
    inline double Phi(double x) { return .5 * std::erfc(-x / std::sqrt(2.0)); }
    inline std::vector<double> Analytic(const Input_& r) {
        double rate = 0.0, yield = 0.0, start = 0.0;
        for (const auto& s : r.carry_) {
            rate += s.rate_ * (s.end_ - start) / 365.0;
            yield += s.yield_ * (s.end_ - start) / 365.0;
            start = s.end_;
        }
        const double t = r.days_ / 365.0, sd = r.volatility_ * std::sqrt(t);
        const double d1 = (std::log(r.spot_ / r.strike_) + rate - yield + .5 * sd * sd) / sd, d2 = d1 - sd;
        const double a = r.spot_ * std::exp(-yield), b = r.strike_ * std::exp(-rate);
        const double density = std::exp(-.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
        const double sign = r.call_ ? 1.0 : -1.0;
        std::vector<double> result{sign * (a * Phi(sign * d1) - b * Phi(sign * d2)), sign * std::exp(-yield) * Phi(sign * d1),
                                   std::exp(-yield) * density / (r.spot_ * sd), a * density * std::sqrt(t)};
        start = 0.0;
        for (const auto& s : r.carry_) {
            const double tau = (s.end_ - start) / 365.0;
            result.push_back(sign * tau * b * Phi(sign * d2));
            result.push_back(-sign * tau * a * Phi(sign * d1));
            start = s.end_;
        }
        return result;
    }
    inline double Tree(const Input_& r, int n) {
        if (r.carry_.size() != 1 || (!r.american_ && r.exercise_ != std::vector<double>{r.days_}))
            throw std::runtime_error("TREE requires constant carry and American or terminal exercise");
        const double dt = r.days_ / (365.0 * n), u = std::exp(r.volatility_ * std::sqrt(dt)), d = 1.0 / u;
        const double p = (std::exp((r.carry_[0].rate_ - r.carry_[0].yield_) * dt) - d) / (u - d);
        const double discount = std::exp(-r.carry_[0].rate_ * dt), up = discount * p, down = discount * (1.0 - p);
        if (!(p >= 0.0 && p <= 1.0))
            throw std::runtime_error("TREE invalid probability");
        std::vector<double> value(n + 1);
        // Re-anchor each layer to avoid exponential recurrence drift across O(n^2) nodes.
        for (int j = 0; j <= n; ++j)
            value[j] = Payoff(r, r.spot_ * std::exp((2 * j - n) * std::log(u)));
        for (int layer = n - 1; layer >= 0; --layer) {
            double spot = r.spot_ * std::exp(-layer * std::log(u));
            for (int j = 0; j <= layer; ++j) {
                value[j] = down * value[j] + up * value[j + 1];
                if (r.american_)
                    value[j] = std::max(value[j], Payoff(r, spot));
                spot *= u * u;
            }
        }
        return value[0];
    }
    inline std::pair<double, double> Integral(const Input_& r, double from, double to) {
        double rate = 0.0, yield = 0.0, start = 0.0;
        for (const auto& s : r.carry_) {
            const double span = std::max(0.0, std::min(to, s.end_) - std::max(from, start)) / 365.0;
            rate += span * s.rate_;
            yield += span * s.yield_;
            start = s.end_;
        }
        return {rate, yield};
    }
    inline double Envelope(const Input_& r, double spot, double day) {
        std::vector<double> opportunities;
        if (r.american_) {
            opportunities.push_back(day);
            double start = 0.0;
            for (const auto& s : r.carry_) {
                const double left = std::max(day, start);
                if (s.end_ >= left) {
                    opportunities.push_back(s.end_);
                    const auto integral = Integral(r, day, left);
                    if (s.rate_ * s.yield_ > 0.0 && s.rate_ != s.yield_ && spot > 0.0) {
                        const double stationary =
                            left + 365.0 *
                                       std::log(s.rate_ * r.strike_ * std::exp(-integral.first) / (s.yield_ * spot * std::exp(-integral.second))) /
                                       (s.rate_ - s.yield_);
                        if (stationary > left && stationary < s.end_)
                            opportunities.push_back(stationary);
                    }
                }
                start = s.end_;
            }
        } else {
            for (double exercise : r.exercise_)
                if (exercise >= day)
                    opportunities.push_back(exercise);
        }
        double best = 0.0;
        for (double exercise : opportunities) {
            const auto integral = Integral(r, day, exercise);
            const double forwardIntrinsic = spot * std::exp(-integral.second) - r.strike_ * std::exp(-integral.first);
            best = std::max(best, r.call_ ? forwardIntrinsic : -forwardIntrinsic);
        }
        return best;
    }
    struct LogResult_ {
        double price_ = 0.0, maxResidual_ = 0.0;
        int maxIterations_ = 0, steps_ = 0;
    };
    inline LogResult_ LogLcp(const Input_& r, int level) {
        const int nodes[] = {4097, 8193, 16385}, subdivisions[] = {16, 32, 64};
        const double bound[] = {4.0, 5.0, 6.0};
        if (level < 0 || level > 2)
            throw std::runtime_error("LOG-LCP level outside frozen range");
        const int n = nodes[level], sub = subdivisions[level];
        const double dx = 2.0 * bound[level] / (n - 1);
        std::vector<double> grid(n), obstacle(n), value(n), rhs(n), inverse(n), multiplier(n);
        for (int i = 0; i < n; ++i) {
            grid[i] = r.strike_ * std::exp(-bound[level] + i * dx);
            obstacle[i] = value[i] = Payoff(r, grid[i]);
        }
        LogResult_ result;
        const double dt = 1.0 / (365.0 * sub);
        for (int day = static_cast<int>(r.days_) - 1; day >= 0; --day) {
            size_t segment = 0;
            while (segment + 1 < r.carry_.size() && r.carry_[segment].end_ <= day)
                ++segment;
            const auto carry = r.carry_[segment];
            const double diffusion = .5 * r.volatility_ * r.volatility_ / (dx * dx);
            const double drift = (carry.rate_ - carry.yield_ - .5 * r.volatility_ * r.volatility_) / (2.0 * dx);
            const double a = -dt * (diffusion - drift), b = 1.0 + dt * (2.0 * diffusion + carry.rate_), c = -dt * (diffusion + drift);
            if (a > 0 || c > 0)
                throw std::runtime_error("LOG-LCP central operator is not monotone");
            inverse[1] = 1.0 / b;
            for (int i = 2; i < n - 1; ++i) {
                multiplier[i] = a * inverse[i - 1];
                inverse[i] = 1.0 / (b - multiplier[i] * c);
            }
            for (int k = sub - 1; k >= 0; --k) {
                const double target = day + double(k) / sub;
                rhs = value;
                value[0] = Envelope(r, grid[0], target);
                value[n - 1] = Envelope(r, grid[n - 1], target);
                rhs[1] -= a * value[0];
                rhs[n - 2] -= c * value[n - 1];
                if (!r.american_) {
                    for (int i = 2; i < n - 1; ++i)
                        rhs[i] -= multiplier[i] * rhs[i - 1];
                    value[n - 2] = rhs[n - 2] * inverse[n - 2];
                    for (int i = n - 3; i > 0; --i)
                        value[i] = (rhs[i] - c * value[i + 1]) * inverse[i];
                    if (k == 0 && std::find(r.exercise_.begin(), r.exercise_.end(), double(day)) != r.exercise_.end())
                        for (int i = 1; i < n - 1; ++i)
                            value[i] = std::max(value[i], obstacle[i]);
                } else {
                    bool converged = false;
                    for (int iteration = 1; iteration <= 100000; ++iteration) {
                        double residual = 0.0;
                        for (int i = 1; i < n - 1; ++i) {
                            const double left = i == 1 ? 0.0 : a * value[i - 1];
                            const double right = i == n - 2 ? 0.0 : c * value[i + 1];
                            value[i] = std::max(obstacle[i], value[i] + 1.2 * ((rhs[i] - left - right) / b - value[i]));
                        }
                        for (int i = 1; i < n - 1; ++i) {
                            const double dual = b * value[i] - rhs[i] + (i == 1 ? 0.0 : a * value[i - 1]) + (i == n - 2 ? 0.0 : c * value[i + 1]);
                            residual = std::max(residual, std::abs(std::min(value[i] - obstacle[i], dual)));
                        }
                        if (residual <= 1e-11) {
                            result.maxResidual_ = std::max(result.maxResidual_, residual);
                            result.maxIterations_ = std::max(result.maxIterations_, iteration);
                            converged = true;
                            break;
                        }
                    }
                    if (!converged)
                        throw std::runtime_error("LOG-LCP original residual failed at frozen iteration cap");
                }
                ++result.steps_;
            }
        }
        const double position = (std::log(r.spot_ / r.strike_) + bound[level]) / dx;
        const int center = std::clamp(static_cast<int>(std::round(position)), 1, n - 2);
        for (int i = center - 1; i <= center + 1; ++i) {
            double weight = 1.0;
            for (int j = center - 1; j <= center + 1; ++j)
                if (i != j)
                    weight *= (r.spot_ - grid[j]) / (grid[i] - grid[j]);
            result.price_ += weight * value[i];
        }
        return result;
    }
} // namespace PdeRiskReference
