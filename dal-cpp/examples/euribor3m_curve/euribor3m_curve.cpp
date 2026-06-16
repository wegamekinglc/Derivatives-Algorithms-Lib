//
// Created by Claude on 2026/6/16.
//
// Single-curve Euribor 3M bootstrap from market quotes (as-of 2026-04-30).
// Market data is embedded verbatim from three CSVs:
//   - EUR_cash_rates.csv                    (3M cash deposit, only 3M used)
//   - EUR_serial_futures_3M_euribor.csv     (3M Euribor serial futures, used up to MAR 28+3)
//   - EUR_swap_rates.csv                    (EUR vanilla swaps, used beyond 2.5y)
//
// All instruments reference 3M Euribor and discount/forecast off one curve
// (classic single-curve construction). No OIS data is assumed.

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/currency/currency.hpp>
#include <dal/currency/currencydata.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/math/vectors.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

namespace {
    // ---- Embedded market data (as-of 2026-04-30) ---------------------------

    struct FutureRow_ {
        const char* contract; // e.g. "MAY 26+3"  (month  2-digit year + 3M tenor)
        double price;
        double convexityAdj; // percent points (matches the CSV Cvx_Adj column)
        double rate;         // convexity-adjusted forward rate, percent
    };

    struct SwapRow_ {
        int months;
        double bid; // percent
        double ask; // percent
    };

    // EUR_cash_rates.csv: only the 3M deposit is used.
    constexpr int CASH_MONTHS = 3;
    constexpr double CASH_3M_RATE = 2.19900; // percent

    // EUR_serial_futures_3M_euribor.csv (full 28 rows); selection by maturity below.
    const Vector_<FutureRow_> FUTURES = {
        {"MAY 26+3", 97.75,  -8e-05,   2.24992},
        {"JUN 26+3", 97.605, -0.00025, 2.39475},
        {"JUL 26+3", 97.48,  -0.00049, 2.51951},
        {"AUG 26+3", 97.38,  -0.00079, 2.61921},
        {"SEP 26+3", 97.295, -0.0011,  2.7039},
        {"OCT 26+3", 97.245, -0.00153, 2.75347},
        {"DEC 26+3", 97.175, -0.00235, 2.82265},
        {"MAR 27+3", 97.14,  -0.00402, 2.85598},
        {"JUN 27+3", 97.17,  -0.00608, 2.82392},
        {"SEP 27+3", 97.245, -0.00852, 2.74648},
        {"DEC 27+3", 97.305, -0.01134, 2.68366},
        {"MAR 28+3", 97.325, -0.01477, 2.66023},
        {"JUN 28+3", 97.32,  -0.01837, 2.66163},
        {"SEP 28+3", 97.31,  -0.02229, 2.66771},
        {"DEC 28+3", 97.295, -0.02657, 2.67843},
        {"MAR 29+3", 97.275, -0.03118, 2.69383},
        {"JUN 29+3", 97.25,  -0.03611, 2.71389},
        {"SEP 29+3", 97.22,  -0.04137, 2.73863},
        {"DEC 29+3", 97.19,  -0.04694, 2.76307},
        {"MAR 30+3", 97.16,  -0.05281, 2.78719},
        {"JUN 30+3", 97.13,  -0.05898, 2.81102},
        {"SEP 30+3", 97.1,   -0.06545, 2.83455},
        {"DEC 30+3", 97.07,  -0.07219, 2.85781},
        {"MAR 31+3", 97.04,  -0.07921, 2.88079},
        {"JUN 31+3", 97.01,  -0.0865,  2.90351},
        {"SEP 31+3", 96.98,  -0.09406, 2.92594},
        {"DEC 31+3", 96.95,  -0.10187, 2.94813},
        {"MAR 32+3", 96.92,  -0.10993, 2.97007},
    };

    // EUR_swap_rates.csv (full 19 rows); 1Y/2Y are dropped by the >=3Y rule below.
    const Vector_<SwapRow_> SWAPS = {
        {12, 2.65817, 2.66463}, {24, 2.73975, 2.74605}, {36, 2.7392, 2.7494},
        {48, 2.75875, 2.76725}, {60, 2.78809, 2.79711}, {72, 2.82247, 2.83713},
        {84, 2.86774, 2.87626}, {96, 2.91091, 2.92009}, {108, 2.9567, 2.9655},
        {120, 2.99868, 3.01082}, {132, 3.04228, 3.05152}, {144, 3.08192, 3.09109},
        {180, 3.17367, 3.18463}, {240, 3.22314, 3.23476}, {300, 3.1974, 3.2109},
        {360, 3.15252, 3.16448}, {420, 3.09317, 3.10683}, {480, 3.03007, 3.04233},
        {600, 2.89103, 2.90337},
    };

    // Futures are used up to and including MAR 28+3 (by settle month).
    constexpr int FUTURES_LAST_YEAR = 2028;
    constexpr int FUTURES_LAST_MONTH = 3;

    // Swaps are used from 3Y onward (i.e. maturities beyond the futures region).
    constexpr int SWAPS_MIN_MONTHS = 36;

    // Reference (benchmark) curve pillars from EUR_3M_EURIBOR_curve_20260430.csv.
    // Zero Rate is continuously-compounded ACT/365; Discount is P(today, date), today = 2026-04-30.
    struct BenchmarkRow_ {
        int mm, dd, yyyy;
        double zeroPct;
        double discount;
    };
    const Vector_<BenchmarkRow_> BENCHMARK = {
        {8, 5, 2026, 2.22330, 0.994109},   {8, 19, 2026, 2.26545, 0.993134},  {9, 16, 2026, 2.35253, 0.991081},
        {10, 21, 2026, 2.40493, 0.988601}, {11, 18, 2026, 2.43726, 0.986602}, {12, 16, 2026, 2.50272, 0.984353},
        {1, 20, 2027, 2.53443, 0.981768},  {3, 17, 2027, 2.60165, 0.977380},  {6, 16, 2027, 2.66429, 0.970374},
        {9, 15, 2027, 2.69842, 0.963496},  {12, 15, 2027, 2.71016, 0.956853}, {3, 15, 2028, 2.71037, 0.950406},
        {6, 21, 2028, 2.70750, 0.943573},  {5, 7, 2029, 2.70462, 0.921519},   {5, 6, 2030, 2.72445, 0.896282},
        {5, 5, 2031, 2.75519, 0.870914},   {5, 5, 2032, 2.79260, 0.845276},   {5, 5, 2033, 2.83700, 0.819440},
        {5, 5, 2034, 2.88313, 0.793578},   {5, 7, 2035, 2.93204, 0.767508},   {5, 5, 2036, 2.97854, 0.741926},
        {5, 5, 2037, 3.02480, 0.716490},   {5, 5, 2038, 3.06872, 0.691481},   {5, 6, 2041, 3.17240, 0.620811},
        {5, 7, 2046, 3.22420, 0.524190},   {5, 5, 2051, 3.18056, 0.451085},   {5, 5, 2056, 3.10780, 0.393196},
        {5, 5, 2066, 2.91443, 0.311309},   {5, 5, 2076, 2.68596, 0.260721},
    };

    int MonthFromAbbrev(const std::string& s) {
        static const std::map<std::string, int> names = {{"JAN", 1},  {"FEB", 2},  {"MAR", 3}, {"APR", 4},
                                                         {"MAY", 5},  {"JUN", 6},  {"JUL", 7}, {"AUG", 8},
                                                         {"SEP", 9},  {"OCT", 10}, {"NOV", 11}, {"DEC", 12}};
        const auto it = names.find(s);
        REQUIRE(it != names.end(), String_("Unknown future contract month: ") + String_(s));
        return it->second;
    }

    struct ParsedContract_ {
        int year;
        int month;
        int tenorMonths;
    };

    ParsedContract_ ParseContract(const char* label) {
        const std::string s(label);
        const auto space = s.find(' ');
        REQUIRE(space != std::string::npos, String_("Bad future label: ") + String_(s));
        const std::string monthAbbrev = s.substr(0, space);
        const std::string rest = s.substr(space + 1);
        const auto plus = rest.find('+');
        REQUIRE(plus != std::string::npos, String_("Bad future label (missing +3): ") + String_(s));
        const int yy = std::stoi(rest.substr(0, plus));
        const int tenor = std::stoi(rest.substr(plus + 1));
        return {2000 + yy, MonthFromAbbrev(monthAbbrev), tenor};
    }

    // Third Wednesday of (year, month): the standard 3M Euribor future expiry / fixing date.
    // Date::DayOfWeek: 0=Sun .. 6=Sat, so Wednesday == 3.
    Date_ ThirdWednesday(int year, int month) {
        const Date_ first(year, month, 1);
        const int dow = Date::DayOfWeek(first);
        const int offset = (3 - dow + 7) % 7; // days to the first Wednesday
        return first.AddDays(offset + 14);    // third Wednesday
    }

    // Advance by n business days on a calendar (for spot / fixing value-date lags).
    Date_ AddBusinessDays(const Holidays_& hols, const Date_& from, int n) {
        Date_ d = from;
        for (int i = 0; i < n; ++i)
            d = Holidays::NextBus(hols, d.AddDays(1));
        return d;
    }

    // Mirrors calibration.cpp OrderInstruments so display names line up with diagnostics.
    struct InstrumentEntry_ {
        Handle_<YCInstrument_> inst;
        String_ name;
    };

    Vector_<InstrumentEntry_> OrderEntries(const Vector_<InstrumentEntry_>& entries) {
        auto ordered = entries;
        std::sort(ordered.begin(), ordered.end(), [](const InstrumentEntry_& lhs, const InstrumentEntry_& rhs) {
            const auto ls = lhs.inst->TimeSpan();
            const auto rs = rhs.inst->TimeSpan();
            if (ls.second != rs.second)
                return ls.second < rs.second;
            if (ls.first != rs.first)
                return ls.first < rs.first;
            return lhs.name < rhs.name;
        });
        return ordered;
    }

    void PrintInstruments(const Vector_<InstrumentEntry_>& entries) {
        const Vector_<int> w = {22, 12, 12, 12};
        std::cout << std::left << std::setw(w[0]) << "Instrument" << std::right << std::setw(w[1]) << "Start"
                  << std::setw(w[2]) << "Maturity" << std::setw(w[3]) << "Market(%)" << '\n';
        std::cout << std::string(58, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (const auto& e : entries) {
            const auto span = e.inst->TimeSpan();
            std::cout << std::left << std::setw(w[0]) << e.name << std::right << std::setw(w[1])
                      << Date::ToString(span.first) << std::setw(w[2]) << Date::ToString(span.second)
                      << std::setw(w[3]) << e.inst->MarketRate() * 100.0 << '\n';
        }
        std::cout << '\n';
    }

    void PrintResiduals(const CurveCalibrationDiagnostics_& d, const Vector_<String_>& names) {
        const Vector_<int> w = {26, 12, 12, 12};
        std::cout << std::left << std::setw(w[0]) << "Instrument" << std::right << std::setw(w[1]) << "Market(%)"
                  << std::setw(w[2]) << "Model(%)" << std::setw(w[3]) << "Error(bp)" << '\n';
        std::cout << std::string(62, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < static_cast<int>(d.instrumentNames_.size()); ++i) {
            std::cout << std::left << std::setw(w[0])
                      << (static_cast<size_t>(i) < names.size() ? names[i].c_str() : d.instrumentNames_[i].c_str())
                      << std::right << std::setw(w[1]) << d.marketRates_[i] * 100.0 << std::setw(w[2])
                      << d.modelRates_[i] * 100.0 << std::setw(w[3]) << d.residuals_[i] * 10000.0 << '\n';
        }
        std::cout << std::string(62, '-') << '\n';
        std::cout << std::left << std::setw(w[0]) << "RMS / max abs residual" << std::right << std::setw(w[1]) << ""
                  << std::setw(w[2]) << d.rmsResidual_ * 10000.0 << std::setw(w[3]) << d.maxAbsResidual_ * 10000.0
                  << "  (bp)\n\n";
    }

    void PrintCurve(const DiscountCurve_& curve, const Date_& today, const Vector_<int>& tenorMonths) {
        const Vector_<int> w = {8, 14, 14, 16};
        std::cout << std::left << std::setw(w[0]) << "Tenor" << std::right << std::setw(w[1]) << "Maturity"
                  << std::setw(w[2]) << "ZeroRate(%)" << std::setw(w[3]) << "DiscountFactor" << '\n';
        std::cout << std::string(52, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        for (const int m : tenorMonths) {
            const std::string label = (m % 12 == 0) ? (std::to_string(m / 12) + "Y") : (std::to_string(m) + "M");
            const Date_ d = Date::AddMonths(today, m);
            const double df = curve(today, d);
            const double yf = static_cast<double>(d - today) / 365.0;
            const double zero = yf > 0.0 ? -std::log(df) / yf : 0.0;
            std::cout << std::left << std::setw(w[0]) << label << std::right << std::setw(w[1])
                      << Date::ToString(d) << std::setw(w[2]) << zero * 100.0 << std::setw(w[3]) << df << '\n';
        }
        std::cout << '\n';
    }

    void PrintBenchmarkComparison(const DiscountCurve_& curve,
                                  const Date_& today,
                                  const Vector_<BenchmarkRow_>& bench) {
        const Vector_<int> w = {12, 11, 11, 10, 11, 11, 11};
        std::cout << std::left << std::setw(w[0]) << "Date" << std::right << std::setw(w[1]) << "BenchZero%"
                  << std::setw(w[2]) << "MineZero%" << std::setw(w[3]) << "dZero(bp)" << std::setw(w[4]) << "BenchDF"
                  << std::setw(w[5]) << "MineDF" << std::setw(w[6]) << "dDF" << '\n';
        std::cout << std::string(77, '-') << '\n';
        std::cout << std::fixed << std::setprecision(6);
        double maxAbsDz = 0.0, sqDz = 0.0, maxAbsDdf = 0.0;
        int n = 0;
        for (const auto& b : bench) {
            const Date_ d(b.yyyy, b.mm, b.dd);
            const double myDf = curve(today, d);
            const double yf = static_cast<double>(d - today) / 365.0;
            const double myZero = yf > 0.0 ? -std::log(myDf) / yf * 100.0 : 0.0;
            const double dz = (myZero - b.zeroPct) * 100.0; // bp
            const double ddf = myDf - b.discount;
            maxAbsDz = std::max(maxAbsDz, std::fabs(dz));
            sqDz += dz * dz;
            maxAbsDdf = std::max(maxAbsDdf, std::fabs(ddf));
            ++n;
            std::cout << std::left << std::setw(w[0]) << Date::ToString(d) << std::right << std::setw(w[1])
                      << b.zeroPct << std::setw(w[2]) << myZero << std::setw(w[3]) << dz << std::setw(w[4])
                      << b.discount << std::setw(w[5]) << myDf << std::setw(w[6]) << ddf << '\n';
        }
        std::cout << std::string(77, '-') << '\n';
        std::cout << "\n  dZero (bp):  RMS = " << std::sqrt(sqDz / n) << "   max abs = " << maxAbsDz
                  << "      dDF:  max abs = " << maxAbsDdf << "\n\n";
    }

    struct InstrumentCounts_ {
        int nCash = 0;
        int nFutures = 0;
        int nSwaps = 0;
    };

    void AppendCash(Vector_<InstrumentEntry_>* entries,
                    InstrumentCounts_* counts,
                    const Date_& today,
                    const Date_& spot,
                    const RateIndexConvention_& euribor3m) {
        entries->push_back({Handle_<YCInstrument_>(new Deposit_(today,
                                                                 spot,
                                                                 Date::AddMonths(spot, CASH_MONTHS),
                                                                 CASH_3M_RATE / 100.0,
                                                                 euribor3m)),
                            "CASH 3M"});
        ++counts->nCash;
    }

    void AppendFutures(Vector_<InstrumentEntry_>* entries,
                       InstrumentCounts_* counts,
                       const Date_& today,
                       const RateIndexConvention_& euribor3m) {
        for (const auto& f : FUTURES) {
            const auto pc = ParseContract(f.contract);
            if (pc.year > FUTURES_LAST_YEAR || (pc.year == FUTURES_LAST_YEAR && pc.month > FUTURES_LAST_MONTH))
                continue;
            const Date_ settle = ThirdWednesday(pc.year, pc.month); // IMM date (3rd Wed), always a TARGET business day
            // Underlying 3M period is IMM-to-IMM: from this 3rd Wednesday to the 3rd Wednesday three months later
            // (not settle + 3M, which keeps the day-of-month and lands on the wrong date).
            const Date_ monthAfter = Date::AddMonths(settle, pc.tenorMonths);
            const Date_ accrualEnd = ThirdWednesday(Date::Year(monthAfter), Date::Month(monthAfter));
            // The CSV Rate column is already convexity-adjusted (Rate = 100 - Price + Cvx_Adj),
            // so we calibrate to it directly with zero convexity in the instrument.
            // (Equivalent: marketRate = (100 - Price)/100, convexityAdjustment = Cvx_Adj/100.)
            entries->push_back(
                {Handle_<YCInstrument_>(new Future_(today, settle, accrualEnd, f.rate / 100.0, euribor3m, 0.0)),
                 String_("FUT ") + String_(f.contract)});
            ++counts->nFutures;
        }
    }

    void AppendSwaps(Vector_<InstrumentEntry_>* entries,
                     InstrumentCounts_* counts,
                     const Date_& today,
                     const Date_& spot,
                     const RateLegConvention_& fixedLeg,
                     const RateIndexConvention_& euribor3m,
                     const RateLegConvention_& floatLeg) {
        for (const auto& s : SWAPS) {
            if (s.months < SWAPS_MIN_MONTHS)
                continue;
            const double mid = (s.bid + s.ask) / 2.0 / 100.0;
            // Maturity is passed UNADJUSTED on purpose: the schedule generator rolls every coupon
            // (including the last) to ModifiedFollowing on TARGET, so the final cash flow already lands
            // on MF(spot+tenor) -- the benchmark pillar. Pre-rolling maturity itself would insert an
            // extra unadjusted grid point and create a zero-length final stub (NaN) for swaps whose
            // spot+tenor is a weekend (e.g. 3Y -> 2029-05-05 Sat).
            entries->push_back({Handle_<YCInstrument_>(new Swap_(today,
                                                                 spot,
                                                                 Date::AddMonths(spot, s.months),
                                                                 mid,
                                                                 fixedLeg,
                                                                 euribor3m,
                                                                 floatLeg)),
                                String_("SWAP ") + String::FromInt(s.months / 12) + "Y"});
            ++counts->nSwaps;
        }
    }

    void BuildCalibrationSpec(const Date_& today,
                              const Vector_<InstrumentEntry_>& ordered,
                              CurveCalibrationSpec_* spec,
                              Vector_<String_>* displayNames) {
        spec->today_ = today;
        spec->ccy_ = "EUR";
        spec->curveName_ = "euribor3m";
        spec->targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS); // single-curve key only
        spec->calibrateDiscountCurve_ = true;
        spec->parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
        spec->knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec->liborBasis_ = DayBasis_("ACT_365F");
        spec->instruments_.reserve(ordered.size());
        displayNames->reserve(ordered.size());
        spec->knotDates_.reserve(ordered.size());
        for (const auto& e : ordered) {
            spec->instruments_.push_back(e.inst);
            displayNames->push_back(e.name);
            spec->knotDates_.push_back(e.inst->TimeSpan().second);
        }
        std::sort(spec->knotDates_.begin(), spec->knotDates_.end());
        spec->knotDates_.erase(std::unique(spec->knotDates_.begin(), spec->knotDates_.end()), spec->knotDates_.end());
    }
} // namespace

int main() {
    RegisterAll_::Init();

    const Date_ today(2026, 4, 30);
    const Holidays_ target("TARGET");
    const Date_ spot = AddBusinessDays(target, today, 2); // Euribor T+2 on TARGET calendar

    // ---- EUR conventions (single-curve: forecast routes to the discount curve) ----
    RateIndexConvention_ euribor3m = Ccy::Conventions::LiborIndex()(Ccy_("EUR"));
    euribor3m.useProjectionCurve_ = false; // single curve: 3M forecast == discount
    euribor3m.forecastTenor_ = PeriodLength_("3M");
    euribor3m.dayBasis_ = DayBasis_("ACT_360");
    euribor3m.businessDayConvention_ = BizDayConvention_("ModifiedFollowing");
    euribor3m.accrualHolidays_ = target;
    euribor3m.fixingHolidays_ = target;

    RateLegConvention_ fixedLeg = Ccy::Conventions::SwapFixedLeg()(Ccy_("EUR"));
    fixedLeg.paymentFrequency_ = PeriodLength_("12M"); // EUR vanilla IRS fixed leg is annual
    fixedLeg.dayBasis_ = DayBasis_("30_360");          // library has 30_360 (proxy for 30E/360)
    fixedLeg.accrualHolidays_ = target;
    fixedLeg.paymentHolidays_ = target;

    RateLegConvention_ floatLeg = Ccy::Conventions::SwapFloatLeg()(Ccy_("EUR"));
    floatLeg.paymentFrequency_ = PeriodLength_("3M"); // 3M Euribor float leg
    floatLeg.dayBasis_ = DayBasis_("ACT_360");
    floatLeg.accrualHolidays_ = target;
    floatLeg.paymentHolidays_ = target;

    Vector_<InstrumentEntry_> entries;
    InstrumentCounts_ counts;
    AppendCash(&entries, &counts, today, spot, euribor3m);
    AppendFutures(&entries, &counts, today, euribor3m);
    AppendSwaps(&entries, &counts, today, spot, fixedLeg, euribor3m, floatLeg);

    const auto ordered = OrderEntries(entries);

    CurveCalibrationSpec_ spec;
    Vector_<String_> displayNames;
    BuildCalibrationSpec(today, ordered, &spec, &displayNames);

    std::cout << "\n" << std::string(70, '=') << "\n"
              << "  Euribor 3M single-curve bootstrap  (as-of " << Date::ToString(today) << ")\n"
              << std::string(70, '=') << "\n"
              << "  spot: " << Date::ToString(spot) << "    futures last contract: MAR 28+3\n"
              << "  instruments: " << counts.nCash << " cash + " << counts.nFutures << " futures + " << counts.nSwaps
              << " swaps = " << (counts.nCash + counts.nFutures + counts.nSwaps) << "    knots: " << spec.knotDates_.size()
              << "\n\n";
    PrintInstruments(ordered);

    Timer_ timer;
    timer.Reset();
    const auto result = CalibrateYieldCurve(spec);
    const auto elapsedMs = timer.Elapsed<milliseconds>();

    std::cout << "  Calibration residuals  (elapsed: " << elapsedMs << " ms)\n";
    std::cout << "  " << std::string(36, '-') << "\n\n";
    PrintResiduals(result.diagnostics_, displayNames);

    std::cout << std::string(70, '=') << "\n"
              << "  Calibrated curve (continuously-compounded zero, ACT/365F)\n"
              << std::string(70, '=') << "\n\n";
    PrintCurve(*result.curve_, today, {6, 12, 24, 36, 60, 84, 120, 180, 240, 360});

    std::cout << std::string(70, '=') << "\n"
              << "  Benchmark comparison  (EUR_3M_EURIBOR_curve_20260430.csv)\n"
              << std::string(70, '=') << "\n\n";
    PrintBenchmarkComparison(*result.curve_, today, BENCHMARK);

    return 0;
}
