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
        // Phase B templated joint yield-curve context. The multi-curve analogue of YCCtx_<T_>:
        // one Number_-typed discount curve per collateral, one per forward tenor. Routes
        // Discount(collateral) and Forward(tenor, collateral) reads in the T_ domain, mirroring
        // CurveBlock_::Discount/Forward's routing (curveblock.cpp:65-83) including the OIS fallback
        // and the forward->discount fallback. NOT a YieldCurve_ subclass; exists only for the joint
        // AAD-tape residual evaluation in jointcalibration.cpp. Phase A's YCCtx_<T_> is untouched
        // (NG2) -- this is a sibling.
        //
        // The pointer maps are non-owning references to curves built in the SAME Gradient call.
        // Pointers (not Handle_<>) because the curves live on the shared_ptr storage of the
        // AnalyticJacobian frame for the duration of the sweep.
        template <class T_>
        struct JointCurveBlock_ {
            std::map<CollateralType_, const DiscountCurve_<T_>*> discountCurves;
            std::map<PeriodLength_, const DiscountCurve_<T_>*> forwardCurves;

            // Mirror of CurveBlock_::Discount (curveblock.cpp:65-74): exact-collateral match, then
            // OIS fallback. The OIS fallback is the joint post-2008 routing convention and must be
            // preserved on the tape so an OIS knot perturbation flows into an IBOR leg's discounting
            // when collateral != OIS.
            const DiscountCurve_<T_>& Discount(const CollateralType_& collateral) const {
                const auto found = discountCurves.find(collateral);
                if (found != discountCurves.end())
                    return *found->second;
                const auto ois = discountCurves.find(CollateralType_(CollateralType_::Value_::OIS));
                REQUIRE(ois != discountCurves.end(), "JointCurveBlock_<T_>::Discount cannot route collateral without an OIS discount curve");
                return *ois->second;
            }

            // Mirror of CurveBlock_::Forward (curveblock.cpp:76-83): exact-tenor match, else
            // Discount(collateral). The fallback matters: an instrument whose forecast tenor has no
            // registered forward curve routes to the discount curve, and the joint eligibility
            // predicate admits this case (a discount/baseless-forward declaration instrument with
            // useProjectionCurve_ == false, or an OIS-discount slice where forecast == discount).
            const DiscountCurve_<T_>& Forward(const PeriodLength_& tenor, const CollateralType_& collateral) const {
                const auto found = forwardCurves.find(tenor);
                if (found != forwardCurves.end())
                    return *found->second;
                return Discount(collateral);
            }
        };
    } // namespace Tape
} // namespace Dal
