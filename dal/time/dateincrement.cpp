#include <dal/platform/platform.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace Date {
        Increment_::~Increment_() {}
    }
}

/*IF--------------------------------------------------------------------------
enumeration SpecialDay
    Date families supporting jump-to-next
alternative IMM IMM3 IMM_QUARTERLY
    Quarterly IMM dates
alternative IMM1 IMM_MONTHLY
    Monthly IMM dates
alternative CDS CDS3 CDS_QUARTERLY
    Quarterly CDS standard maturities
alternative EOM
    End of a month
method Date_ Step(const Date_& base, bool forward) const;
-IF-------------------------------------------------------------------------*/

// note that "DAY" and "D" are not defined -- they would be ambiguous
/*IF--------------------------------------------------------------------------
enumeration DateStepSize
    Repeatable step length
alternative Y YEAR YEARS
alternative M MONTH MONTHS
alternative BD BUS_DAY BUSINESS_DAY
alternative CD CAL_DAY CALENDAR_DAY
method Date_ operator() (const Date_& base, bool forward, int n_steps, const Holidays_& holidays) const;
-IF-------------------------------------------------------------------------*/

namespace {
    using namespace Dal;

    #include <dal/auto/MG_SpecialDay_enum.hpp>
    #include <dal/auto/MG_SpecialDay_enum.inc>

    #include <dal/auto/MG_DateStepSize_enum.hpp>
    #include <dal/auto/MG_DateStepSize_enum.inc>

    void MonthInRange(int* y, int* m) {
        while (*m > 12)
            *m -= 12, ++*y;
        while (*m < 1)
            *m += 12, --*y;
    }

    Date_ ToCDS(const Date_& from, bool forward) {
        int yy = Date::Year(from);
        int mm = Date::Month(from);
        int dd = Date::Day(from);

        if (forward ? dd >= 20 : dd <= 20) {
            do {
                mm += forward ? 1 : -1;
            } while (mm % 3);
        }
        MonthInRange(&yy, &mm);
        return Date_(yy, mm, 20);
    }

    int IMMDay(int yy, int mm) {
        return 21 - Date::DayOfWeek(Date_(yy, mm, 18));
    }

    Date_ ToIMM(const Date_& from, bool forward, int imm_months) {
        int yy = Date::Year(from);
        int mm = Date::Month(from);
        int dd = Date::Day(from);

        if (forward ? dd >= IMMDay(yy, mm) : dd <= IMMDay(yy, mm)) {
            do {
                mm += forward ? 1 : -1;
            } while (mm % imm_months);
        }
        MonthInRange(&yy, &mm);
        return Date_(yy, mm, IMMDay(yy, mm));
    }

    Date_ SpecialDay_::Step(const Date_& from, bool forward) const {
        switch (val_) {
        default:
            THROW("Invalid SpecialDay type");
        case Value_::CDS:
            return ToCDS(from, forward);
        case Value_::IMM:
            return ToIMM(from, forward, 3);
        case Value_::IMM1:
            return ToIMM(from, forward, 1);
        case Value_::EOM:
            return forward
                       ? Date::EndOfMonth(from)
                       : Date_(Date::Year(from), Date::Month(from), 1).AddDays(-1);
        }
    }

    class IncrementNextSpecial_ : public Date::Increment_ {
        SpecialDay_ targets_;
        Date_ FwdFrom(const Date_& d) const override {
            return targets_.Step(d, true);
        }
        Date_ BackFrom(const Date_& d) const override {
            return targets_.Step(d, false);
        }
    public:
        IncrementNextSpecial_(const SpecialDay_& d): targets_(d) {};
    };

//    Date_ DateStepSize_::operator()(const Date_& from, bool forward, int n_steps, const Holidays_& holidays) const {
//        int sign = forward ? 1 : -1;
//        Date_ ret_val = from;
//        switch (val_) {
//        default:
//            THROW("Invalid DateStepSize");
//        case Value_::Y:
//            ret_val = Date::AddMonths(ret_val, 12 * n_steps * sign);
//            break;
//        case Value_::M:
//            ret_val = Date::AddMonths(ret_val, n_steps * sign);
//            break;
//        case Value_::CD:
//            ret_val = ret_val.AddDays(n_steps * sign);
//            break;
//        case Value_::BD:
//            while (--n_steps >= 0) {
//                ret_val = forward ? Holidays::NextBus(holidays, ret_val.AddDays(1))
//                                  : Holidays::PrevBus(holidays, ret_val.AddDays(-1));
//            }
//        }
//        return forward ? Holidays::NextBus(holidays, ret_val) : Holidays::PrevBus(holidays, ret_val);
//    }
}