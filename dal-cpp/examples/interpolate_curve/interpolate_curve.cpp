//
// Created by dal-implementer on 2026/6/14.
//

#include <iomanip>
#include <iostream>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    // Calendar="all" in rateslib means every day is a business day. DAL's Holidays::None()
    // still treats weekends as non-business, so use Unadjusted to avoid rolling IMM/stub dates.
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

    // 14 knot dates from rateslib Table 6.2: 1 on the value date, 9 IMM/stub dates spanning
    // the first ~2.25y, and 4 annual rolls out to 10y.
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

    CurveCalibrationSpec_ BuildSpec(const Date_& today, LogDfScheme_ scheme) {
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

    const char* SchemeName(LogDfScheme_::Value_ s) {
        switch (s) {
            case LogDfScheme_::Value_::LOG_LINEAR: return "log-linear";
            case LogDfScheme_::Value_::LOG_CUBIC_NATURAL: return "log-cubic";
            case LogDfScheme_::Value_::MIXED: return "mixed";
            default: return "?";
        }
    }

    void PrintHeader(const char* title) {
        std::cout << "\n" << std::string(78, '=') << "\n  " << title << "\n"
                  << std::string(78, '=') << "\n";
    }

    void PrintDfTable(const Vector_<Date_>& dates,
                      const Vector_<>& dfLinear,
                      const Vector_<>& dfCubic,
                      const Vector_<>& dfMixed) {
        PrintHeader("Solved node discount factors (rateslib Table 6.2, 6 dp)");
        const Vector_<int> w = {13, 13, 13, 13};
        std::cout << std::left << std::setw(w[0]) << "Node date" << std::right
                  << std::setw(w[1]) << "log-linear" << std::setw(w[2]) << "log-cubic"
                  << std::setw(w[3]) << "mixed" << '\n';
        std::cout << std::string(52, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(dates.size()); ++i) {
            std::cout << std::left << std::setw(w[0]) << Date::ToString(dates[i]) << std::right
                      << std::setw(w[1]) << dfLinear[i] << std::setw(w[2]) << dfCubic[i]
                      << std::setw(w[3]) << dfMixed[i] << '\n';
        }
    }

    // One-year continuously-compounded forward rate at each node: f = -d(log DF)/dt.
    // operator()(from, to) returns the forward DF DF(to)/DF(from) (< 1 for positive rates),
    // so f = -log(DF(to)/DF(from)) / yf over an ACT/365F year, comparable across all schemes.
    double AnnualForward(const DiscountCurve_& curve, const Date_& from, const DayBasis_& basis) {
        const auto to = Date::AddMonths(from, 12);
        const double yf = basis(from, to, nullptr);
        const double fwdDf = curve(from, to); // forward DF = DF(to)/DF(from)
        return -std::log(fwdDf) / yf;
    }

    void PrintForwardTable(const Vector_<Date_>& dates,
                           const DiscountCurve_& linear,
                           const DiscountCurve_& cubic,
                           const DiscountCurve_& mixed,
                           const DayBasis_& basis) {
        PrintHeader("1y forward rates at node dates (%)");
        const Vector_<int> w = {13, 13, 13, 13};
        std::cout << std::left << std::setw(w[0]) << "Node date" << std::right
                  << std::setw(w[1]) << "log-linear" << std::setw(w[2]) << "log-cubic"
                  << std::setw(w[3]) << "mixed" << '\n';
        std::cout << std::string(52, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        // Skip the terminal node: the 1y-ahead date lies beyond the last knot, so the
        // forward there depends on each scheme's extrapolation rule and is not comparable.
        for (int i = 0; i + 1 < static_cast<int>(dates.size()); ++i) {
            const auto& d = dates[i];
            std::cout << std::left << std::setw(w[0]) << Date::ToString(d) << std::right
                      << std::setw(w[1]) << AnnualForward(linear, d, basis) * 100.0
                      << std::setw(w[2]) << AnnualForward(cubic, d, basis) * 100.0
                      << std::setw(w[3]) << AnnualForward(mixed, d, basis) * 100.0 << '\n';
        }
    }

    void PrintResiduals(const CurveCalibrationDiagnostics_& d, LogDfScheme_::Value_ scheme) {
        std::cout << std::left << std::setw(12) << SchemeName(scheme) << std::right
                  << "  maxAbsResidual = " << std::scientific << std::setprecision(3)
                  << d.maxAbsResidual_ << std::fixed << ",  rmsResidual = " << std::scientific
                  << std::setprecision(3) << d.rmsResidual_ << std::fixed
                  << (d.usedApproximateFit_ ? "  [approx fit]" : "  [exact solve]") << '\n';
    }
} // namespace

int main() {
    RegisterAll_::Init();

    const Date_ today(2022, 1, 1);
    const DayBasis_ basis("ACT_365F");

    std::cout << "interpolate_curve -- discount-curve interpolation (three LOG_DISCOUNT schemes)\n"
              << "Value date: " << Date::ToString(today) << "    14 nodes, 13 IRS/FRA instruments\n";

    // -- Calibrate each scheme --
    auto linSpec = BuildSpec(today, LogDfScheme_::Value_::LOG_LINEAR);
    auto cubSpec = BuildSpec(today, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    auto mixSpec = BuildSpec(today, LogDfScheme_::Value_::MIXED);
    const auto rLin = CalibrateYieldCurve(linSpec);
    const auto rCub = CalibrateYieldCurve(cubSpec);
    const auto rMix = CalibrateYieldCurve(mixSpec);

    // -- Read back node DFs --
    const auto* cLin = dynamic_cast<const DiscountLogDF_*>(rLin.curve_.get());
    const auto* cCub = dynamic_cast<const DiscountLogDF_*>(rCub.curve_.get());
    const auto* cMix = dynamic_cast<const DiscountLogDF_*>(rMix.curve_.get());
    REQUIRE(cLin && cCub && cMix, "calibrated curve is not a DiscountLogDF_");

    const auto dates = cLin->NodeDates();
    PrintDfTable(dates, cLin->NodeDF(), cCub->NodeDF(), cMix->NodeDF());

    // -- Forward-rate comparison --
    PrintForwardTable(dates, *cLin, *cCub, *cMix, basis);

    // -- Repricing residuals --
    PrintHeader("Repricing residuals across the three schemes");
    PrintResiduals(rLin.diagnostics_, LogDfScheme_::Value_::LOG_LINEAR);
    PrintResiduals(rCub.diagnostics_, LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
    PrintResiduals(rMix.diagnostics_, LogDfScheme_::Value_::MIXED);
    std::cout << '\n';

    return 0;
}
