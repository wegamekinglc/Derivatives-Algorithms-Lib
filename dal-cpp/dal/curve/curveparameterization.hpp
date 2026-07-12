//
// Created by dal-implementer on 2026/7/12.
//

#pragma once

#include <memory>

#include <dal/curve/calibration.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {
    struct CurveDefinition_ {
        String_ name_;
        String_ ccy_;
        CurveParameterization_ parameterization_;
        LogDfScheme_ logDfScheme_;
        Date_ anchorDate_;
        Vector_<Date_> nodeDates_;
        DayBasis_ dayCount_;
    };

    struct CurveParameterLayout_ {
        int storageNodeCount_ = 0;
        int parameterCount_ = 0;
        int paramsPerDeclaredKnot_ = 0;
        bool pinnedAnchor_ = false;
    };

    CurveDefinition_ MakeCurveDefinition(const String_& name,
                                         const String_& ccy,
                                         CurveParameterization_ parameterization,
                                         LogDfScheme_ logDfScheme,
                                         const Vector_<Date_>& declaredKnots,
                                         const Date_& anchor,
                                         const DayBasis_& dayCount);

    CurveParameterLayout_ BuildCurveParameterLayout(const CurveDefinition_& definition);

    Vector_<AAD::Number_> RegisterCurveParameters(const Vector_<>& parameters);

    template <class T_, class B_ = Tape::DiscountCurve_<double>>
    std::unique_ptr<Tape::DiscountCurve_<T_>>
    BuildDiscountCurveUniqueT(const CurveDefinition_& definition, const Vector_<T_>& parameters, const Handle_<B_>& base = Handle_<B_>());

    template <class T_, class B_ = Tape::DiscountCurve_<double>>
    std::shared_ptr<Tape::DiscountCurve_<T_>>
    BuildDiscountCurveT(const CurveDefinition_& definition, const Vector_<T_>& parameters, const Handle_<B_>& base = Handle_<B_>());
} // namespace Dal
