#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF---------------------------------------------------------
enumeration DayBasis
    Daycounts for accrued interest
extensible
alternative ACT_365F ACT/365F ACT_365FIXED ACT/365FIXED
    Always uses a 365-day year
alternative ACT_365L ACT/365L ISMA_Year
    Sometimes uses a 366-day year
alternative ACT_360 ACT/360 MONEY ACTUAL/360
alternative ACT_ACT ACT/ACT ACTUAL/ACTUAL
alternative BOND 30_360 30/360 BOND_BASIS
    30/360 Bond Basis, also known as ISDA 30/360
alternative THIRTY_360_US 30_360_US 30U/360
    30/360 US with the end-of-February rules
method double operator()(const Date_& start_date, const Date_& end_date, const DayBasis::Context_* context) const;
-IF---------------------------------------------------------*/

namespace Dal {

    class Date_;

    namespace DayBasis {
        struct Context_ {
            bool isLast_;
            Date_ nominalStart_;
            Date_ nominalEnd_;
            int couponMonths_;

            Context_(bool isLast, Date_ nominalStart, Date_ nominalEnd, int couponMonths)
            : isLast_(isLast), nominalStart_(nominalStart), nominalEnd_(nominalEnd), couponMonths_(couponMonths) {}
        };
    } // namespace DayBasis

#include <dal/auto/MG_DayBasis_enum.hpp>

    namespace DayBasis {
        // Canonical ACT/365F basis -- the libor basis assumed by curve calibration and pricing blocks.
        [[nodiscard]] inline DayBasis_ Act365F() { return DayBasis_("ACT_365F"); }
    } // namespace DayBasis
} // namespace Dal
