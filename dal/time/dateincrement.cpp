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
}