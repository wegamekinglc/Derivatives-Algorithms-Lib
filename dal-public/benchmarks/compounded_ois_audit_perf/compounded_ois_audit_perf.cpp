#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal/benchmarks/bench.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/platform/initall.hpp>
#include <dal/time/dateutils.hpp>

namespace {
    using namespace Dal;

    bool BusinessDay(const Date_& day) { return day == Date_(2024, 1, 13) || (!Date::IsWeekEnd(day) && day != Date_(2024, 1, 15)); }
    Date_ EndDate(int count) {
        Date_ day(2024, 1, 1);
        for (int i = 0; i < count; ++i) {
            day = day.AddDays(1);
            while (!BusinessDay(day))
                day = day.AddDays(1);
        }
        return day;
    }
    CompoundedOisScriptSpec_ Contract(int count, const String_& mode) {
        const Date_ start(2024, 1, 1), end = EndDate(count), pay = end.AddDays(2);
        CompoundedOisScriptSpec_ spec;
        spec.instrumentId_ = "audit-" + mode;
        spec.direction_ = "PayFixed";
        spec.notional_ = 100.;
        spec.coupon_ = .02;
        spec.currency_ = spec.collateralCurrency_ = Ccy_("USD");
        spec.collateralType_ = CollateralType_("OIS");
        spec.discountRoute_ = "USD.OIS";
        spec.projectionRoute_ = "USD.SOFR";
        spec.fixingIndex_ = "USD-SOFR";
        spec.observationBasis_ = DayBasis_("ACT_360");
        spec.fixedBasis_ = DayBasis_("ACT_365F");
        spec.calendar_ = HolidayCenterData_("holiday-and-working-weekend", {Date_(2024, 1, 15)}, {Date_(2024, 1, 13)});
        spec.ruleId_ = "dal.compounded-ois-cashflows/2";
        spec.observationConvention_ = CompoundedOisObservationConvention_{
            CompoundedOisObservationMode_(mode), mode == "Lookback" || mode == "ObservationShift" ? 5 : 0, mode == "Lockout" ? 5 : 0};
        spec.fixedLeg_ = {{start, end, pay, (end - start) / 365.}};
        spec.coupons_ = {{"coupon", start, end, pay}};
        return spec;
    }
    CurveScriptEvaluationRequest_ Request(const CompoundedOisScriptSpec_& spec) {
        const Date_ today(2023, 12, 1), last = spec.coupons_[0].payment_.AddDays(20);
        CurveScriptEvaluationRequest_ request;
        request.geometry_.today_ = today;
        request.geometry_.ccy_ = "USD";
        request.geometry_.knotDates_ = {today, last};
        request.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        request.geometry_.calibrateDiscountCurve_ = false;
        request.geometry_.targetOvernightRoute_ = "USD.SOFR";
        request.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        request.valuation_.evaluationDate_ = today;
        request.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        request.compoundedOis_ = PrepareCompoundedOis(spec, request.valuation_);
        request.parameters_ = {-.02 * (last - today) / 365.};
        return request;
    }
    size_t PeakRssKb() {
#ifdef __linux__
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line))
            if (line.rfind("VmHWM:", 0) == 0)
                return std::stoull(line.substr(6));
#endif
        return 0;
    }
    template <class F_> void Measure(const std::string& name, const F_& run) {
        Bench::Print(Bench::Run(name, [&] {
            const auto result = run();
            Bench::DoNotOptimize(&result);
        }));
    }
    template <class F_> void Resources(const std::string& name, int rows, int columns, const F_& run) {
        BufferCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        {
            BufferCapacityScope_ scope(&budget);
            const auto table = run();
            REQUIRE(table.Rows() == rows && table.Cols() == columns, "Audit projection shape changed");
        }
        REQUIRE(budget.CapacityBytes() == 0, "Audit projection retained tracked capacity");
        std::cout << "OIS_AUDIT_RESOURCE case=" << name << " rows=" << rows << " columns=" << columns << " cells=" << size_t(rows) * columns
                  << " cell_payload_lower_bound=" << size_t(rows) * columns * sizeof(Cell_)
                  << " tracked_vector_peak_bytes=" << budget.PeakCapacityBytes() << " after_tracked_bytes=" << budget.CapacityBytes()
                  << " process_peak_rss_kb=" << PeakRssKb() << '\n';
    }
    void Verify(const Matrix_<Cell_>& prepared, const Matrix_<Cell_>& evaluated, int count) {
        REQUIRE(prepared.Rows() == count + 1 && prepared.Cols() == 22, "Prepared audit shape");
        REQUIRE(evaluated.Rows() == count + 1 && evaluated.Cols() == 26, "Evaluated audit shape");
        for (int i = 0; i < prepared.Rows(); ++i)
            for (int j = 0; j < prepared.Cols(); ++j)
                REQUIRE(prepared(i, j) == evaluated(i, j), "Prepared/evaluated audit prefix mismatch");
        REQUIRE(evaluated(1, 3) == Cell_(0.) && evaluated(1, 24) == Cell_(4.) && evaluated(1, 25) == Cell_(1.), "Audit coordinate bases");
    }
    void Run(int count, const String_& mode) {
        const auto request = Request(Contract(count, mode));
        const auto instrument = NewCompoundedOisScriptInstrument(request.compoundedOis_);
        const auto result = EvaluateCurveScriptInstrument(instrument, request);
        REQUIRE(result.observationSchedule_.size() == size_t(count), "Frozen audit schedule count");
        const auto prepared = [&] { return CompoundedOisObservationScheduleTable(request.compoundedOis_); };
        const auto evaluated = [&] { return CurveScriptEvaluationTable(result, "observation_schedule"); };
        Verify(prepared(), evaluated(), count);
        const std::string prefix = std::string(mode.begin(), mode.end()) + "_D" + std::to_string(count);
        Measure(prefix + " prepared_22_column_projection", prepared);
        Measure(prefix + " evaluated_26_column_projection", evaluated);
        Measure(prefix + " owning_evaluation_end_to_end", [&] { return EvaluateCurveScriptInstrument(instrument, request); });
        Resources(prefix + "_prepared", count + 1, 22, prepared);
        Resources(prefix + "_evaluated", count + 1, 26, evaluated);
    }
} // namespace

int main(int argc, char** argv) {
    try {
        REQUIRE(argc == 1 || argc == 5, "usage: compounded_ois_audit_perf [--mode None|Lockout --days 30|1825]");
        std::string mode, days;
        if (argc == 5) {
            REQUIRE(std::string(argv[1]) == "--mode" && std::string(argv[3]) == "--days", "Expected --mode MODE --days DAYS");
            mode = argv[2];
            days = argv[4];
            REQUIRE(mode == "None" || mode == "Lockout", "Selector mode must be None or Lockout");
            REQUIRE(days == "30" || days == "1825", "Selector days must be 30 or 1825");
        }
        Dal::RegisterAll_::Init();
        Dal::Bench::PrintHeader();
        if (argc == 5) {
            Run(days == "30" ? 30 : 1825, Dal::String_(mode));
            std::cout << "OIS audit conversion verified: mode=" << mode << "; D=" << days << '\n';
            return 0;
        }
        for (const int count : {30, 365, 1825})
            for (const Dal::String_ mode : {"None", "Lookback", "ObservationShift", "Lockout"})
                Run(count, mode);
        std::cout << "OIS audit conversion verified: D=30,365,1825; four modes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
