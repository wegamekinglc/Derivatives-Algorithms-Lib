#pragma once

#include <array>
#include <cmath>

#include <dal/math/aad/secondorder.hpp>

namespace Dal::SpotCallDetail {
    // Internal owning entry for an independently priced smooth European call.
    // Both legs use the scalar Spot formula. No active primitive is exposed.
    class FixedSpotCall_ {
        double strike_, time_, carry_, rootTime_, spotDiscount_, strikePv_;
        std::array<int, 2> axes_;

        template <class T_> T_ D1(const T_& spot, const T_& vol) const {
            using std::log;
            return (log(spot / strike_) + (carry_ + 0.5 * vol * vol) * time_) / (vol * rootTime_);
        }

        AAD::SecondOrderNumber_ Record(const Vector_<AAD::SecondOrderNumber_>& inputs) const;

    public:
        FixedSpotCall_(double strike, double time, double rate, double dividend, std::array<int, 2> axes = {0, 1});

        // The scalar formula is also the independent comparator and wide replay.
        template <class T_> T_ Scalar(const T_& spot, const T_& vol) const {
            using std::erfc;
            static constexpr double invRootTwo = 0.7071067811865475244;
            const T_ d1 = D1(spot, vol), d2 = d1 - vol * rootTime_;
            const auto cdf = [](const T_& d) -> T_ { return 0.5 * erfc(-invRootTwo * d); };
            return spot * spotDiscount_ * cdf(d1) - strikePv_ * cdf(d2);
        }

        [[nodiscard]] AAD::SecondOrderResult_
        Evaluate(const Vector_<>& point, const Matrix_<>& directions, const AAD::SecondOrderOptions_& options = {}) const;
    };
} // namespace Dal::SpotCallDetail
