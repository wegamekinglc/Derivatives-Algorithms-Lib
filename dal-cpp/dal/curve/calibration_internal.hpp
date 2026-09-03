//
// Created by dal-implementer on 2026/6/27.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <type_traits>
#include <utility>

#include <dal/curve/ycinstrument.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/schedules.hpp>

namespace Dal {

    // Internal observation seam shared across the DAL library boundary. Test/benchmark support
    // only: the counter is process-global, so observers must run in a quiescent process.
    BASE_EXPORT void RecordCurveCalibrationInvocation();
    BASE_EXPORT void ResetCurveCalibrationInvocationCount();
    BASE_EXPORT int CurveCalibrationInvocationCount();

    // Shared internal helpers for single- and joint-curve calibration and leg-period construction.
    // inline so the header-only definitions do not violate the ODR across translation units.

    // Stable ordering of curve-calibration instruments: by maturity, then by start, then by name.
    inline Vector_<Handle_<YCInstrument_>> OrderInstruments(const Vector_<Handle_<YCInstrument_>>& instruments) {
        auto ordered = instruments;
        std::sort(ordered.begin(), ordered.end(), [](const Handle_<YCInstrument_>& lhs, const Handle_<YCInstrument_>& rhs) {
            const auto lhsSpan = lhs->TimeSpan();
            const auto rhsSpan = rhs->TimeSpan();
            if (lhsSpan.second != rhsSpan.second)
                return lhsSpan.second < rhsSpan.second;
            if (lhsSpan.first != rhsSpan.first)
                return lhsSpan.first < rhsSpan.first;
            return lhs->Name() < rhs->Name();
        });
        return ordered;
    }

    // 4-way dispatch over {Deposit_, FRA_, Future_, Swap_}. OISSwap_ reaches the Swap_ arm via
    // inheritance. Each caller supplies its per-type lambda; on no match the common return type is
    // default-constructed (nullptr for pointers, empty Handle_<>, false for bool).
    template <class D_, class F_, class Fu_, class S_>
    auto VisitRate(const YCInstrument_& inst, D_&& depositFn, F_&& fraFn, Fu_&& futureFn, S_&& swapFn)
        -> std::common_type_t<std::invoke_result_t<D_, const Deposit_&>,
                              std::invoke_result_t<F_, const FRA_&>,
                              std::invoke_result_t<Fu_, const Future_&>,
                              std::invoke_result_t<S_, const Swap_&>> {
        using R_ = std::common_type_t<std::invoke_result_t<D_, const Deposit_&>, std::invoke_result_t<F_, const FRA_&>,
                                      std::invoke_result_t<Fu_, const Future_&>, std::invoke_result_t<S_, const Swap_&>>;
        if (const auto* deposit = dynamic_cast<const Deposit_*>(&inst))
            return static_cast<R_>(std::forward<D_>(depositFn)(*deposit));
        if (const auto* fra = dynamic_cast<const FRA_*>(&inst))
            return static_cast<R_>(std::forward<F_>(fraFn)(*fra));
        if (const auto* future = dynamic_cast<const Future_*>(&inst))
            return static_cast<R_>(std::forward<Fu_>(futureFn)(*future));
        if (const auto* swap = dynamic_cast<const Swap_*>(&inst))
            return static_cast<R_>(std::forward<S_>(swapFn)(*swap));
        return R_{};
    }

    // Resolve the float index convention of a curve instrument, or nullptr if it has none.
    // Used by analytic-Jacobian eligibility checks (single-curve) and projection-curve routing (joint).
    // Returns a borrowed pointer; the instrument outlives the call.
    inline const RateIndexConvention_* FloatConventionOf(const YCInstrument_& inst) {
        return VisitRate(
            inst, [](const Deposit_& d) -> const RateIndexConvention_* { return &d.FloatConvention(); },
            [](const FRA_& f) -> const RateIndexConvention_* { return &f.FloatConvention(); },
            [](const Future_& fu) -> const RateIndexConvention_* { return &fu.FloatConvention(); },
            [](const Swap_& s) -> const RateIndexConvention_* { return &s.FloatConvention(); });
    }

    // Wrap a schedule period as an accrual period with unit notional under the given day basis.
    inline AccrualPeriod_ MakeAccrualPeriod(const SchedulePeriod_& period, const DayBasis_& basis) {
        return AccrualPeriod_(period.accrualStart_, period.accrualEnd_, 1.0, basis, period.dayCountContext_, period.isStub_);
    }

    // Shared analytic-Jacobian eligibility bar: the libor basis must be the canonical ACT/365F.
    [[nodiscard]] inline bool HasAct365FLiborBasis(const DayBasis_& basis) { return basis == DayBasis::Act365F(); }

    // Build a leg's coupon periods. PeriodT must be an aggregate initializable as
    // {SchedulePeriod_, AccrualPeriod_} (e.g. CouponPeriod_, XccyCouponPeriod_).
    template <class PeriodT_>
    Vector_<PeriodT_> BuildLegPeriods(
        const Date_& start, const Date_& maturity, const RateLegConvention_& legConvention, int fixingLag, const Holidays_& fixingHolidays) {
        Vector_<PeriodT_> retval;
        for (const auto& period :
             MakeSchedulePeriods(start, maturity, legConvention.paymentFrequency_, legConvention.accrualHolidays_, fixingLag, fixingHolidays,
                                 legConvention.paymentLag_, legConvention.paymentHolidays_, DateGeneration_("Forward"),
                                 legConvention.businessDayConvention_, legConvention.paymentConvention_, legConvention.endOfMonth_)) {
            retval.push_back({period, MakeAccrualPeriod(period, legConvention.dayBasis_)});
        }
        return retval;
    }

    // maxAbs and RMS of a residual sequence, single-pass. Scaled accumulation avoids overflow
    // when finite residuals are too large to square directly.
    struct ResidualStats_ {
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
    };

    template <class E_> ResidualStats_ ResidualStats(const Vector_<E_>& residuals) {
        double maxAbs = 0.0;
        double scale = 0.0;
        double scaledSquares = 0.0;
        for (const E_ r : residuals) {
            const double absResidual = std::fabs(r);
            if (!std::isfinite(absResidual))
                return ResidualStats_{absResidual, absResidual};
            maxAbs = std::max(maxAbs, absResidual);
            if (absResidual == 0.0)
                continue;
            if (scale < absResidual) {
                const double ratio = scale / absResidual;
                scaledSquares = 1.0 + scaledSquares * ratio * ratio;
                scale = absResidual;
            } else {
                const double ratio = absResidual / scale;
                scaledSquares += ratio * ratio;
            }
        }
        const double meanScaledSquares = residuals.empty() ? 0.0 : std::min(1.0, scaledSquares / static_cast<double>(residuals.size()));
        return ResidualStats_{maxAbs, scale * std::sqrt(meanScaledSquares)};
    }

    inline void RequireFiniteResidualStats(const ResidualStats_& stats, const String_& context) {
        REQUIRE(std::isfinite(stats.maxAbsResidual_), context + " maximum absolute residual must be finite");
        REQUIRE(std::isfinite(stats.rmsResidual_), context + " RMS residual must be finite");
    }

    // Shared convergence bar for the joint calibrations: every residual within 10x the caller's tolerance.
    [[nodiscard]] inline bool ResidualsWithinBar(const Vector_<>& residuals, double bar) {
        for (const double r : residuals) {
            if (std::fabs(r) > bar)
                return false;
        }
        return true;
    }

    // Shared non-convergence message tail: residual stats plus the solver evaluation count. Each call
    // site keeps its own prefix and exception type.
    [[nodiscard]] inline String_ NonConvergenceStats(const Vector_<>& residuals, int evaluationCount, bool includeRms = true) {
        const ResidualStats_ stats = ResidualStats(residuals);
        String_ retval = "maxAbsResidual = " + String::FromDouble(stats.maxAbsResidual_);
        if (includeRms)
            retval += ", rmsResidual = " + String::FromDouble(stats.rmsResidual_);
        return retval + " after " + String::FromInt(evaluationCount) + " evaluations";
    }

    // Solver-control dictionary keys shared by every curve-calibration driver.
    constexpr const char* KEY_MAX_EVALUATIONS = "MAXEVALUATIONS";
    constexpr const char* KEY_MAX_RESTARTS = "MAXRESTARTS";

    // Shared underdetermined-solver driver: exact solves decompose the smoothing weights and call
    // Find; everything else uses the approximate fit. The exact flag (not the solve mode) is passed
    // so each call site keeps its own EXACT-vs-APPROXIMATE branching semantics.
    inline Vector_<> RunCurveSolver(const Underdetermined::Function_& func,
                                    const Vector_<>& guess,
                                    const Vector_<>& tol,
                                    bool exact,
                                    double fitTolerance,
                                    const Sparse::TriDiagonal_& weights,
                                    int maxEvaluations,
                                    int maxRestarts,
                                    Matrix_<>* effJacobianInverse = nullptr,
                                    Matrix_<>* fwdJacobian = nullptr) {
        Dictionary_ ctrlDict;
        ctrlDict.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(maxEvaluations)));
        ctrlDict.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(maxRestarts)));
        const UnderdeterminedControls_ controls(ctrlDict);
        if (exact) {
            std::unique_ptr<Sparse::SymmetricDecomposition_> wDecomp(weights.DecomposeSymmetric());
            return Underdetermined::Find(func, guess, tol, *wDecomp, controls, effJacobianInverse, fwdJacobian);
        }
        return Underdetermined::Approximate(func, guess, tol, fitTolerance, weights, controls);
    }

    template <class ResidualFunction_>
    void CentralDifferenceJacobian(
        const Vector_<>& parameters, int residualCount, double bump, const ResidualFunction_& residualFunction, Matrix_<>* jacobian) {
        REQUIRE(residualCount >= 0 && std::isfinite(bump) && bump > 0.0, "Central-difference Jacobian inputs are invalid");
        jacobian->Resize(residualCount, static_cast<int>(parameters.size()));
        Vector_<> up = parameters;
        Vector_<> down = parameters;
        for (int column = 0; column < static_cast<int>(parameters.size()); ++column) {
            up[column] += bump;
            down[column] -= bump;
            const Vector_<> upResiduals = residualFunction(up);
            const Vector_<> downResiduals = residualFunction(down);
            REQUIRE(static_cast<int>(upResiduals.size()) == residualCount && static_cast<int>(downResiduals.size()) == residualCount,
                    "Central-difference Jacobian residual width changed");
            for (int row = 0; row < residualCount; ++row)
                (*jacobian)(row, column) = (upResiduals[row] - downResiduals[row]) / (2.0 * bump);
            up[column] = parameters[column];
            down[column] = parameters[column];
        }
    }

    // Resolve the coupon-months count for a single-period instrument's day-count context.
    // Forecasts off a projection curve use the curve's tenor; everything else uses CouponMonths.
    inline int SinglePeriodCouponMonths(const RateIndexConvention_& convention, const Date_& start, const Date_& maturity) {
        return convention.useProjectionCurve_ ? convention.forecastTenor_.Months() : CouponMonths(start, maturity);
    }

    // Build the SchedulePeriod_ shared by every single-period rate (Deposit, FRA, Future, and their
    // projection variants). The caller supplies the resolved couponMonths so both the
    // CouponMonths(...) and useProjectionCurve_ ternary call-sites collapse to one schedule build.
    inline SchedulePeriod_
    BuildSinglePeriodSchedule(const Date_& start, const Date_& maturity, const RateIndexConvention_& convention, int couponMonths) {
        SchedulePeriod_ period;
        period.unadjustedStart_ = start;
        period.unadjustedEnd_ = maturity;
        period.accrualStart_ = Holidays::Adjust(convention.accrualHolidays_, start, convention.businessDayConvention_);
        period.accrualEnd_ = Holidays::Adjust(convention.accrualHolidays_, maturity, convention.businessDayConvention_);
        period.dayCountContext_ = Handle_<DayBasis::Context_>(new DayBasis::Context_(true, start, maturity, couponMonths));
        return period;
    }

} // namespace Dal
