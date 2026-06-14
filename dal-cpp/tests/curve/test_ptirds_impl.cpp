//
// Scratch verification harness for the PTIRDS single-currency curve implementation.
// NOT the committed deliverable test (that is the dal-tester agent's job, task 4).
// This file drives Phases 1-5 of the implementation and prints the solved node DFs
// against the §2.5 acceptance table so the implementer can resolve S2 and S3
// empirically before handing off.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    RateLegConvention_ AnnualLeg() {
        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        // Calendar="all" means every day is a business day. DAL's Holidays::None() still
        // treats weekends as non-business, so use Unadjusted to avoid rolling IMM/stub dates.
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }

    RateIndexConvention_ AnnualIndex() {
        RateIndexConvention_ idx;
        idx.forecastTenor_ = PeriodLength_("12M");
        idx.dayBasis_ = DayBasis_("ACT_365F");
        idx.fixingLag_ = 0;
        idx.spotLag_ = 0;
        idx.fixingHolidays_ = Holidays::None();
        idx.accrualHolidays_ = Holidays::None();
        idx.businessDayConvention_ = BizDayConvention_("Unadjusted");
        idx.useProjectionCurve_ = false;
        return idx;
    }

    Vector_<Date_> PtirdsKnotDates() {
        return {
            Date_(2022, 1, 1),
            Date_(2022, 3, 15), Date_(2022, 6, 15), Date_(2022, 9, 21), Date_(2022, 12, 21),
            Date_(2023, 3, 15), Date_(2023, 6, 21), Date_(2023, 9, 20), Date_(2023, 12, 20),
            Date_(2024, 3, 15),
            Date_(2025, 1, 1), Date_(2027, 1, 1), Date_(2029, 1, 1), Date_(2032, 1, 1),
        };
    }

    Vector_<Handle_<YCInstrument_>> PtirdsInstruments(const Date_& today) {
        const auto fixedLeg = AnnualLeg();
        const auto floatIdx = AnnualIndex();
        const auto floatLeg = AnnualLeg();
        const auto mk = [&](const Date_& start, const Date_& end, double parPct) {
            return Handle_<YCInstrument_>(
                new Swap_(today, start, end, parPct / 100.0, fixedLeg, floatIdx, floatLeg));
        };
        Vector_<Handle_<YCInstrument_>> v;
        v.reserve(13);
        v.push_back(mk(Date_(2022, 1, 1), Date_(2022, 1, 3), 1.00));   // 1-business-day
        v.push_back(mk(Date_(2022, 3, 15), Date_(2022, 6, 15), 1.05));
        v.push_back(mk(Date_(2022, 6, 15), Date_(2022, 9, 21), 1.12));
        v.push_back(mk(Date_(2022, 9, 21), Date_(2022, 12, 21), 1.16));
        v.push_back(mk(Date_(2022, 12, 21), Date_(2023, 3, 15), 1.21));
        v.push_back(mk(Date_(2023, 3, 15), Date_(2023, 6, 21), 1.27));
        v.push_back(mk(Date_(2023, 6, 21), Date_(2023, 9, 20), 1.45));
        v.push_back(mk(Date_(2023, 9, 20), Date_(2023, 12, 20), 1.68));
        v.push_back(mk(Date_(2023, 12, 20), Date_(2024, 3, 15), 1.92));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.68));   // 3y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2027, 1, 1), 2.10));   // 5y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2029, 1, 1), 2.20));   // 7y
        v.push_back(mk(Date_(2022, 1, 1), Date_(2032, 1, 1), 2.07));   // 10y
        return v;
    }

    CurveCalibrationSpec_ BuildBaseSpec(const Date_& today, LogDfScheme_ scheme) {
        CurveCalibrationSpec_ spec;
        spec.today_ = today;
        spec.ccy_ = "USD";
        spec.curveName_ = "ptirds";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.fitTolerance_ = 1.0e-8;
        spec.smoothingWeight_ = 1.0;
        spec.knotDates_ = PtirdsKnotDates();
        spec.instruments_ = PtirdsInstruments(today);
        spec.logDfScheme_ = scheme;
        return spec;
    }

    void PrintNodeDFs(const char* label, const CurveCalibrationResult_& r) {
        const auto* c = dynamic_cast<const DiscountLogDF_*>(r.curve_.get());
        EXPECT_NE(c, nullptr) << "calibrated curve is not a DiscountLogDF_";
        if (!c) return;
        const auto dfs = c->NodeDF();
        const auto dates = c->NodeDates();
        const Vector_<> expected = {
            1.000000, 0.998002, 0.995368, 0.992383, 0.989522,
            0.986774, 0.983421, 0.979878, 0.975791, 0.971397,
            0.950979, 0.900384, 0.857395, 0.814369,
        };
        // B3 diagnostic: estimate cond(J) from effJacobianInverse_ (J^{-1} for the square case).
        // cond(J) ~ ||J^{-1}||_inf since ||J||_inf is O(1) on this well-scaled problem.
        double invInfNorm = 0.0;
        const auto& jinv = r.diagnostics_.effJacobianInverse_;
        for (int i = 0; i < static_cast<int>(jinv.Rows()); ++i) {
            double rowSum = 0.0;
            for (int j = 0; j < static_cast<int>(jinv.Cols()); ++j)
                rowSum += std::fabs(jinv(i, j));
            invInfNorm = std::max(invInfNorm, rowSum);
        }
        std::printf("\n  [%s] maxAbsResid=%.3e rms=%.3e usedApprox=%d ||J^-1||_inf=%.3e\n",
                    label,
                    r.diagnostics_.maxAbsResidual_,
                    r.diagnostics_.rmsResidual_,
                    r.diagnostics_.usedApproximateFit_ ? 1 : 0,
                    invInfNorm);
        double maxDiff = 0.0;
        std::printf("    %-12s %-12s %-12s %-12s\n", "node", "date", "solved_DF", "|diff|");
        for (int i = 0; i < static_cast<int>(dfs.size()); ++i) {
            const double diff = std::fabs(dfs[i] - expected[i]);
            maxDiff = std::max(maxDiff, diff);
            std::printf("    %-12d %-12s %-12.6f %-12.3e\n",
                        i, Date::ToString(dates[i]).c_str(), dfs[i], diff);
        }
        std::printf("    max|solved - expected(log-linear)| = %.3e\n", maxDiff);
    }
} // namespace

TEST(PtirdsImplTest, TestDiscountLogDFNodes) {
    const Vector_<Date_> dates = PtirdsKnotDates();
    const DayBasis_ basis("ACT_365F");
    const Date_& anchor = dates.front();
    // flat -2% continuous: logDF_i = -0.02 * yf(anchor, dates[i])
    Vector_<> logDF(dates.size());
    for (int i = 0; i < static_cast<int>(dates.size()); ++i)
        logDF[i] = -0.02 * basis(anchor, dates[i], nullptr);
    Handle_<Interp1_> interp(Dal::Interp::NewLinear("seed", [&] {
        Vector_<> yf(dates.size());
        for (int i = 0; i < static_cast<int>(dates.size()); ++i)
            yf[i] = basis(anchor, dates[i], nullptr);
        return yf;
    }(), logDF));
    std::unique_ptr<DiscountCurve_> curve(NewDiscountLogDF("t", "USD", dates, logDF, basis, interp));
    // anchor -> anchor DF must be 1
    ASSERT_NEAR((*curve)(anchor, anchor), 1.0, 1e-12);
    // anchor -> node DF must equal exp(logDF_i)
    for (int i = 0; i < static_cast<int>(dates.size()); ++i) {
        ASSERT_NEAR((*curve)(anchor, dates[i]), std::exp(logDF[i]), 1e-12) << "node " << i;
    }
}

TEST(PtirdsImplTest, TestCalibrateLogLinear) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_LINEAR);
    const auto r = CalibrateYieldCurve(spec);
    PrintNodeDFs("log_linear", r);
    EXPECT_LT(r.diagnostics_.maxAbsResidual_, 1.0e-7);
}

TEST(PtirdsImplTest, TestCalibrateLogCubicNatural) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const auto r = CalibrateYieldCurve(spec);
    PrintNodeDFs("log_cubic_natural", r);
    EXPECT_LT(r.diagnostics_.maxAbsResidual_, 1.0e-7);
}

TEST(PtirdsImplTest, TestCalibrateMixed) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::MIXED);
    const auto r = CalibrateYieldCurve(spec);
    PrintNodeDFs("mixed", r);
    EXPECT_LT(r.diagnostics_.maxAbsResidual_, 1.0e-7);
}
