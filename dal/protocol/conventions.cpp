//
// Created by wegam on 2022/2/2.
//

#include <dal/platform/platform.hpp>
#include <dal/protocol/conventions.hpp>
#include <dal/platform/strict.hpp>

#include <dal/time/datetime.hpp>
#include <dal/time/holidays.hpp>
#include <dal/currency/currencydata.hpp>

namespace Dal {
    Date_ Libor::StartFromFix(const Ccy_& ccy, const Date_& fix_date) {
        const Holidays_& hols = Ccy::Conventions::LiborFixHolidays()(ccy);
        const int nDays = Ccy::Conventions::LiborFixDays()(ccy);
        if (nDays == 0)
            return Holidays::NextBus(hols, fix_date);
        // normal case -- increment, go to business day
        Date_ ret_val = fix_date;
        for (int ii = 0; ii < nDays; ++ii)
            ret_val = Holidays::NextBus(hols, ret_val.AddDays(1));
        return ret_val;
    }

    namespace {
        Date_ FixDateFromStart(const Ccy_& ccy, const Date_& start) {
            const Holidays_& hols = Ccy::Conventions::LiborFixHolidays()(ccy);
            const int nDays = Ccy::Conventions::LiborFixDays()(ccy);
            if (nDays == 0)
                return Holidays::PrevBus(hols, start);
            Date_ ret_val = start;
            for (int ii = 0; ii < nDays; ++ii)
                ret_val = Holidays::PrevBus(hols, ret_val.AddDays(-1));
            return ret_val;
        }
    }	// leave local

    // TODO: POSTPONED:  fix this to be consistent with StartFromFix, and to know correct fixing time
    DateTime_ Libor::FixFromStart(const Ccy_& ccy, const Date_& start_date) {
        return DateTime_(FixDateFromStart(ccy, start_date), 10);
    }
}