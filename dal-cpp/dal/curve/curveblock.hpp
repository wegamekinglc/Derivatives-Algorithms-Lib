//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <map>

#include <dal/curve/calibration.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {

    class CurveBlock_ : public YieldCurve_ {
        const DiscountCurve_* dc_;
        std::map<CollateralType_, Handle_<DiscountCurve_>> discountCurves_;
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwardCurves_;
        DayBasis_ liborBasis_;

    public:
        explicit CurveBlock_(const DiscountCurve_& dc);
        CurveBlock_(const DiscountCurve_& dc, const DayBasis_& liborBasis);
        explicit CurveBlock_(const Handle_<DiscountCurve_>& dc, const DayBasis_& liborBasis = DayBasis::Act365F());
        CurveBlock_(const String_& name,
                    const String_& ccy,
                    const std::map<CollateralType_, Handle_<DiscountCurve_>>& discountCurves,
                    const std::map<PeriodLength_, Handle_<DiscountCurve_>>& forwardCurves = {},
                    const DayBasis_& liborBasis = DayBasis::Act365F());
        [[nodiscard]] bool HasDiscount(const CollateralType_& collateral) const override;
        [[nodiscard]] bool HasForward(const PeriodLength_& tenor) const override;
        [[nodiscard]] const DiscountCurve_& Discount(const CollateralType_& collateral) const override;
        [[nodiscard]] const DiscountCurve_& Forward(const PeriodLength_& tenor, const CollateralType_& collateral) const override;
        [[nodiscard]] double FwdLibor(const PeriodLength_& tenor, const Date_& fixingDate) const override;
        [[nodiscard]] const std::map<CollateralType_, Handle_<DiscountCurve_>>& DiscountCurves() const { return discountCurves_; }
        [[nodiscard]] const std::map<PeriodLength_, Handle_<DiscountCurve_>>& ForwardCurves() const { return forwardCurves_; }
        [[nodiscard]] const DayBasis_& LiborBasis() const { return liborBasis_; }
        void Write(Archive::Store_& dst) const override;
    };

    std::unique_ptr<DiscountCurve_> CalibrateYieldCurve(const Date_& today,
                                                        const String_& ccy,
                                                        const Vector_<Handle_<YCInstrument_>>& instruments,
                                                        const Vector_<Date_>& knotDates,
                                                        double smoothingWeight = 1.0,
                                                        double tolerance = 1.0e-8,
                                                        int maxEvaluations = 200,
                                                        int maxRestarts = 20,
                                                        Matrix_<>* effJacobianInverse = nullptr);

} // namespace Dal
