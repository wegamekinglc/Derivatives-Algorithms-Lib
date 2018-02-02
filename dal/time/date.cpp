//
// Created by Cheng Li on 2018/2/2.
//

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/algorithms.hpp>

namespace dal {

    namespace {
        const uint16_t EXCEL_OFFSET = 25568;

        void ExcelDateToYMD(long serial, short *yy, short *mm, short *dd) {
            serial += 2415019;
            const auto alpha = static_cast<int>((serial - 1867216.25) / 36524.25);
            const auto A = serial + 1 + alpha - (alpha / 4);
            const auto B = A + 1524;
            const auto C = static_cast<int>((B - 122.1) / 365.25);
            const auto D = static_cast<int>(365.25 * C);
            const auto E = static_cast<int>((B - D) / 30.6001);
            ASSIGN(dd, static_cast<short>(B - D - static_cast<int>(30.6001 * E)));
            const int m = E - (E > 13 ? 13 : 1);
            ASSIGN(mm, static_cast<short>(m));
            ASSIGN(yy, static_cast<short>(C - (m > 2.5 ? 4716 : 4715)));
        }

        int ExcelDateFromYMD(int yy, int mm, int dd) {
            // based on Vogelpoel's port of now-lost pseudocode
            // ignore Excel bug around 29 Feb 1900 -- dates before then will be wrong
            return (1461 * (yy + (mm - 14) / 12)) / 4 +
                   (367 * (mm - 2 - 12 * ((mm - 14) / 12))) / 12 -
                   (3 * ((yy + 4900 + (mm - 14) / 12)) / 100) / 4 +
                   dd - 693894l;
        }

        uint16_t SerialFromYMD(int yy, int mm, int dd)
        {
            const auto xl = ExcelDateFromYMD(yy, mm, dd);
            const auto retval = static_cast<uint16_t>(xl - EXCEL_OFFSET);
            return static_cast<int>(retval) + EXCEL_OFFSET == xl
                   ? retval
                   : uint16_t(0);		// equality failure means overflow of uint16
        }
    }

    Date_::Date_(int yyyy, int mm, int dd)
    :serial_(SerialFromYMD(yyyy, mm, dd)) {}

    Date_ date::FromExcel(int serial) {
        Date_ ret_val;
        if (serial > EXCEL_OFFSET && serial < EXCEL_OFFSET + (1 << 16))
            ret_val.serial_ = static_cast<uint16_t>(serial - EXCEL_OFFSET);
        // otherwise leave it invalid
        return ret_val;
    }

    int date::ToExcel(const Date_ &dt) {
        return dt.serial_ + EXCEL_OFFSET;
    }

    short date::Year(const Date_ &dt) {
        short yy;
        ExcelDateToYMD(ToExcel(dt), &yy, nullptr, nullptr);
        return yy;
    }

    short date::Month(const Date_ &dt) {
        short mm;
        ExcelDateToYMD(ToExcel(dt), nullptr, &mm, nullptr);
        return mm;
    }

    short date::Day(const Date_ &dt) {
        short dd;
        ExcelDateToYMD(ToExcel(dt), nullptr, nullptr, &dd);
        return dd;
    }

    String_ date::ToString(const Date_ &dt) {
        short yy, mm, dd;
        ExcelDateToYMD(ToExcel(dt), &yy, &mm, &dd);
        String_ ret_val("0000-00-00");
        sprintf(&ret_val[0], "%4d-%2d-%2d", int(yy), int(mm), int(dd));
        if (ret_val[5] == ' ')
            ret_val[5] = '0';
        if (ret_val[8] == ' ')
            ret_val[8] = '0';
        return ret_val;
    }

    short date::DayOfWeek(const Date_ &dt) {
        static const int KNOWN_SUNDAY = 974;
        return static_cast<short>((ToExcel(dt) - KNOWN_SUNDAY) % 7);
    }

    short date::DaysInMonth(int year, int month) {
        return month == 12
               ? static_cast<short>(31)
               : static_cast<short>(Date_(year, month + 1, 1) - Date_(year, month, 1));
    }

    Date_ date::EndOfMonth(const Date_ &dt) {
        const auto yy = Year(dt);
        const auto mm = Month(dt);
        return mm == 12 ? Date_(yy + 1, 1, 1).AddDays(-1) : Date_(yy, mm + 1, 1).AddDays(-1);
    }

    Date_ date::AddMonths(const Date_ &dt, int n_months, bool preserve_eom) {
        int yy = Year(dt);
        int mm = Month(dt);
        auto dd = Day(dt);
        const auto to_eom = preserve_eom && dd == DaysInMonth(yy, mm);
        const auto ny = n_months / 12;
        yy += ny;
        mm += n_months - 12 * ny;
        if (mm > 12)
            mm -= 12, ++yy;
        if (mm < 1)
            mm += 12, --yy;
        const auto d_max = DaysInMonth(yy, mm);
        if (to_eom || dd > d_max)
            dd = d_max;
        return Date_(yy, mm, dd);
    }

    int operator - (const Date_& lhs, const Date_& rhs) {
        return date::ToExcel(lhs) - date::ToExcel(rhs);
    }

    Date_ date::Minimum() {
        return FromExcel(EXCEL_OFFSET + 1);
    }

    Date_ date::Maximum() {
        return Minimum().AddDays(0xFFF0);
    }
}