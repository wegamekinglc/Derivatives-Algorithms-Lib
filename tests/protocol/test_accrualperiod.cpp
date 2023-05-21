//
// Created by wegam on 2023/5/21.
//

#include <gtest/gtest.h>
#include <dal/protocol/accrualperiod.hpp>

using namespace Dal;

TEST(ProtocolTest, TestAccrualPeriod) {
    Date_ start(2023, 5, 21);
    Date_ end(2023, 8, 21);
    AccrualPeriod_ ap(start, end, 1.0, DayBasis_("Act_365F"));

    Handle_<DayBasis::Context_> ctx(new DayBasis::Context_(false, start, end, 12));
    AccrualPeriod_ ap2(start, end, 1.0, DayBasis_("Act_365F"), ctx, false);
}