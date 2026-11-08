#pragma once

#include <algorithm>
#include <chrono>
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
    inline void ValidateTree(const Input_& r) {
        if (r.carry_.size() != 1 || (!r.american_ && r.exercise_ != std::vector<double>{r.days_}))
            throw std::runtime_error("TREE requires constant carry and American or terminal exercise");
    }
    inline void TreeLayer(const Input_& r, int layer, double u, double up, double down, std::vector<double>* value) {
        double spot = r.spot_ * std::exp(-layer * std::log(u));
        for (int j = 0; j <= layer; ++j) {
            (*value)[j] = down * (*value)[j] + up * (*value)[j + 1];
            if (r.american_)
                (*value)[j] = std::max((*value)[j], Payoff(r, spot));
            spot *= u * u;
        }
    }
    inline void Deadline(double epoch) {
        if (epoch == 0.0)
            return;
        const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now >= epoch)
            throw std::runtime_error("reference wall-clock budget exhausted");
    }
    inline double Tree(const Input_& r, int n, double deadlineEpoch = 0.0) {
        ValidateTree(r);
        const double dt = r.days_ / (365.0 * n), u = std::exp(r.volatility_ * std::sqrt(dt)), d = 1.0 / u;
        const double p = (std::exp((r.carry_[0].rate_ - r.carry_[0].yield_) * dt) - d) / (u - d);
        const double discount = std::exp(-r.carry_[0].rate_ * dt), up = discount * p, down = discount * (1.0 - p);
        if (!(p >= 0.0 && p <= 1.0))
            throw std::runtime_error("TREE invalid probability");
        std::vector<double> value(n + 1);
        for (int j = 0; j <= n; ++j)
            value[j] = Payoff(r, r.spot_ * std::exp((2 * j - n) * std::log(u)));
        for (int layer = n - 1; layer >= 0; --layer) {
            if ((layer & 255) == 0)
                Deadline(deadlineEpoch);
            TreeLayer(r, layer, u, up, down, &value);
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
    inline void StationaryOpportunity(const Input_& r, const Carry_& s, double spot, double day, double left, std::vector<double>* out) {
        const auto integral = Integral(r, day, left);
        if (s.rate_ * s.yield_ > 0.0 && s.rate_ != s.yield_ && spot > 0.0) {
            const double stationary =
                left + 365.0 * std::log(s.rate_ * r.strike_ * std::exp(-integral.first) / (s.yield_ * spot * std::exp(-integral.second))) /
                           (s.rate_ - s.yield_);
            if (stationary > left && stationary < s.end_)
                out->push_back(stationary);
        }
    }
    inline std::vector<double> Opportunities(const Input_& r, double spot, double day) {
        std::vector<double> out;
        if (!r.american_) {
            for (double exercise : r.exercise_)
                if (exercise >= day)
                    out.push_back(exercise);
            return out;
        }
        out.push_back(day);
        double start = 0.0;
        for (const auto& s : r.carry_) {
            const double left = std::max(day, start);
            if (s.end_ >= left) {
                out.push_back(s.end_);
                StationaryOpportunity(r, s, spot, day, left, &out);
            }
            start = s.end_;
        }
        return out;
    }
    inline double Envelope(const Input_& r, double spot, double day) {
        double best = 0.0;
        for (double exercise : Opportunities(r, spot, day)) {
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
    struct LogGrid_ {
        const Input_& r_;
        int n_, sub_;
        double bound_, dx_, dt_, a_ = 0.0, b_ = 0.0, c_ = 0.0;
        std::vector<double> grid_, obstacle_, value_, rhs_, inverse_, multiplier_;
        LogResult_ result_;
        LogGrid_(const Input_& r, int level) : r_(r) {
            const int nodes[] = {4097, 8193, 16385}, subdivisions[] = {16, 32, 64};
            const double bound[] = {4.0, 5.0, 6.0};
            if (level < 0 || level > 2)
                throw std::runtime_error("LOG-LCP level outside frozen range");
            n_ = nodes[level];
            sub_ = subdivisions[level];
            bound_ = bound[level];
            dx_ = 2.0 * bound[level] / (n_ - 1);
            grid_.resize(n_);
            obstacle_.resize(n_);
            value_.resize(n_);
            rhs_.resize(n_);
            inverse_.resize(n_);
            multiplier_.resize(n_);
            for (int i = 0; i < n_; ++i) {
                grid_[i] = r.strike_ * std::exp(-bound[level] + i * dx_);
                obstacle_[i] = value_[i] = Payoff(r, grid_[i]);
            }
            dt_ = 1.0 / (365.0 * sub_);
        }
        void Coefficients(int day) {
            size_t segment = 0;
            while (segment + 1 < r_.carry_.size() && r_.carry_[segment].end_ <= day)
                ++segment;
            const auto carry = r_.carry_[segment];
            const double diffusion = .5 * r_.volatility_ * r_.volatility_ / (dx_ * dx_);
            const double drift = (carry.rate_ - carry.yield_ - .5 * r_.volatility_ * r_.volatility_) / (2.0 * dx_);
            a_ = -dt_ * (diffusion - drift);
            b_ = 1.0 + dt_ * (2.0 * diffusion + carry.rate_);
            c_ = -dt_ * (diffusion + drift);
            if (a_ > 0 || c_ > 0)
                throw std::runtime_error("LOG-LCP central operator is not monotone");
            inverse_[1] = 1.0 / b_;
            for (int i = 2; i < n_ - 1; ++i) {
                multiplier_[i] = a_ * inverse_[i - 1];
                inverse_[i] = 1.0 / (b_ - multiplier_[i] * c_);
            }
        }
        void Boundaries(double target) {
            rhs_ = value_;
            value_[0] = Envelope(r_, grid_[0], target);
            value_[n_ - 1] = Envelope(r_, grid_[n_ - 1], target);
            rhs_[1] -= a_ * value_[0];
            rhs_[n_ - 2] -= c_ * value_[n_ - 1];
        }
        void Thomas() {
            for (int i = 2; i < n_ - 1; ++i)
                rhs_[i] -= multiplier_[i] * rhs_[i - 1];
            value_[n_ - 2] = rhs_[n_ - 2] * inverse_[n_ - 2];
            for (int i = n_ - 3; i > 0; --i)
                value_[i] = (rhs_[i] - c_ * value_[i + 1]) * inverse_[i];
        }
        void Projection(int day, int k) {
            if (k == 0 && std::find(r_.exercise_.begin(), r_.exercise_.end(), double(day)) != r_.exercise_.end())
                for (int i = 1; i < n_ - 1; ++i)
                    value_[i] = std::max(value_[i], obstacle_[i]);
        }
        void SorSweep() {
            for (int i = 1; i < n_ - 1; ++i) {
                const double left = i == 1 ? 0.0 : a_ * value_[i - 1];
                const double right = i == n_ - 2 ? 0.0 : c_ * value_[i + 1];
                value_[i] = std::max(obstacle_[i], value_[i] + 1.2 * ((rhs_[i] - left - right) / b_ - value_[i]));
            }
        }
        double Residual() const {
            double residual = 0.0;
            for (int i = 1; i < n_ - 1; ++i) {
                const double dual = b_ * value_[i] - rhs_[i] + (i == 1 ? 0.0 : a_ * value_[i - 1]) + (i == n_ - 2 ? 0.0 : c_ * value_[i + 1]);
                residual = std::max(residual, std::abs(std::min(value_[i] - obstacle_[i], dual)));
            }
            return residual;
        }
        void Sor() {
            for (int iteration = 1; iteration <= 100000; ++iteration) {
                SorSweep();
                const double residual = Residual();
                if (residual <= 1e-11) {
                    result_.maxResidual_ = std::max(result_.maxResidual_, residual);
                    result_.maxIterations_ = std::max(result_.maxIterations_, iteration);
                    return;
                }
            }
            throw std::runtime_error("LOG-LCP original residual failed at frozen iteration cap");
        }
        void Day(int day) {
            Coefficients(day);
            for (int k = sub_ - 1; k >= 0; --k) {
                Boundaries(day + double(k) / sub_);
                if (r_.american_)
                    Sor();
                else {
                    Thomas();
                    Projection(day, k);
                }
                ++result_.steps_;
            }
        }
        double Interpolate() const {
            const double position = (std::log(r_.spot_ / r_.strike_) + bound_) / dx_;
            const int center = std::clamp(static_cast<int>(std::round(position)), 1, n_ - 2);
            double result = 0.0;
            for (int i = center - 1; i <= center + 1; ++i) {
                double weight = 1.0;
                for (int j = center - 1; j <= center + 1; ++j)
                    if (i != j)
                        weight *= (r_.spot_ - grid_[j]) / (grid_[i] - grid_[j]);
                result += weight * value_[i];
            }
            return result;
        }
    };
    inline LogResult_ LogLcp(const Input_& r, int level) {
        LogGrid_ grid(r, level);
        for (int day = static_cast<int>(r.days_) - 1; day >= 0; --day)
            grid.Day(day);
        grid.result_.price_ = grid.Interpolate();
        return grid.result_;
    }
} // namespace PdeRiskReference
