//
// Created by wegam on 2020/12/5.
//

#include <iostream>
#include <format>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidays.hpp>

using namespace Dal;
using namespace std;

int main() {
    Dal::RegisterAll_::Init();

    Date_ d(2025, 12, 6);
    cout << std::format("This date: {0}", Date::ToString(d).c_str()) << endl;

    Holidays_ hol("CN.SSE CN.IB");
    Date_ next_d = Holidays::NextBus(hol, d);
    cout << std::format("Next {0} business date: {1}", hol.String().c_str(), Date::ToString(next_d).c_str()) << endl;

    Date_ prev_d = Holidays::PrevBus(hol, d);
    cout << std::format("Prev {0} business date: {1}", hol.String().c_str(), Date::ToString(prev_d).c_str()) << endl;

    CountBusDays_ counter(hol);
    cout << std::format("Business days to year end: {0}", counter(d, Date_(2025, 12, 31)));

    return 0;
}