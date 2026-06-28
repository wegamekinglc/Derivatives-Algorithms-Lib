//
// Created by Copilot on 2026/5/7.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/protocol/payment.hpp>

using namespace Dal;

TEST(ProtocolTest, TestPaymentConditionsDefault) {
    Payment::Conditions_ conditions;

    ASSERT_EQ(conditions.exerciseCondition_, ExerciseCondition_::Value_::UNCONDITIONAL);
}

TEST(ProtocolTest, TestPaymentInfoStoresOptionalAccrualPeriod) {
    AccrualPeriod_ accrual(Date_(2024, 1, 1), Date_(2024, 4, 1), 1.0, DayBasis_("ACT/365F"));
    Payment::Conditions_ conditions;
    Payment::Info_ info("coupon", DateTime_(Date_(2024, 1, 15), 0.5), conditions, &accrual);
    Payment::Info_ infoWithoutAccrual("fee");

    ASSERT_EQ(info.description_, String_("coupon"));
    ASSERT_EQ(info.knownTime_, DateTime_(Date_(2024, 1, 15), 0.5));
    ASSERT_EQ(info.conditions_.exerciseCondition_, ExerciseCondition_::Value_::UNCONDITIONAL);
    ASSERT_TRUE(info.period_.has_value());
    ASSERT_EQ(info.period_->startDate_, accrual.startDate_);
    ASSERT_EQ(info.period_->endDate_, accrual.endDate_);
    ASSERT_DOUBLE_EQ(info.period_->dcf_, accrual.dcf_);
    ASSERT_FALSE(infoWithoutAccrual.period_.has_value());
}

TEST(ProtocolTest, TestPaymentConstructorAndNullTag) {
    const auto& nullTag = Payment::Null();
    const auto& sameNullTag = Payment::Null();
    Payment::Info_ info("principal", DateTime_(Date_(2024, 2, 1), 0.25));
    Payment_ payment(DateTime_(Date_(2024, 1, 10), 0.5), Ccy_("USD"), Date_(2024, 2, 1), "main", info, Date_(2024, 1, 31));
    Payment_ defaultCommit(DateTime_(Date_(2024, 1, 10), 0.5), Ccy_("USD"), Date_(2024, 2, 1), "main", info);

    ASSERT_TRUE(nullTag.IsEmpty());
    ASSERT_EQ(nullTag.get(), sameNullTag.get());
    ASSERT_EQ(payment.eventTime_, DateTime_(Date_(2024, 1, 10), 0.5));
    ASSERT_EQ(payment.ccy_, Ccy_("USD"));
    ASSERT_EQ(payment.date_, Date_(2024, 2, 1));
    ASSERT_EQ(payment.stream_, String_("main"));
    ASSERT_EQ(payment.tag_.description_, String_("principal"));
    ASSERT_EQ(payment.commitDate_, Date_(2024, 1, 31));
    ASSERT_EQ(defaultCommit.commitDate_, Date::Minimum());
}
