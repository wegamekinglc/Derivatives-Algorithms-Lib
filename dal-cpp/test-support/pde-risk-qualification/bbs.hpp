#pragma once

#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

#include "reference.hpp"

namespace PdeRiskBbs {
    struct Result_ {
        double price_ = 0.0, rootContinuation_ = 0.0, rootIntrinsic_ = 0.0;
        uint64_t nodes_ = 0, bsCalls_ = 0, obstacleHits_ = 0, terminalHits_ = 0;
        bool rootExercised_ = false;
    };
    inline void PositiveFinite(double value) {
        if (!std::isfinite(value) || value <= 0.0)
            throw std::runtime_error("BBS nonfinite or nonpositive input/node");
    }
    inline void Validate(const PdeRiskReference::Input_& r, int n) {
        if (n < 1 || n > 16385)
            throw std::runtime_error("BBS step count outside pilot range");
        PdeRiskReference::ValidateTree(r);
        for (double value : {r.spot_, r.strike_, r.volatility_, r.days_})
            PositiveFinite(value);
        for (double value : {r.carry_[0].rate_, r.carry_[0].yield_})
            if (!std::isfinite(value))
                throw std::runtime_error("BBS nonfinite carry");
    }
    inline double MillsDifferenceQuotient(double x, double h) {
        double lower = 0.0, upper = 0.0, relative = 0.0;
        // Track (M(x)-M(x+h))/h directly through the positive continued fraction.
        for (int k = 64; k > 0; --k) {
            const double a = x + lower, b = x + h + upper;
            relative = k * (1.0 - relative) / a / b;
            lower = k / a;
            upper = k / b;
        }
        return (1.0 - relative) / (x + lower) / (x + h + upper);
    }
    inline double EuropeanTail(const PdeRiskReference::Input_& r, double spot, double dt, double sd, double x) {
        const double logAmount = r.call_ ? std::log(spot) - r.carry_[0].yield_ * dt : std::log(r.strike_) - r.carry_[0].rate_ * dt;
        const double logValue =
            logAmount - .5 * x * x - .5 * std::log(2.0 * std::acos(-1.0)) + std::log(sd) + std::log(MillsDifferenceQuotient(x, sd));
        return std::exp(logValue);
    }
    [[noreturn]] inline void EuropeanError(const PdeRiskReference::Input_& r, double spot, double dt, double sd, double d1, double d2, double value) {
        std::ostringstream message;
        message << std::setprecision(std::numeric_limits<double>::max_digits10) << "BBS invalid European value: call=" << r.call_ << " spot=" << spot
                << " strike=" << r.strike_ << " vol=" << r.volatility_ << " rate=" << r.carry_[0].rate_ << " yield=" << r.carry_[0].yield_
                << " dt=" << dt << " sd=" << sd << " d1=" << d1 << " d2=" << d2 << " value=" << value;
        throw std::runtime_error(message.str());
    }
    inline double European(const PdeRiskReference::Input_& r, double spot, double dt) {
        const double sd = r.volatility_ * std::sqrt(dt);
        const double d1 = (std::log(spot / r.strike_) + (r.carry_[0].rate_ - r.carry_[0].yield_) * dt + .5 * sd * sd) / sd;
        const double d2 = d1 - sd, sign = r.call_ ? 1.0 : -1.0;
        const double tail = r.call_ ? -d1 : d2;
        const double value = tail >= 8.0 ? EuropeanTail(r, spot, dt, sd, tail)
                                         : sign * (spot * std::exp(-r.carry_[0].yield_ * dt) * PdeRiskReference::Phi(sign * d1) -
                                                   r.strike_ * std::exp(-r.carry_[0].rate_ * dt) * PdeRiskReference::Phi(sign * d2));
        if (!std::isfinite(value) || value < 0.0 || !std::isfinite(sd) || sd <= 0.0)
            EuropeanError(r, spot, dt, sd, d1, d2, value);
        return value;
    }
    inline double Obstacle(const PdeRiskReference::Input_& r, double spot, double continuation, bool terminal, bool root, Result_* result) {
        const double intrinsic = PdeRiskReference::Payoff(r, spot);
        const bool exercise = r.american_ && intrinsic > continuation;
        if (exercise) {
            ++result->obstacleHits_;
            if (terminal)
                ++result->terminalHits_;
        }
        if (root) {
            result->rootContinuation_ = continuation;
            result->rootIntrinsic_ = intrinsic;
            result->rootExercised_ = exercise;
        }
        return r.american_ ? std::max(intrinsic, continuation) : continuation;
    }
    inline void
    Initialize(const PdeRiskReference::Input_& r, int n, double u, double dt, double deadline, std::vector<double>* value, Result_* result) {
        for (int j = 0; j < n; ++j) {
            if ((j & 255) == 0)
                PdeRiskReference::Deadline(deadline);
            ++result->nodes_;
            const double spot = r.spot_ * std::exp((2 * j - (n - 1)) * std::log(u));
            PositiveFinite(spot);
            ++result->bsCalls_;
            const double bs = European(r, spot, dt);
            (*value)[j] = Obstacle(r, spot, bs, true, n == 1, result);
        }
    }
    inline void Layer(const PdeRiskReference::Input_& r, int layer, double u, double up, double down, std::vector<double>* value, Result_* result) {
        double spot = r.spot_ * std::exp(-layer * std::log(u));
        for (int j = 0; j <= layer; ++j) {
            ++result->nodes_;
            const double continuation = down * (*value)[j] + up * (*value)[j + 1];
            if (!std::isfinite(continuation))
                throw std::runtime_error("BBS nonfinite rollback value");
            PositiveFinite(spot);
            (*value)[j] = Obstacle(r, spot, continuation, false, layer == 0, result);
            spot *= u * u;
        }
    }
    inline void Evaluate(const PdeRiskReference::Input_& r, int n, double deadline, Result_* result) {
        Validate(r, n);
        PdeRiskReference::Deadline(deadline);
        const double dt = r.days_ / (365.0 * n), u = std::exp(r.volatility_ * std::sqrt(dt)), d = 1.0 / u;
        const double p = (std::exp((r.carry_[0].rate_ - r.carry_[0].yield_) * dt) - d) / (u - d);
        if (!(p >= 0.0 && p <= 1.0))
            throw std::runtime_error("BBS invalid probability");
        const double discount = std::exp(-r.carry_[0].rate_ * dt), up = discount * p, down = discount * (1.0 - p);
        std::vector<double> value(n);
        Initialize(r, n, u, dt, deadline, &value, result);
        for (int layer = n - 2; layer >= 0; --layer) {
            if ((layer & 255) == 0)
                PdeRiskReference::Deadline(deadline);
            Layer(r, layer, u, up, down, &value, result);
        }
        PdeRiskReference::Deadline(deadline);
        result->price_ = value[0];
    }
} // namespace PdeRiskBbs
