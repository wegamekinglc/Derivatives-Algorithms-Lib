#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/script/instrument.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/time/schedules.hpp>

#include "../../../dal-cpp/examples/floatformat.hpp"

using namespace Dal;

namespace {
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

    ScriptInstrumentDefinitionSpec_ AnnualSwapDefinition(const Date_& start, const Date_& end, double coupon) {
        const auto leg = AnnualLeg();
        const auto periods =
            MakeSchedulePeriods(start, end, leg.paymentFrequency_, leg.accrualHolidays_, 0, Holidays::None(), leg.paymentLag_, leg.paymentHolidays_,
                                DateGeneration_("Forward"), leg.businessDayConvention_, leg.paymentConvention_, leg.endOfMonth_);
        const auto price = CalibrationValueUnit_::Parse("Price:USD");
        const auto rate = CalibrationValueUnit_::Parse("Rate");
        ScriptInstrumentDefinitionSpec_ spec;
        spec.header_.payCurrency_ = spec.header_.resultCurrency_ = spec.header_.collateralCurrency_ = Ccy_("USD");
        spec.header_.discountRoute_ = "USD.OIS";
        spec.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", "100"},
                              {ScriptSourceRowKind_::Value_::Definition, "K", String::FromDouble(coupon)}};
        for (const auto& period : periods) {
            const auto alpha = String::FromDouble(leg.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get()));
            spec.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, Date::ToString(period.paymentDate_), "PAY PAYS N*(" + alpha + "*K)"});
        }
        for (const auto& period : periods) {
            const auto alpha = String::FromDouble(leg.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get()));
            const String_ forward = "((1/FIX(IR[USD,DF," + Date::ToString(period.accrualEnd_) + "," + Date::ToString(period.accrualStart_) + "]," +
                                    Date::ToString(period.fixingDate_) + ")-1)/" + alpha + ")";
            spec.source_.rows_.push_back(
                {ScriptSourceRowKind_::Value_::Event, Date::ToString(period.paymentDate_), "PAY PAYS N*(-" + alpha + "*" + forward + ")"});
        }
        spec.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                            {"K", ScriptParameterType_::Value_::Real, rate, ScriptParameterRole_::Value_::Coupon, true}};
        spec.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                          {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), rate, 1, 1e-12}};
        return spec;
    }

    Handle_<CalibrationInstrument_> AnnualSwapInstrument(const String_& id, const Date_& start, const Date_& end, double coupon) {
        return NewScriptCalibrationInstrument({id, NewScriptInstrumentDefinition(AnnualSwapDefinition(start, end, coupon)), {}, end});
    }

    Vector_<Date_> PtirdsKnotDates() {
        return {
            Date_(2022, 1, 1),  Date_(2022, 3, 15), Date_(2022, 6, 15), Date_(2022, 9, 21),  Date_(2022, 12, 21),
            Date_(2023, 3, 15), Date_(2023, 6, 21), Date_(2023, 9, 20), Date_(2023, 12, 20), Date_(2024, 3, 15),
            Date_(2025, 1, 1),  Date_(2027, 1, 1),  Date_(2029, 1, 1),  Date_(2032, 1, 1),
        };
    }

    Vector_<Handle_<CalibrationObservation_>> PtirdsObservations() {
        const auto rate = CalibrationValueUnit_::Parse("Rate");
        int index = 0;
        const auto mk = [&](const Date_& start, const Date_& end, double parPct) {
            const String_ id = "ptirds-" + String::FromInt(++index);
            const auto instrument = AnnualSwapInstrument(id, start, end, parPct / 100.0);
            const auto quote = NewCalibrationMarketQuote(id + "-quote", parPct / 100.0, rate);
            return NewCalibrationObservation(id + "-observation", instrument, "ParRate", quote, 1.0e-10);
        };
        Vector_<Handle_<CalibrationObservation_>> v;
        v.reserve(13);
        v.push_back(mk(Date_(2022, 1, 1), Date_(2022, 1, 3), 1.00));
        v.push_back(mk(Date_(2022, 3, 15), Date_(2022, 6, 15), 1.05));
        v.push_back(mk(Date_(2022, 6, 15), Date_(2022, 9, 21), 1.12));
        v.push_back(mk(Date_(2022, 9, 21), Date_(2022, 12, 21), 1.16));
        v.push_back(mk(Date_(2022, 12, 21), Date_(2023, 3, 15), 1.21));
        v.push_back(mk(Date_(2023, 3, 15), Date_(2023, 6, 21), 1.27));
        v.push_back(mk(Date_(2023, 6, 21), Date_(2023, 9, 20), 1.45));
        v.push_back(mk(Date_(2023, 9, 20), Date_(2023, 12, 20), 1.68));
        v.push_back(mk(Date_(2023, 12, 20), Date_(2024, 3, 15), 1.92));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2025, 1, 1), 1.68));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2027, 1, 1), 2.10));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2029, 1, 1), 2.20));
        v.push_back(mk(Date_(2022, 1, 1), Date_(2032, 1, 1), 2.07));
        return v;
    }

    CurveInstrumentCalibrationSpec_
    BuildSpec(const Date_& today, LogDfScheme_ scheme, const Vector_<Handle_<CalibrationObservation_>>& observations) {
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.curveName_ = "ptirds";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.geometry_.liborBasis_ = DayBasis_("ACT_365F");
        spec.geometry_.knotDates_ = PtirdsKnotDates();
        spec.geometry_.logDfScheme_ = scheme;
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        spec.market_.routes_ = {{"USD.OIS"}};
        spec.observations_ = observations;
        spec.solver_.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.solver_.smoothingWeight_ = 1.0;
        spec.solver_.fitNormTolerance_ = 1.0e-8;
        return spec;
    }

    const char* SchemeName(LogDfScheme_::Value_ s) {
        switch (s) {
        case LogDfScheme_::Value_::LOG_LINEAR:
            return "log-linear";
        case LogDfScheme_::Value_::LOG_CUBIC_NATURAL:
            return "log-cubic";
        case LogDfScheme_::Value_::MIXED:
            return "mixed";
        default:
            return "?";
        }
    }

    void PrintHeader(const char* title) { std::cout << "\n" << std::string(70, '=') << "\n  " << title << "\n" << std::string(70, '=') << "\n\n"; }

    void PrintDfTable(const Vector_<Date_>& dates, const Vector_<>& dfLinear, const Vector_<>& dfCubic, const Vector_<>& dfMixed) {
        PrintHeader("Solved node discount factors (rateslib Table 6.2, 6 dp)");
        const Vector_<int> w = {13, 13, 13, 13};
        std::cout << std::left << std::setw(w[0]) << "Node date" << std::right << std::setw(w[1]) << "log-linear" << std::setw(w[2]) << "log-cubic"
                  << std::setw(w[3]) << "mixed" << '\n';
        std::cout << std::string(52, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(dates.size()); ++i) {
            std::cout << std::left << std::setw(w[0]) << Date::ToString(dates[i]) << std::right << std::setw(w[1]) << dfLinear[i] << std::setw(w[2])
                      << dfCubic[i] << std::setw(w[3]) << dfMixed[i] << '\n';
        }
        std::cout << std::string(52, '-') << "\n\n";
    }

    double AnnualForward(const DiscountCurve_& curve, const Date_& from, const DayBasis_& basis) {
        const auto to = Date::AddMonths(from, 12);
        const double yf = basis(from, to, nullptr);
        const double fwdDf = curve(from, to);
        return -std::log(fwdDf) / yf;
    }

    void PrintForwardTable(
        const Vector_<Date_>& dates, const DiscountCurve_& linear, const DiscountCurve_& cubic, const DiscountCurve_& mixed, const DayBasis_& basis) {
        PrintHeader("1y forward rates at node dates (%)");
        const Vector_<int> w = {13, 13, 13, 13};
        std::cout << std::left << std::setw(w[0]) << "Node date" << std::right << std::setw(w[1]) << "log-linear" << std::setw(w[2]) << "log-cubic"
                  << std::setw(w[3]) << "mixed" << '\n';
        std::cout << std::string(52, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i + 1 < static_cast<int>(dates.size()); ++i) {
            const auto& d = dates[i];
            std::cout << std::left << std::setw(w[0]) << Date::ToString(d) << std::right << std::setw(w[1]) << AnnualForward(linear, d, basis) * 100.0
                      << std::setw(w[2]) << AnnualForward(cubic, d, basis) * 100.0 << std::setw(w[3]) << AnnualForward(mixed, d, basis) * 100.0
                      << '\n';
        }
        std::cout << std::string(52, '-') << "\n\n";
    }

    void PrintResiduals(const CurveInstrumentCalibrationResult_& result, LogDfScheme_::Value_ scheme, double elapsedMs) {
        double maxAbs = 0.0;
        for (double residual : result.rawResiduals_)
            maxAbs = std::max(maxAbs, std::abs(residual));
        const double rms = result.rawNorm_ / std::sqrt(static_cast<double>(result.rawResiduals_.size()));
        std::cout << std::left << std::setw(14) << SchemeName(scheme) << std::right << std::setw(20) << ExampleFloat(maxAbs, 3) << std::setw(20)
                  << ExampleFloat(rms, 3) << std::setw(16) << (result.solveMode_ == CurveSolveMode_::Value_::EXACT ? "exact solve" : "approx fit")
                  << std::setw(14) << ExampleFloat(elapsedMs, 3) << '\n';
    }
} // namespace

int main() {
    InitGlobalData(1);

    const Date_ today(2022, 1, 1);
    const DayBasis_ basis("ACT_365F");

    std::cout << "interpolate_curve_script -- discount-curve interpolation (three LOG_DISCOUNT schemes)\n"
              << "Value date: " << Date::ToString(today) << "    14 nodes, 13 scripted IRS/FRA instruments\n";

    const auto observations = PtirdsObservations();
    auto linSpec = BuildSpec(today, LogDfScheme_::Value_::LOG_LINEAR, observations);
    auto cubSpec = BuildSpec(today, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, observations);
    auto mixSpec = BuildSpec(today, LogDfScheme_::Value_::MIXED, observations);
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.preparationCache_ = std::make_shared<CalibrationPreparationCache_>();
    const auto calibrate = [options](const CurveInstrumentCalibrationSpec_& spec, double& elapsedMs) {
        const auto t0 = std::chrono::steady_clock::now();
        auto result = CalibrateCurveInstruments(spec, options);
        const auto t1 = std::chrono::steady_clock::now();
        elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        REQUIRE(result.parameters_.size() == result.axes_.observations_.size(), "calibration system is not square");
        REQUIRE(result.fitWithinTolerance_, "curve calibration did not fit within tolerance");
        return result;
    };
    double msLin = 0.0, msCub = 0.0, msMix = 0.0;
    const auto rLin = calibrate(linSpec, msLin);
    const auto rCub = calibrate(cubSpec, msCub);
    const auto rMix = calibrate(mixSpec, msMix);

    const auto* cLin = dynamic_cast<const DiscountLogDF_*>(rLin.curve_.get());
    const auto* cCub = dynamic_cast<const DiscountLogDF_*>(rCub.curve_.get());
    const auto* cMix = dynamic_cast<const DiscountLogDF_*>(rMix.curve_.get());
    REQUIRE(cLin && cCub && cMix, "calibrated curve is not a DiscountLogDF_");

    const auto dates = cLin->NodeDates();
    PrintDfTable(dates, cLin->NodeDF(), cCub->NodeDF(), cMix->NodeDF());
    PrintForwardTable(dates, *cLin, *cCub, *cMix, basis);

    PrintHeader("Repricing residuals and calibration time (per scheme)");
    std::cout << std::left << std::setw(14) << "Scheme" << std::right << std::setw(20) << "Max abs residual" << std::setw(20) << "RMS residual"
              << std::setw(16) << "Fit" << std::setw(14) << "Time(ms)" << '\n'
              << std::string(84, '-') << '\n';
    PrintResiduals(rLin, LogDfScheme_::Value_::LOG_LINEAR, msLin);
    PrintResiduals(rCub, LogDfScheme_::Value_::LOG_CUBIC_NATURAL, msCub);
    PrintResiduals(rMix, LogDfScheme_::Value_::MIXED, msMix);
    std::cout << std::string(84, '-') << "\n\n";

    return 0;
}
