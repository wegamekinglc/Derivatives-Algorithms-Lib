//
// Created by wegam on 2026/4/19.
//

#pragma once

#include <dal/curve/discount.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>

namespace Dal {

    struct DepositInstrument_ {
        Date_ maturity_;
        double marketRate_;
    };

    struct SwapInstrument_ {
        Date_ maturity_;
        double marketRate_;
        int freqMonths_;
    };

    double DepositRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, const DayBasis_& basis);
    double SwapRate(const DiscountCurve_& dc, const Date_& today, const Date_& maturity, int freqMonths, const DayBasis_& basis);

    DiscountCurve_* CalibrateYieldCurve(const Date_& today,
                                         const Vector_<DepositInstrument_>& deposits,
                                         const Vector_<SwapInstrument_>& swaps,
                                         const Vector_<Date_>& knotDates,
                                         const DayBasis_& basis,
                                         double smoothingWeight = 1.0,
                                         double tolerance = 1.0e-8,
                                         int maxEvaluations = 200,
                                         int maxRestarts = 20,
                                         Matrix_<>* effJacobianInverse = nullptr);

} // namespace Dal
