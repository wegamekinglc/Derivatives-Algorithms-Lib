//
// Created by dal-implementer on 2026/6/20.
//

#pragma once

#include <cmath>
#include <dal/curve/jointycctx.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/time/datetime.hpp>

namespace Dal {
    namespace Tape {
        template <class T_>
        struct JointRate_ {
            virtual ~JointRate_() = default;
            virtual T_ operator()(const JointCurveBlock_<T_>& block) const = 0;
        };

        template <class T_>
        Handle_<JointRate_<T_>> ProjectionRateAt(const YCInstrument_& inst);

        // Simple forward rate from a forecast discount factor and accrual fraction.
        template <class T_>
        T_ ForwardRateFromDf(const T_& df, double accrual) {
            return (1.0 / df - 1.0) / accrual;
        }

        template <class T_>
        T_ ForwardRate(const DiscountCurve_<T_>& forecast,
                       const Date_& start,
                       const Date_& maturity,
                       const DayBasis_& basis,
                       const Handle_<DayBasis::Context_>& context) {
            const T_ fwdDf = forecast(start, maturity);
            return ForwardRateFromDf(fwdDf, basis(start, maturity, context.get()));
        }

        // 0 before valuation, 1 at valuation, curve otherwise; the validation message stays caller-specific.
        template <class T_>
        T_ DiscountFromValuation(const DiscountCurve_<T_>& curve,
                                 const DateTime_& valuationTime,
                                 const Date_& date,
                                 const char* errorMessage) {
            if (date < valuationTime.Date())
                return T_(0.0);
            const T_ result = date == valuationTime.Date() ? T_(1.0) : curve(valuationTime.Date(), date);
            const double value = Dal::AAD::Value(result);
            REQUIRE(std::isfinite(value) && value > 0.0, errorMessage);
            return result;
        }

        // Resolve forecast curve in joint block: forward if useProjectionCurve_, else discount.
        template <class T_>
        const DiscountCurve_<T_>& ForecastCurve(const JointCurveBlock_<T_>& block, const RateIndexConvention_& convention) {
            return convention.useProjectionCurve_ ? block.Forward(convention.forecastTenor_, convention.collateral_)
                                                  : block.Discount(convention.collateral_);
        }

        template <class T_>
        T_ DepositRateFromCurves(const DiscountCurve_<T_>& forecast, const SchedulePeriod_& period, const DayBasis_& basis) {
            return ForwardRate(forecast, period.accrualStart_, period.accrualEnd_, basis, period.dayCountContext_);
        }

        template <class T_>
        T_ ForwardRateFromCurves(const DiscountCurve_<T_>& forecast,
                                 const SchedulePeriod_& period,
                                 const DayBasis_& basis,
                                 double convexityAdjustment) {
            return DepositRateFromCurves(forecast, period, basis) - convexityAdjustment;
        }

        template <class T_, class Period_>
        T_ SwapRateFromCurves(const DiscountCurve_<T_>& discount,
                              const DiscountCurve_<T_>& forecast,
                              const Date_& tradeDate,
                              const Vector_<Period_>& fixedPeriods,
                              const Vector_<Period_>& floatPeriods,
                              const DayBasis_& floatBasis) {
            T_ annuity(static_cast<double>(0.0));
            for (const auto& period : fixedPeriods)
                annuity += static_cast<double>(period.accrual_.dcf_) * discount(tradeDate, period.schedule_.paymentDate_);
            // Dal::AAD::Value extracts the primal on every backend; static_cast<double> would
            // only work on native and CoDiPack (XAD/Adept have no conversion operator).
            REQUIRE(Dal::AAD::Value(annuity) > 0.0, "Swap pricing requires positive fixed-leg annuity");
            T_ floatPv(static_cast<double>(0.0));
            for (const auto& period : floatPeriods) {
                const T_ fixing = ForwardRate(forecast,
                                              period.schedule_.accrualStart_,
                                              period.schedule_.accrualEnd_,
                                              floatBasis,
                                              period.schedule_.dayCountContext_);
                floatPv += fixing * static_cast<double>(period.accrual_.dcf_) * discount(tradeDate, period.schedule_.paymentDate_);
            }
            return floatPv / annuity;
        }
    } // namespace Tape
} // namespace Dal
