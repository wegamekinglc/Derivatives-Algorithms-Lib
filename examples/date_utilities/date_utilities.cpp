//
// Created by wegam on 2020/12/5.
//

#include <iostream>
#include <format>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;
using namespace std;

int main() {
    Dal::RegisterAll_::Init();

    cout << "--- Basic Date Operations ---" << endl;
    Date_ d(2025, 12, 6);
    cout << std::format("Current date: {0}", Date::ToString(d).c_str()) << endl;
    cout << std::format("Year: {0}, Month: {1}, Day: {2}", Date::Year(d), Date::Month(d), Date::Day(d)) << endl;
    cout << std::format("Day of week: {0} (0=Sun, 6=Sat)", Date::DayOfWeek(d)) << endl;
    cout << std::format("Is weekend: {0}", Date::IsWeekEnd(d) ? "Yes" : "No") << endl;
    cout << std::format("Excel serial: {0}", Date::ToExcel(d)) << endl;
    cout << endl;

    cout << "--- Date Arithmetic ---" << endl;
    Date_ d_plus_10 = d.AddDays(10);
    cout << std::format("Add 10 days: {0}", Date::ToString(d_plus_10).c_str()) << endl;
    
    Date_ d_minus_5 = d.AddDays(-5);
    cout << std::format("Subtract 5 days: {0}", Date::ToString(d_minus_5).c_str()) << endl;
    
    int days_diff = d_plus_10 - d;
    cout << std::format("Days between dates: {0}", days_diff) << endl;
    
    Date_ eom = Date::EndOfMonth(d);
    cout << std::format("End of month: {0}", Date::ToString(eom).c_str()) << endl;
    
    Date_ plus_3m = Date::AddMonths(d, 3);
    cout << std::format("Add 3 months: {0}", Date::ToString(plus_3m).c_str()) << endl;
    
    Date_ plus_1y = Date::AddMonths(d, 12);
    cout << std::format("Add 1 year: {0}", Date::ToString(plus_1y).c_str()) << endl;
    cout << endl;

    cout << "--- Business Day Adjustments ---" << endl;
    Holidays_ hol("CN.SSE CN.IB");
    cout << std::format("Holidays: {0}", hol.String().c_str()) << endl;
    
    Date_ next_d = Holidays::NextBus(hol, d);
    cout << std::format("Next business date: {0}", Date::ToString(next_d).c_str()) << endl;

    Date_ prev_d = Holidays::PrevBus(hol, d);
    cout << std::format("Previous business date: {0}", Date::ToString(prev_d).c_str()) << endl;

    CountBusDays_ counter(hol);
    int bus_days = counter(d, Date_(2025, 12, 31));
    cout << std::format("Business days to year end: {0}", bus_days) << endl;
    cout << endl;

    cout << "--- Date Range Iteration ---" << endl;
    Date_ start = Date_(2025, 12, 1);
    Date_ end = Date_(2025, 12, 10);
    cout << std::format("Dates from {0} to {1}:", Date::ToString(start).c_str(), Date::ToString(end).c_str()) << endl;
    for (Date_ date = start; date <= end; ++date) {
        cout << std::format("{0} (DoW: {1})", Date::ToString(date).c_str(), Date::DayOfWeek(date)) << endl;
    }
    cout << endl;

    cout << "--- Day Count Conventions ---" << endl;
    Date_ start_date(2025, 1, 15);
    Date_ end_date(2025, 7, 15);
    
    DayBasis_ basis_act365("ACT/365F");
    double dcf_365 = basis_act365(start_date, end_date, nullptr);
    cout << std::format("ACT/365F: {0:.6f} years", dcf_365) << endl;
    
    DayBasis_ basis_act360("ACT/360");
    double dcf_360 = basis_act360(start_date, end_date, nullptr);
    cout << std::format("ACT/360: {0:.6f} years", dcf_360) << endl;
    
    DayBasis_ basis_30360("30/360");
    double dcf_30360 = basis_30360(start_date, end_date, nullptr);
    cout << std::format("30/360: {0:.6f} years", dcf_30360) << endl;
    cout << endl;

    cout << "--- Date Schedule Generation ---" << endl;
    Date_ sch_start(2025, 1, 15);
    Date_ sch_maturity(2025, 12, 15);
    auto tenor = Date::ParseIncrement("3M");
    
    Schedule_ schedule = DateGenerate(sch_start, sch_maturity, tenor, DateGeneration_("Forward"));
    cout << std::format("Quarterly schedule ({0} periods):", schedule.size() - 1) << endl;
    for (const auto& sch_date : schedule) {
        cout << std::format("{0}", Date::ToString(sch_date).c_str()) << endl;
    }
    cout << endl;

    cout << "--- Excel Date Conversion ---" << endl;
    int excel_serial = 45992;  // A date in 2025
    Date_ from_excel = Date::FromExcel(excel_serial);
    cout << std::format("Excel serial {0} = {1}", excel_serial, Date::ToString(from_excel).c_str()) << endl;
    cout << std::format("Back to Excel: {0}", Date::ToExcel(from_excel)) << endl;
    cout << endl;

    cout << "--- Date Validation ---" << endl;
    Date_ valid_date(2025, 2, 28);
    Date_ invalid_date;
    cout << std::format("Date {0} is valid: {1}", Date::ToString(valid_date).c_str(), valid_date.IsValid() ? "Yes" : "No") << endl;
    cout << std::format("Empty date is valid: {0}", invalid_date.IsValid() ? "Yes" : "No") << endl;
    cout << endl;

    cout << "--- Date Period Calculation ---" << endl;
    Date_ issue_date(2025, 1, 15);
    Date_ maturity_date(2025, 12, 15);
    Date_ current_date(2025, 6, 30);
    
    int total_days = maturity_date - issue_date;
    int days_to_maturity = maturity_date - current_date;
    int days_elapsed = current_date - issue_date;
    
    cout << std::format("Bond issue date: {0}", Date::ToString(issue_date).c_str()) << endl;
    cout << std::format("Bond maturity date: {0}", Date::ToString(maturity_date).c_str()) << endl;
    cout << std::format("Current date: {0}", Date::ToString(current_date).c_str()) << endl;
    cout << std::format("Total bond term: {0} days", total_days) << endl;
    cout << std::format("Days to maturity: {0} days", days_to_maturity) << endl;
    cout << std::format("Days elapsed: {0} days", days_elapsed) << endl;
    cout << std::format("Percentage completed: {0:.2f}%", (days_elapsed * 100.0 / total_days)) << endl;
    
    // Check if date is in range
    bool in_range = current_date >= issue_date && current_date <= maturity_date;
    cout << std::format("Current date is within bond life: {0}", in_range ? "Yes" : "No") << endl;

    return 0;
}