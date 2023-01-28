//
// Created by wegam on 2021/1/2.
//

#include <dal/math/rootfind.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    BracketedBrent_::BracketedBrent_(const std::pair<double, double>& low,
                                     const std::pair<double, double>& high,
                                     double tol)
        : tol_(tol) {
        Initialize(low, high);
    }

    void BracketedBrent_::Initialize(const std::pair<double, double>& low, const std::pair<double, double>& high) {
        a_ = low;
        b_ = high;
        REQUIRE(a_.second * b_.second <= 0.0, "Interval is not bracketed");
        if (std::fabs(a_.second) < std::fabs(b_.second))
            std::swap(a_, b_);
        c_ = a_;
        bisect_ = true;
    }

    double BracketedBrent_::NextX() {
        double s;
        if (c_.second != a_.second && c_.second != b_.second) { // inverse quadratic interpolation
            s = a_.first * b_.second * c_.second / ((a_.second - b_.second) * (a_.second - c_.second)) +
                b_.first * a_.second * c_.second / ((b_.second - a_.second) * (b_.second - c_.second)) +
                c_.first * a_.second * b_.second / ((c_.second - a_.second) * (c_.second - b_.second));
        } else { // secant interpolation
            s = (a_.first * b_.second - b_.first * a_.second) / (b_.second - a_.second);
        }
        double cDist = fabs(
            c_.first - (bisect_ ? b_.first : d_)); // d_ won't be used before it is set, because bisect_ is set at start
        bisect_ = (s - b_.first) * (s - 0.75 * a_.first - 0.25 * b_.first) >= 0.0 || fabs(s - b_.first) > 0.5 * cDist ||
                  cDist < tol_;
        if (bisect_)
            s = 0.5 * (a_.first + b_.first);
        // done with d_; use it to hold s
        d_ = s;
        return d_;
    }

    void BracketedBrent_::PutY(double f_s) {
        const double s = d_;
        d_ = c_.first;
        c_ = b_;
        if (f_s * a_.second > 0)
            a_ = std::make_pair(s, f_s);
        else
            b_ = std::make_pair(s, f_s);

        if (std::fabs(a_.second) < std::fabs(b_.second))
            std::swap(a_, b_);
    }

    // preliminary hunt phase, followed by BracketedBrent

    Brent_::Brent_(double guess, double tol, double step_size)
        : phase_(Phase_::INITIALIZE), increasing_(true),
          stepSize_(step_size > 0.0 ? step_size : 0.1 * std::max(0.01, std::fabs(guess))), trialX_(guess),
          knownPoint_(Dal::INF, Dal::INF), engine_(tol) {}

    double Brent_::NextX() { return phase_ == Phase_::BRACKETED ? engine_.NextX() : trialX_; }

    void Brent_::PutY(double y) {
        static const double EXPANSION = std::exp(1.0);
        switch (phase_) {
        case Phase_::INITIALIZE:
            knownPoint_ = std::make_pair(trialX_, y);
            trialX_ += stepSize_ * (increasing_ ? 1 : -1) * (y < 0.0 ? 1 : -1);
            phase_ = Phase_::HUNT;
            break;
        case Phase_::BRACKETED:
            engine_.PutY(y);
            break;
        case Phase_::HUNT:
            if (y * knownPoint_.second <= 0.0) {
                engine_.Initialize(knownPoint_, std::make_pair(trialX_, y));
                phase_ = Phase_::BRACKETED;
            } else {
                if (std::fabs(y) > std::fabs(knownPoint_.second))
                    increasing_ = !increasing_;
                else
                    knownPoint_ = std::make_pair(trialX_, y);
                stepSize_ *= EXPANSION;
                trialX_ = knownPoint_.first + (increasing_ ? 1 : -1) * (knownPoint_.second < 0.0 ? 1 : -1) * stepSize_;
            }
            break;
        }
    }

    double Brent_::BracketWidth() const { return phase_ == Phase_::BRACKETED ? engine_.BracketWidth() : Dal::INF; }
} // namespace Dal