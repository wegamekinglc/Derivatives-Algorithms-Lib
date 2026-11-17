#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <utility>

#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

namespace {
    constexpr double NOTIONAL = 1'000'000.0;
    constexpr double FIXED_RATE = 0.007;
    constexpr double FIT_TOLERANCE = 1.0e-11;
    constexpr double BASE_5Y_QUOTE = 0.00762;
    constexpr double SHOCKED_5Y_QUOTE = 0.0090;

    const Date_ TODAY(2012, 12, 11);

    Date_ AdvanceBusinessDays(const Date_& date, int days, const Holidays_& calendar) {
        auto result = date;
        for (int i = 0; i < days; ++i)
            result = Holidays::NextBus(calendar, result.AddDays(1));
        return result;
    }

    struct Conventions_ {
        Holidays_ calendar_{"TARGET"};
        BizDayConvention_ following_{"Following"};
        BizDayConvention_ modifiedFollowing_{"ModifiedFollowing"};
        DayBasis_ act360_{"ACT_360"};
        RateIndexConvention_ cash_{0, 0, false, PeriodLength_(), act360_, following_, calendar_, calendar_};
        RateIndexConvention_ euribor_{2, 2, true, PeriodLength_("6M"), act360_, modifiedFollowing_, calendar_, calendar_, true};
        RateLegConvention_ ois_{0, PeriodLength_("12M"), act360_, modifiedFollowing_, following_, calendar_, calendar_};
        // The annual fixed dates fall on the 13th, where Bond Basis and 30E/360 agree.
        RateLegConvention_ fixed_{0,        PeriodLength_("12M"), DayBasis_("BOND"), BizDayConvention_("Unadjusted"), modifiedFollowing_, calendar_,
                                  calendar_};
        RateLegConvention_ floating_{0, PeriodLength_("6M"), act360_, modifiedFollowing_, modifiedFollowing_, calendar_, calendar_};
    };

    CurveCalibrationSpec_ CurveSpec(const Date_& anchor, const String_& name) {
        CurveCalibrationSpec_ spec;
        spec.today_ = anchor;
        spec.ccy_ = "EUR";
        spec.curveName_ = name;
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.logDfScheme_ = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;
        spec.knotDates_ = {anchor};
        spec.initialGuess_ = 0.01;
        spec.tolerance_ = FIT_TOLERANCE;
        spec.fitTolerance_ = FIT_TOLERANCE;
        return spec;
    }

    Handle_<DiscountCurve_> Calibrate(CurveCalibrationSpec_ spec, const Holidays_& calendar) {
        for (const auto& instrument : spec.instruments_)
            spec.knotDates_.push_back(Holidays::NextBus(calendar, instrument->TimeSpan().second));
        std::sort(spec.knotDates_.begin(), spec.knotDates_.end());

        CurveCalibrationOptions_ options;
        options.computeEffJacobianInverse_ = false;
        options.computeForwardJacobian_ = false;
        auto result = CalibrateYieldCurve(spec, options);
        const double residual = result.diagnostics_.maxAbsResidual_;
        REQUIRE(residual <= FIT_TOLERANCE, "Calibration did not recover the market quotes");
        std::cout << spec.curveName_ << ": " << spec.instruments_.size() << " quotes, max absolute residual " << std::scientific
                  << std::setprecision(2) << 1.0e4 * residual << " bp\n";
        return Handle_<DiscountCurve_>(result.curve_.release());
    }

    Handle_<DiscountCurve_> BuildEstr(const Date_& spot, const Conventions_& conventions) {
        auto spec = CurveSpec(TODAY, "ESTR");
        for (int lag : {0, 1, 2}) {
            const auto start = AdvanceBusinessDays(TODAY, lag, conventions.calendar_);
            const auto end = AdvanceBusinessDays(start, 1, conventions.calendar_);
            spec.instruments_.emplace_back(new Deposit_(TODAY, start, end, 0.0004, conventions.cash_));
        }

        const auto addOis = [&](const Date_& start, const Date_& end, double quote) {
            spec.instruments_.emplace_back(new OISSwap_(TODAY, start, end, quote, conventions.ois_, conventions.cash_, conventions.ois_));
        };
        addOis(spot, spot.AddDays(7), 0.00070);
        addOis(spot, spot.AddDays(14), 0.00069);
        addOis(spot, spot.AddDays(21), 0.00078);
        addOis(spot, Date::AddMonths(spot, 1), 0.00074);

        const Date_ meetingDates[] = {{2013, 1, 16}, {2013, 2, 13}, {2013, 3, 13}, {2013, 4, 10}, {2013, 5, 8}, {2013, 6, 12}};
        const double meetingQuotes[] = {0.000460, 0.000160, -0.000070, -0.000130, -0.000140};
        for (size_t i = 0; i < std::size(meetingQuotes); ++i)
            addOis(meetingDates[i], meetingDates[i + 1], meetingQuotes[i]);

        const pair<int, double> swapQuotes[] = {
            {15, 0.00002},  {18, 0.00008},  {21, 0.00021},  {24, 0.00036},  {36, 0.00127},  {48, 0.00274},
            {60, 0.00456},  {72, 0.00647},  {84, 0.00827},  {96, 0.00996},  {108, 0.01147}, {120, 0.01280},
            {132, 0.01404}, {144, 0.01516}, {180, 0.01764}, {240, 0.01939}, {300, 0.02003}, {360, 0.02038},
        };
        for (const auto& [months, quote] : swapQuotes)
            addOis(spot, Date::AddMonths(spot, months), quote);

        return Calibrate(std::move(spec), conventions.calendar_);
    }

    Handle_<DiscountCurve_>
    BuildEuribor(const Date_& spot, const Conventions_& conventions, const Handle_<DiscountCurve_>& discount, double fiveYearQuote) {
        auto spec = CurveSpec(spot, "Euribor 6M");
        spec.calibrateDiscountCurve_ = false;
        spec.targetTenor_ = conventions.euribor_.forecastTenor_;
        spec.discountCurves_.emplace(spec.targetCollateral_, discount);

        auto depositIndex = conventions.cash_;
        depositIndex.useProjectionCurve_ = true;
        depositIndex.forecastTenor_ = spec.targetTenor_;
        const auto depositStart = AdvanceBusinessDays(TODAY, 3, conventions.calendar_);
        const auto depositEnd = Holidays::Adjust(conventions.calendar_, Date::AddMonths(depositStart, 6), conventions.following_);
        spec.instruments_.emplace_back(new Deposit_(spot, depositStart, depositEnd, 0.00312, depositIndex));

        const double fraQuotes[] = {0.002930, 0.002720, 0.002600, 0.002560, 0.002520, 0.002480, 0.002540, 0.002610, 0.002670,
                                    0.002790, 0.002910, 0.003030, 0.003180, 0.003350, 0.003520, 0.003710, 0.003890, 0.004090};
        const auto advance = [&](const Date_& date, int months) {
            return Holidays::Adjust(conventions.calendar_, Date::AddMonths(date, months), conventions.modifiedFollowing_);
        };
        for (int i = 0; i < static_cast<int>(std::size(fraQuotes)); ++i) {
            const auto start = advance(spot, i + 1);
            const auto end = advance(start, 6);
            spec.instruments_.emplace_back(new FRA_(spot, start, end, fraQuotes[i], conventions.euribor_));
        }

        const pair<int, double> swapQuotes[] = {
            {3, 0.004240},  {4, 0.005760},  {5, fiveYearQuote}, {6, 0.009540},  {7, 0.011350},  {8, 0.013030},
            {9, 0.014520},  {10, 0.015840}, {12, 0.018090},     {15, 0.020370}, {20, 0.021870}, {25, 0.022340},
            {30, 0.022560}, {35, 0.022950}, {40, 0.023480},     {50, 0.024210}, {60, 0.024630},
        };
        for (const auto& [years, quote] : swapQuotes) {
            const auto maturity = Date::AddMonths(spot, 12 * years);
            spec.instruments_.emplace_back(new Swap_(spot, spot, maturity, quote, conventions.fixed_, conventions.euribor_, conventions.floating_));
        }
        return Calibrate(std::move(spec), conventions.calendar_);
    }

    double Annuity(const Date_& start, const Date_& end, const RateLegConvention_& leg, const DiscountCurve_& discount) {
        const auto periods =
            MakeSchedulePeriods(start, end, leg.paymentFrequency_, leg.accrualHolidays_, 0, Holidays::None(), leg.paymentLag_, leg.paymentHolidays_,
                                DateGeneration_("Forward"), leg.businessDayConvention_, leg.paymentConvention_, leg.endOfMonth_);
        double annuity = 0.0;
        for (const auto& period : periods)
            annuity += discount(TODAY, period.paymentDate_) * leg.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
        return annuity;
    }

    struct SwapValue_ {
        double npv_;
        double fairSpread_;
        double fairRate_;
    };

    SwapValue_
    Value(const Date_& start, const Conventions_& conventions, const Handle_<DiscountCurve_>& discount, const Handle_<DiscountCurve_>& projection) {
        const auto maturity = Date::AddMonths(start, 60);
        const CurveBlock_ market("EUR rates", "EUR", {{conventions.euribor_.collateral_, discount}},
                                 {{conventions.euribor_.forecastTenor_, projection}});
        const Swap_ swap(TODAY, start, maturity, FIXED_RATE, conventions.fixed_, conventions.euribor_, conventions.floating_);
        const double fairRate = (*swap.Precompute({}))(market);
        const double fixedAnnuity = Annuity(start, maturity, conventions.fixed_, *discount);
        const double floatAnnuity = Annuity(start, maturity, conventions.floating_, *discount);
        const double pvPerUnit = (fairRate - FIXED_RATE) * fixedAnnuity;
        return {NOTIONAL * pvPerUnit, -pvPerUnit / floatAnnuity, fairRate};
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const Conventions_ conventions;
    const auto spot = AdvanceBusinessDays(TODAY, 2, conventions.calendar_);
    const auto forward = Holidays::Adjust(conventions.calendar_, Date::AddMonths(spot, 12), conventions.following_);

    std::cout << "EUR multi-curve example, 2012-12-11, natural cubic log-DF interpolation\n";
    const Timer_ timer;
    const auto discount = BuildEstr(spot, conventions);
    const auto projection = BuildEuribor(spot, conventions, discount, BASE_5Y_QUOTE);
    const auto shockedProjection = BuildEuribor(spot, conventions, discount, SHOCKED_5Y_QUOTE);

    const auto baseSpot = Value(spot, conventions, discount, projection);
    const auto baseForward = Value(forward, conventions, discount, projection);
    const auto shockedSpot = Value(spot, conventions, discount, shockedProjection);
    const auto shockedForward = Value(forward, conventions, discount, shockedProjection);
    const double elapsedMs = 0.001 * static_cast<double>(timer.Elapsed<>());
    REQUIRE(std::abs(baseSpot.fairRate_ - BASE_5Y_QUOTE) <= FIT_TOLERANCE, "Spot swap does not reprice the original 5Y quote");
    REQUIRE(std::abs(shockedSpot.fairRate_ - SHOCKED_5Y_QUOTE) <= FIT_TOLERANCE, "Spot swap does not reprice the shocked 5Y quote");

    const auto print = [](const char* scenario, const SwapValue_& value, double referenceNpv, double referenceSpread, double referenceRate) {
        std::cout << std::left << std::setw(20) << scenario << std::right << std::fixed << std::setprecision(2) << std::setw(15) << value.npv_
                  << std::setw(15) << referenceNpv << std::setprecision(5) << std::setw(14) << 100.0 * value.fairSpread_ << std::setw(12)
                  << 100.0 * value.fairRate_ << '\n';
        REQUIRE(std::abs(value.npv_ - referenceNpv) < 0.005, String_(scenario) + " NPV differs from the published value at cent precision");
        REQUIRE(std::abs(value.fairSpread_ - referenceSpread) < 0.00005, String_(scenario) + " fair spread differs from the published value");
        REQUIRE(std::abs(value.fairRate_ - referenceRate) < 0.00005, String_(scenario) + " fair fixed rate differs from the published value");
    };
    std::cout << "\n5Y input / trade      DAL NPV (EUR)   NanoQL (EUR)    Spread (%)    Rate (%)\n"
              << "----------------------------------------------------------------------------\n";
    print("0.762%  5Y spot", baseSpot, 3076.04, -0.0006, 0.0076);
    print("0.762%  1Y x 5Y", baseForward, 19202.54, -0.0038, 0.0109);
    print("0.900%  5Y spot", shockedSpot, 9922.71, -0.0020, 0.0090);
    print("0.900%  1Y x 5Y", shockedForward, 19202.54, -0.0038, 0.0109);
    std::cout << "\nCalibration and pricing time: " << std::fixed << std::setprecision(3) << elapsedMs << " ms\n";
}
