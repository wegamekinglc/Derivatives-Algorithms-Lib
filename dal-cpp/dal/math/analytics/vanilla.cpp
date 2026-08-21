//
// Created by wegam on 2022/9/11.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/analytics/vanilla.hpp>

namespace Dal::AAD {

    double BlackScholesIVol(double spot, double strike, double prem, double mat) {
        REQUIRE(mat > 0.0, "BlackScholesIVol: maturity must be positive");
        static const OptionType_ type("CALL");
        const auto std_dev = Dal::Distribution::BlackIV(spot, strike, type, prem);
        return std_dev / Dal::sqrt(mat);
    }

    double BachelierIVol(double spot, double strike, double prem, double mat) {
        REQUIRE(mat > 0.0, "BachelierIVol: maturity must be positive");
        static const OptionType_ type("CALL");
        const auto std_dev = Dal::Distribution::BachelierIV(spot, strike, type, prem);
        return std_dev / Dal::sqrt(mat);
    }
} // namespace Dal::AAD
