//
// Created by wegam on 2025/7/6.
//

#pragma once

#include <optional>
#include <utility>

#include <dal/math/operators.hpp>
#include <dal/platform/host.hpp>

//  Smoothed condition kernels shared by fuzzy evaluators.

namespace Dal::Script {

    template <class T_> class SmoothingConstants_ {
        std::optional<std::pair<T_, T_>> values_;

    public:
        SmoothingConstants_() = default;
        // Copies do not borrow a recording owner's prefix nodes.
        SmoothingConstants_(const SmoothingConstants_&) {}
        SmoothingConstants_& operator=(const SmoothingConstants_& rhs) {
            if (this != &rhs)
                values_.reset();
            return *this;
        }
        SmoothingConstants_(SmoothingConstants_&&) = default;
        SmoothingConstants_& operator=(SmoothingConstants_&&) = default;

        void Initialize() { values_.emplace(0.0, 1.0); }
        [[nodiscard]] FORCE_INLINE T_ Zero() const { return values_ ? values_->first : T_(0.0); }
        [[nodiscard]] FORCE_INLINE T_ One() const { return values_ ? values_->second : T_(1.0); }
    };

    template <> class SmoothingConstants_<double> {
    public:
        void Initialize() {}
        [[nodiscard]] static FORCE_INLINE double Zero() { return 0.0; }
        [[nodiscard]] static FORCE_INLINE double One() { return 1.0; }
    };

    template <class T_> FORCE_INLINE T_ CSpr(const T_& x, double eps, const SmoothingConstants_<T_>& constants) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps)
            return constants.Zero();
        if (x > halfEps)
            return constants.One();
        return (x + halfEps) / eps;
    }

    template <class T_> FORCE_INLINE T_ CSpr(const T_& x, double lb, double rb, const SmoothingConstants_<T_>& constants) {
        if (x < lb)
            return constants.Zero();
        if (x > rb)
            return constants.One();
        return (x - lb) / (rb - lb);
    }

    template <class T_> FORCE_INLINE T_ BFly(const T_& x, double eps, const SmoothingConstants_<T_>& constants) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps || x > halfEps)
            return constants.Zero();
        return (halfEps - abs(x)) / halfEps;
    }

    template <class T_> FORCE_INLINE T_ BFly(const T_& x, double lb, double rb, const SmoothingConstants_<T_>& constants) {
        if (x < lb || x > rb)
            return constants.Zero();
        if (x < 0.0)
            return 1.0 - x / lb;
        return 1.0 - x / rb;
    }

    template <class T_> FORCE_INLINE T_ CSpr(const T_& x, double eps) { return CSpr(x, eps, SmoothingConstants_<T_>{}); }
    template <class T_> FORCE_INLINE T_ CSpr(const T_& x, double lb, double rb) { return CSpr(x, lb, rb, SmoothingConstants_<T_>{}); }
    template <class T_> FORCE_INLINE T_ BFly(const T_& x, double eps) { return BFly(x, eps, SmoothingConstants_<T_>{}); }
    template <class T_> FORCE_INLINE T_ BFly(const T_& x, double lb, double rb) { return BFly(x, lb, rb, SmoothingConstants_<T_>{}); }
} // namespace Dal::Script
