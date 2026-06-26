//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <map>
#include <dal/curve/discount.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Tape {
        template <class T_>
        struct JointCurveBlock_ {
            std::map<CollateralType_, const DiscountCurve_<T_>*> discountCurves;
            std::map<PeriodLength_, const DiscountCurve_<T_>*> forwardCurves;

            const DiscountCurve_<T_>& Discount(const CollateralType_& collateral) const {
                const auto found = discountCurves.find(collateral);
                if (found != discountCurves.end())
                    return *found->second;
                const auto ois = discountCurves.find(CollateralType_(CollateralType_::Value_::OIS));
                REQUIRE(ois != discountCurves.end(), "JointCurveBlock_<T_>::Discount cannot route collateral without an OIS discount curve");
                return *ois->second;
            }

            const DiscountCurve_<T_>& Forward(const PeriodLength_& tenor, const CollateralType_& collateral) const {
                const auto found = forwardCurves.find(tenor);
                if (found != forwardCurves.end())
                    return *found->second;
                return Discount(collateral);
            }
        };
    } // namespace Tape
} // namespace Dal
