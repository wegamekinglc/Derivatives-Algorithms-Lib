//
// Created by wegam on 2021/1/2.
//

#pragma once

#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration BrentPhase
    Internal state-machine phase for the Brent root finder
switchable
alternative INITIALIZE
alternative HUNT
alternative BRACKETED
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_BrentPhase_enum.hpp>
    class RootFinder_ {
    public:
        virtual ~RootFinder_() = default;
        virtual double NextX() = 0;
        virtual void PutY(double) = 0;
        virtual double BracketWidth() const = 0;
    };

    struct Converged_ {
        double xTol_, fTol_;
        Converged_(double xTol, double fTol) : xTol_(xTol), fTol_(fTol) {}

        bool operator()(RootFinder_& t, double e) const {
            t.PutY(e);
            return std::abs(e) < fTol_ || t.BracketWidth() < xTol_;
        }
    };

    class BracketedBrent_ : public RootFinder_ {
        std::pair<double, double> a_, b_, c_;
        const double tol_;
        bool bisect_;
        double d_;

        friend class Brent_;
        explicit BracketedBrent_(double tol) : tol_(tol), bisect_(false), d_(0.0) {}

        void Initialize(const std::pair<double, double>& low, const std::pair<double, double>& high);

    public:
        BracketedBrent_(const std::pair<double, double>& low, const std::pair<double, double>& high, double tol);

        double NextX() override;
        void PutY(double y) override;
        [[nodiscard]] double BracketWidth() const override { return std::abs(a_.first - b_.first); }
    };

    class Brent_ : public RootFinder_ {
        BrentPhase_ phase_;
        bool increasing_;
        double stepSize_, trialX_;
        std::pair<double, double> knownPoint_;
        BracketedBrent_ engine_;

    public:
        explicit Brent_(double guess, double tol = Dal::EPSILON, double step_size = 0.0);
        double NextX() override;
        void PutY(double y) override;
        [[nodiscard]] double BracketWidth() const override;
    };
} // namespace Dal
