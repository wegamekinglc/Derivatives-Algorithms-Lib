//
// Created by dal-tester on 2026/6/14.
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

    // 13 instruments from rateslib Table 6.2: 9 single-period FRAs (the short end) plus
    // 4 annual-roll vanilla swaps (3y, 5y, 7y, 10y).
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

    // Authoritative reference: rateslib v2.7.x docs, Table 6.2 at
    // https://rateslib.com/py/en/2.7.x/z_ptirds_curve.html -- solved node discount factors
    // for the three interpolation schemes. The three columns are distinct by design; each
    // scheme must be checked against its OWN column. The mixed column reproduces log-linear
    // exactly for nodes 0..10 (through 2025-01-01) and diverges only at 2027/2029/2032.
    const Vector_<> EXPECTED_LOG_LINEAR = {
        1.000000, 0.998002, 0.995368, 0.992383, 0.989522,
        0.986774, 0.983421, 0.979878, 0.975791, 0.971397,
        0.950979, 0.900384, 0.857395, 0.814369,
    };
    const Vector_<> EXPECTED_LOG_CUBIC = {
        1.000000, 0.997990, 0.995355, 0.992371, 0.989509,
        0.986762, 0.983408, 0.979866, 0.975779, 0.971385,
        0.950979, 0.900395, 0.857430, 0.814470,
    };
    const Vector_<> EXPECTED_MIXED = {
        1.000000, 0.998002, 0.995368, 0.992383, 0.989522,
        0.986774, 0.983421, 0.979878, 0.975791, 0.971397,
        0.950979, 0.900384, 0.857422, 0.814460,
    };

    // Cross-check solved node DFs against the rateslib reference column, then verify
    // repricing residuals are below the fit-tolerance ceiling.
    void AssertNodesAndResiduals(const CurveCalibrationResult_& r, const Vector_<>& expectedColumn) {
        ASSERT_LT(r.diagnostics_.maxAbsResidual_, 1.0e-8)
            << "repricing residual exceeded 1e-8; max=" << r.diagnostics_.maxAbsResidual_;
        const auto* c = dynamic_cast<const DiscountLogDF_*>(r.curve_.get());
        ASSERT_NE(c, nullptr) << "calibrated curve is not a DiscountLogDF_";
        const auto dfs = c->NodeDF();
        const auto dates = c->NodeDates();
        ASSERT_EQ(static_cast<int>(dfs.size()), 14);
        ASSERT_EQ(static_cast<int>(expectedColumn.size()), 14);
        for (int i = 0; i < 14; ++i) {
            ASSERT_NEAR(dfs[i], expectedColumn[i], 1.0e-6)
                << "node " << i << " (" << Date::ToString(dates[i]) << ")";
        }
    }
} // namespace

TEST(PTIRDSCurveTest, TestLogLinearMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_LINEAR);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_LOG_LINEAR);
}

TEST(PTIRDSCurveTest, TestLogCubicMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_LOG_CUBIC);
}

TEST(PTIRDSCurveTest, TestMixedMatchesRateslib) {
    const Date_ today(2022, 1, 1);
    auto spec = BuildBaseSpec(today, LogDfScheme_::Value_::MIXED);
    const auto r = CalibrateYieldCurve(spec);
    AssertNodesAndResiduals(r, EXPECTED_MIXED);
}
