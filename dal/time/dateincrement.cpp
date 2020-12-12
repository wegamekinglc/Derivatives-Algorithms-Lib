#include <dal/platform/platform.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/holidays.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/composite.hpp>

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

    #include <dal/auto/MG_DateStepSize_enum.hpp>
    #include <dal/auto/MG_DateStepSize_enum.inc>

    Date_ DateStepSize_::operator()(const Date_& from, bool forward, int n_steps, const Holidays_& holidays) const {
        int sign = forward ? 1 : -1;
        Date_ ret_val = from;
        switch (val_) {
        default:
            THROW("Invalid DateStepSize");
        case Value_::Y:
            ret_val = Date::AddMonths(ret_val, 12 * n_steps * sign);
            break;
        case Value_::M:
            ret_val = Date::AddMonths(ret_val, n_steps * sign);
            break;
        case Value_::CD:
            ret_val = ret_val.AddDays(n_steps * sign);
            break;
        case Value_::BD:
            while (--n_steps >= 0) {
                ret_val = forward ? Holidays::NextBus(holidays, ret_val.AddDays(1))
                                  : Holidays::PrevBus(holidays, ret_val.AddDays(-1));
            }
        }
        if (holidays != Holidays::None())
            return forward ? Holidays::NextBus(holidays, ret_val) : Holidays::PrevBus(holidays, ret_val);
        else
            return ret_val;
    }

    class IncrementMultistep_ : public Date::Increment_ {
        int nSteps_;
        DateStepSize_ stepBy_;
        Holidays_ hols_;

        Date_ FwdFrom(const Date_& d) const {
            return stepBy_(d, true, nSteps_, hols_);
        }

        Date_ BackFrom(const Date_& d) const {
            return stepBy_(d, false, nSteps_, hols_);
        }

    public:
        IncrementMultistep_(int n, const DateStepSize_& d, const Holidays_& h)
        :nSteps_(n), stepBy_(d), hols_(h) {}
    };

    class IncrementCompound_: public Composite_<const Date::Increment_> {
    public:
        template <class F_>
        Date_ Apply(const Date_& date, const F_& func) const {
            Date_ new_date = date;
            for (const auto& inc : contents_)
                new_date = (inc.get()->*func)(new_date);
            return new_date;
        }

        Date_ FwdFrom(const Date_& d) const {
            return Apply(d, &Increment_::FwdFrom);
        }

        Date_ BackFrom(const Date_& d) const {
            return Apply(d, &Increment_::BackFrom);
        }
    };
}

namespace Dal {
    Handle_<Date::Increment_> Date::ToIMM(bool monthly) {
        static const Handle_<Date::Increment_> QUARTERLY(new IncrementNextSpecial_(SpecialDay_("IMM")));
        static const Handle_<Date::Increment_> MONTHLY(new IncrementNextSpecial_(SpecialDay_("IMM_MONTHLY")));
        return monthly ? MONTHLY : QUARTERLY;
    }

    Handle_<Date::Increment_> Date::ParseIncrement(const String_& src) {
        NOTICE(src);
        if (src.find('&') != String_::npos) {
            const auto subs = String::Split(src, '&', false);
            std::unique_ptr<IncrementCompound_> ret_val(new IncrementCompound_);
            for (const auto& s : subs)
                ret_val->Append(ParseIncrement(s));
            return Handle_<Date::Increment_>(ret_val.release());
        }

        // Ok, it's a single increment
        REQUIRE(!src.empty(), "Increment should not be empty");
        auto nonNumeric = src.find_first_not_of("0123456789");
        if (nonNumeric == 0)
            return Handle_<Date::Increment_>(new IncrementNextSpecial_(SpecialDay_(src)));

        auto sc = src.find(';');
        Holidays_ hols(Holidays::None());
        if (sc != String_::npos)
            hols = Holidays_(src.substr(sc + 1));
        const auto n = String::ToInt(src.substr(0, nonNumeric));
        DateStepSize_ step(src.substr(nonNumeric, sc - nonNumeric));
        return Handle_<Date::Increment_>(new IncrementMultistep_(n, step, hols));
    }

    bool IsLiborTenor(const String_& tenor) {
        return tenor.find_first_of("yY") == String_::npos;
    }

    bool IsSwapTenor(const String_& tenor) {
        return !IsLiborTenor(tenor);
    }
}