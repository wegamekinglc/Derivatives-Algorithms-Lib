//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/curve/yc.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {

    class CalibratedYieldCurve_ : public YieldCurve_ {
        const DiscountCurve_& dc_;
    public:
        explicit CalibratedYieldCurve_(const DiscountCurve_& dc);
        [[nodiscard]] const DiscountCurve_& Discount(const CollateralType_& collateral) const override;
        [[nodiscard]] double FwdLibor(const PeriodLength_& tenor, const Date_& fixing_date) const override;
        void Write(Archive::Store_& dst) const override;
    };

    double DepositRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, const DayBasis_& basis);
    double SwapRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, int freqMonths, const DayBasis_& basis);

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                        const String_& ccy,
                                         const Vector_<Handle_<YCInstrument_>>& instruments,
                                         const Vector_<Date_>& knotDates,
                                         double smoothingWeight = 1.0,
                                         double tolerance = 1.0e-8,
                                         int maxEvaluations = 200,
                                         int maxRestarts = 20,
                                         Matrix_<>* effJacobianInverse = nullptr);

} // namespace Dal
