//
// Created by wegam on 2025/7/6.
//

#pragma once

#include <dal/platform/host.hpp>
#include <dal/math/operators.hpp>

//  Smoothed condition kernels shared by the tree-walk FuzzyEvaluator_ and
//  the compiled fuzzy opcodes (FuzzyEqual/FuzzyComp and their discrete
//  variants): call-spread for inequalities, butterfly for equalities.

namespace Dal::Script {

    template <class T_>
    FORCE_INLINE T_ CSpr(const T_& x, double eps) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps)
            return T_(0.0);
        if (x > halfEps)
            return T_(1.0);
        return (x + halfEps) / eps;
    }

    template <class T_>
    FORCE_INLINE T_ CSpr(const T_& x, double lb, double rb) {
        if (x < lb)
            return T_(0.0);
        if (x > rb)
            return T_(1.0);
        return (x - lb) / (rb - lb);
    }

    template <class T_>
    FORCE_INLINE T_ BFly(const T_& x, double eps) {
        const double halfEps = 0.5 * eps;

        if (x < -halfEps || x > halfEps)
            return T_(0.0);
        return (halfEps - abs(x)) / halfEps;
    }

    template <class T_>
    FORCE_INLINE T_ BFly(const T_& x, double lb, double rb) {
        if (x < lb || x > rb)
            return T_(0.0);
        if (x < 0.0)
            return 1.0 - x / lb;
        return 1.0 - x / rb;
    }
} // namespace Dal::Script
