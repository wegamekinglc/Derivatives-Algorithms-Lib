//
// Created by wegam on 2022/6/12.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/dateutils.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

TEST(GlobalsTest, TestSetAccountingDate) {
    Date_ dt(2022, 6, 11);
    Global::Dates_().SetAccountingDate(dt);
    Date_ gdt = Global::Dates_().AccountingDate();
    ASSERT_EQ(gdt, dt);
}

TEST(GlobalsTest, TestSetEvaluationDate) {
    Date_ dt(2022, 6, 10);
    Global::Dates_().SetEvaluationDate(dt);
    Date_ gdt = Global::Dates_().EvaluationDate();
    ASSERT_EQ(gdt, dt);
}

TEST(GlobalsTest, TestSetAccountingDateInScope) {
    Date_ dt(2022, 6, 9);
    Global::Dates_().SetAccountingDate(Date::Today());
    {
        auto ret(XGLOBAL::SetAccountingDateInScope(dt));
        Date_ gdt = Global::Dates_().AccountingDate();
        ASSERT_EQ(gdt, dt);
    }
    Date_ gdt = Global::Dates_().AccountingDate();
    ASSERT_EQ(gdt, Date::Today());
}

TEST(GlobalsTest, TestSetEvaluationDateInScope) {
    Date_ dt(2022, 6, 8);
    Global::Dates_().SetEvaluationDate(Date::Today());
    {
        auto ret(XGLOBAL::SetEvaluationDateInScope(dt));
        Date_ gdt = Global::Dates_().EvaluationDate();
        ASSERT_EQ(gdt, dt);
    }
    Date_ gdt = Global::Dates_().EvaluationDate();
    ASSERT_EQ(gdt, Date::Today());
}