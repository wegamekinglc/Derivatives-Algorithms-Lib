//
// Created by wegam on 2022/1/29.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration PeriodLength
        Standardized time intervals
alternative ANNUAL 12M
alternative SEMIANNUAL SEMI 6M
alternative QUARTERLY 3M
alternative MONTHLY 1M
method int Months() const;
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_PeriodLength_enum.hpp>

    class Date_;
    class Ccy_;
    namespace Date {
        Date_ NominalMaturity(const Date_& start, const PeriodLength_& step, const Ccy_& ccy);
    }
} // namespace Dal