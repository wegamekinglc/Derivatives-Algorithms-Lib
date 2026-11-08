#include <gtest/gtest.h>

#include <type_traits>

#include <dal-public/src/earlyexercisepderisk.hpp>

using namespace Dal;

TEST(PublicEarlyExercisePdeRiskTest, TestAliasesAndOwningEvaluation) {
    static_assert(std::is_same_v<EarlyExercisePdeRiskResult_, PDE::EarlyExercisePdeRiskResult_>);
    EarlyExercisePdeRiskRequest_ request;
    const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
    request.valuation_.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {maturity}};
    request.valuation_.market_ = {100.0, 0.2, {{maturity, 0.05, 0.03}}};
    request.valuation_.settings_.gridPoints_ = 41;
    request.valuation_.settings_.ordinarySteps_ = 2;
    request.valuation_.settings_.rannacherIntervals_ = 0;
    ValidateEarlyExercisePdeRiskSettings(request.risk_);
    ValidateEarlyExercisePdeRiskRequest(request);
    const auto result = EvaluateEarlyExercisePdeRisk(request);
    auto copy = result;
    request.valuation_.market_.spot_ = 50.0;
    const auto moved = std::move(copy);
    ASSERT_TRUE(moved.PriceAvailable());
    ASSERT_DOUBLE_EQ(moved.Price(), result.Price());
    ASSERT_EQ(moved.Request().valuation_.market_.spot_, 100.0);
    ASSERT_EQ(moved.Scenarios().size(), 17);
    ASSERT_EQ(moved.Risks().size(), 5);
    ASSERT_FALSE(moved.AadUsed());
}
