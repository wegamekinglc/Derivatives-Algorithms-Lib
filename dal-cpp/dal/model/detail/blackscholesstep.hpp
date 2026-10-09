//
// Created by Codex on 2026/10/9.
//

#pragma once

#include <cmath>

#include <dal/math/operators.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD::Detail {
    template <class T_> void ValidateBlackScholesParameters(const T_& spot, const T_& vol, const T_& rate, const T_& div) {
        REQUIRE(std::isfinite(Value(spot)) && Value(spot) > 0.0, "InvalidModelParameter: spot must be finite and positive");
        REQUIRE(std::isfinite(Value(vol)) && Value(vol) >= 0.0, "InvalidModelParameter: vol must be finite and nonnegative");
        REQUIRE(std::isfinite(Value(rate)), "InvalidModelParameter: rate must be finite");
        REQUIRE(std::isfinite(Value(div)), "InvalidModelParameter: div must be finite");
    }

    template <class T_> struct BlackScholesCoefficients_ {
        T_ std_;
        T_ drift_;
    };

    template <class T_> BlackScholesCoefficients_<T_> BlackScholesCoefficients(const T_& vol, const T_& mu, double dt) {
        BlackScholesCoefficients_<T_> result{vol * Dal::sqrt(dt), (mu - 0.5 * vol * vol) * dt};
        REQUIRE(std::isfinite(Value(result.std_)) && std::isfinite(Value(result.drift_)), "InvalidModelParameter: non-finite BS step");
        return result;
    }

    template <class T_> T_ BlackScholesNumeraire(const T_& rate, double time) {
        T_ result = Dal::exp(rate * time);
        REQUIRE(std::isfinite(Value(result)) && Value(result) > 0.0, "InvalidModelParameter: non-finite or zero BS numeraire");
        return result;
    }

    template <class T_> T_ BlackScholesDiscount(const T_& rate, double time, double maturity) {
        T_ result = Dal::exp(-rate * (maturity - time));
        REQUIRE(std::isfinite(Value(result)) && Value(result) > 0.0, "InvalidModelParameter: non-finite or zero BS discount factor");
        return result;
    }
} // namespace Dal::AAD::Detail
