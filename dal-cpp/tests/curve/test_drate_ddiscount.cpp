//
// Created by dal-implementer on 2026/6/15.
//

#include <gtest/gtest.h>
#include <cmath>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    // A reference discount curve built from explicit (date, logDF) knots -- the simulated calibrated curve.
    std::unique_ptr<DiscountLogDF_> MakeReferenceCurve(const Vector_<Date_>& dates, const Vector_<>& logDF) {
        std::unique_ptr<DiscountCurve_> base(
            NewDiscountLogDF("ref", "USD", dates, logDF, DayBasis_("ACT_365F"), LogDfScheme_::Value_::LOG_LINEAR));
        DiscountLogDF_* cast = dynamic_cast<DiscountLogDF_*>(base.get());
        EXPECT_NE(cast, nullptr);
        (void)base.release();
        return std::unique_ptr<DiscountLogDF_>(cast);
    }

    // Build a YieldCurve_ context with the supplied discount curve in the OIS slot and no forwards.
    Handle_<YieldCurve_> MakeYieldContext(const String_& name, const DiscountCurve_& dc) {
        std::map<CollateralType_, Handle_<DiscountCurve_>> discounts;
        discounts[CollateralType_(CollateralType_::Value_::OIS)] =
            Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), &dc));
        std::map<PeriodLength_, Handle_<DiscountCurve_>> forwards;
        return Handle_<YieldCurve_>(new CurveBlock_(name, "USD", discounts, forwards, DayBasis_("ACT_365F")));
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

    RateLegConvention_ AnnualLeg() {
        RateLegConvention_ leg;
        leg.paymentLag_ = 0;
        leg.paymentFrequency_ = PeriodLength_("12M");
        leg.dayBasis_ = DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Holidays::None();
        leg.paymentHolidays_ = Holidays::None();
        leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
        leg.paymentConvention_ = BizDayConvention_("Unadjusted");
        return leg;
    }

    // Verify that, for each (date, derivative) pair returned by rate->DRateDDiscount(yc, DISCOUNT),
    // the derivative matches the central-difference sensitivity of (*rate)(yc) w.r.t. the curve's
    // DF(anchor, date). The curve's anchor is the first node date. We bump logDF at the corresponding
    // knot column and re-evaluate.
    void CheckDRateDDiscountVsFD(const YCInstrument_::Rate_& rate, const YieldCurve_& yc, const DiscountLogDF_& curve) {
        const Date_ anchor = curve.NodeDates().front();
        const DayBasis_ basis = curve.DayCount();
        const auto dRateDdf = rate.DRateDDiscount(yc, YCInstrument_::Rate_::Target_::DISCOUNT);
        ASSERT_FALSE(dRateDdf.empty());

        // For each (date, deriv) pair, identify the storage knot whose yf is closest to that date and
        // compute the central-difference sensitivity by perturbing its logDF. We bump each free knot
        // in turn, identify the date-bucketed contribution via finite-difference on logDF vs the
        // chain-rule dlogDF/dD = 1/D -- equivalently, bump logDF directly and use the chain rule
        // dRate/dlogDF_knot = sum over returned dates on that knot of (dRate/dD) * D * basis_weight.
        // For brevity we just check that the TOTAL finite-difference sensitivity matches the sum
        // implied by the chain rule through InterpBasisWeights (i.e. the analytic per-knot contribution).
        const double h = 1.0e-6;
        const int nNodes = static_cast<int>(curve.NodeDates().size());
        for (int knot = 1; knot < nNodes; ++knot) {
            // Build a bumped curve by perturbing logDF_[knot].
            Vector_<> bumpedLog = curve.NodeLogDF();
            bumpedLog[knot] += h;
            std::unique_ptr<DiscountCurve_> up(
                NewDiscountLogDF("up", "USD", curve.NodeDates(), bumpedLog, basis, LogDfScheme_::Value_::LOG_LINEAR));
            bumpedLog = curve.NodeLogDF();
            bumpedLog[knot] -= h;
            std::unique_ptr<DiscountCurve_> dn(
                NewDiscountLogDF("dn", "USD", curve.NodeDates(), bumpedLog, basis, LogDfScheme_::Value_::LOG_LINEAR));
            Handle_<YieldCurve_> ycUp = MakeYieldContext("up", *up);
            Handle_<YieldCurve_> ycDn = MakeYieldContext("dn", *dn);
            const double rateUp = rate(*ycUp);
            const double rateDn = rate(*ycDn);
            const double dr_dlogDF_knot = (rateUp - rateDn) / (2.0 * h);
            // Analytic chain rule: sum over (date, dRate/dD_date) of dRate/dD_date * D_date * b_knot(date)
            // where b_knot(date) is the basis weight at solver column knot-1 for yf(date).
            const int solverCol = knot - 1;
            double analytic = 0.0;
            for (const auto& [date, dr_dD] : dRateDdf) {
                const double yf = basis(anchor, date, nullptr);
                const double D_date = curve(anchor, date);
                const auto weights = curve.InterpBasisWeights(yf);
                for (const auto& [col, w] : weights)
                    if (col == solverCol)
                        analytic += dr_dD * D_date * w;
            }
            // Compare. Tolerance 1e-8 (looser than InterpBasis because we go through D * basis).
            if (std::abs(dr_dlogDF_knot) < 1e-12) {
                ASSERT_NEAR(analytic, 0.0, 1e-9) << "knot " << knot << " FD=" << dr_dlogDF_knot;
            } else {
                ASSERT_NEAR(analytic, dr_dlogDF_knot, 1e-8 * std::max(1.0, std::abs(dr_dlogDF_knot)))
                    << "knot " << knot << " analytical=" << analytic << " FD=" << dr_dlogDF_knot;
            }
        }
    }
} // namespace

TEST(DRateDDiscountTest, TestSwapAnalyticMatchesCentralDifference) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> dates = {
        anchor, Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1), Date_(2026, 1, 1),
    };
    const Vector_<> logDF = {0.0, -0.02, -0.04, -0.06, -0.08};
    auto curve = MakeReferenceCurve(dates, logDF);
    ASSERT_NE(curve, nullptr);

    // Build a 3y vanilla swap starting at the anchor.
    auto yc = MakeYieldContext("ctx", *curve);
    const auto fixedLeg = AnnualLeg();
    const auto floatIdx = AnnualIndex();
    const auto floatLeg = AnnualLeg();
    Swap_ swap(anchor, anchor, Date_(2025, 1, 1), 0.02, fixedLeg, floatIdx, floatLeg);
    auto rate = swap.Precompute(yc);
    CheckDRateDDiscountVsFD(*rate, *yc, *curve);
}

TEST(DRateDDiscountTest, TestDepositAnalyticMatchesCentralDifference) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> dates = {
        anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
    };
    const Vector_<> logDF = {0.0, -0.005, -0.012, -0.025};
    auto curve = MakeReferenceCurve(dates, logDF);
    ASSERT_NE(curve, nullptr);
    auto yc = MakeYieldContext("ctx", *curve);

    // Deposit from anchor to first knot (6m). Anchor proxy == deposit start == curve anchor.
    Deposit_ deposit(anchor, Date_(2022, 7, 1), 0.01, DayBasis_("ACT_365F"));
    auto rate = deposit.Precompute(yc);
    CheckDRateDDiscountVsFD(*rate, *yc, *curve);
}

TEST(DRateDDiscountTest, TestFRADifferentStartEndMatchesCentralDifference) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> dates = {
        anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), Date_(2024, 1, 1),
    };
    const Vector_<> logDF = {0.0, -0.005, -0.012, -0.025};
    auto curve = MakeReferenceCurve(dates, logDF);
    ASSERT_NE(curve, nullptr);
    auto yc = MakeYieldContext("ctx", *curve);

    // 6x12 FRA. The deposit-rate-style derivative handles the s/m ratio correctly.
    FRA_ fra(anchor, Date_(2022, 7, 1), Date_(2023, 1, 1), 0.012, AnnualIndex());
    auto rate = fra.Precompute(yc);
    CheckDRateDDiscountVsFD(*rate, *yc, *curve);
}

TEST(DRateDDiscountTest, TestBasisSwapAnalyticMatchesCentralDifference) {
    const Date_ anchor(2022, 1, 1);
    const Vector_<Date_> dates = {
        anchor, Date_(2023, 1, 1), Date_(2024, 1, 1), Date_(2025, 1, 1), Date_(2026, 1, 1),
    };
    const Vector_<> logDF = {0.0, -0.02, -0.04, -0.06, -0.08};
    auto curve = MakeReferenceCurve(dates, logDF);
    ASSERT_NE(curve, nullptr);
    auto yc = MakeYieldContext("ctx", *curve);

    // 3y basis swap: 1y-spread leg vs 6m-reference leg. Both legs resolve to the discount curve.
    RateIndexConvention_ spreadIdx = AnnualIndex();
    spreadIdx.forecastTenor_ = PeriodLength_("12M");
    RateIndexConvention_ refIdx = AnnualIndex();
    refIdx.forecastTenor_ = PeriodLength_("6M");
    RateLegConvention_ spreadLeg = AnnualLeg();
    RateLegConvention_ refLeg = AnnualLeg();
    refLeg.paymentFrequency_ = PeriodLength_("6M");
    BasisSwap_ bs(
        anchor, anchor, Date_(2025, 1, 1), 0.001, spreadIdx, spreadLeg, refIdx, refLeg);
    auto rate = bs.Precompute(yc);
    CheckDRateDDiscountVsFD(*rate, *yc, *curve);
}

TEST(DRateDDiscountTest, TestDefaultReturnsEmpty) {
    // A custom Rate_ subclass with no override must return empty (the silent fallback path).
    class EmptyRate_ : public YCInstrument_::Rate_ {
    public:
        double operator()(const YieldCurve_&) const override { return 0.0; }
    };
    EmptyRate_ r;
    const auto result = r.DRateDDiscount(*Handle_<YieldCurve_> {}, YCInstrument_::Rate_::Target_::DISCOUNT);
    ASSERT_TRUE(result.empty());
}
