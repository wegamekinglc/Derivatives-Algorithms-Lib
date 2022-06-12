//
// Created by wegam on 2022/6/12.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

TEST(GlobalsTest, TestSetAccountingDate) {
    Date_ dt(2022, 6, 11);
    XGLOBAL::SetAccountingDate(dt);
    Date_ gdt = Global::Dates_().AccountingDate();
    ASSERT_EQ(gdt, dt);
}

TEST(GlobalsTest, TestSetEvaluationDate) {
    Date_ dt(2022, 6, 10);
    XGLOBAL::SetEvaluationDate(dt);
    Date_ gdt = Global::Dates_().EvaluationDate();
    ASSERT_EQ(gdt, dt);
}

TEST(GlobalsTest, TestSetAccountingDateInScope) {
    Date_ dt(2022, 6, 9);
    auto ret(XGLOBAL::SetAccountingDateInScope(dt));
    Date_ gdt = Global::Dates_().AccountingDate();
    ASSERT_EQ(gdt, dt);
}

TEST(GlobalsTest, TestSetEvaluationDateInScope) {
    Date_ dt(2022, 6, 8);
    auto ret(XGLOBAL::SetEvaluationDateInScope(dt));
    Date_ gdt = Global::Dates_().EvaluationDate();
    ASSERT_EQ(gdt, dt);
}