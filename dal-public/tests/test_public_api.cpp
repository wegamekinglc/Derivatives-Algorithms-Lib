//
// Created by wegam on 2026/5/30.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

#include <cmath>

using Dal::Date_;

TEST(PublicApiTest, TestEvaluationDateRoundTrip) {
    // Set a known date and read it back
    Date_ d(2026, 5, 30);
    Dal::SetEvaluationDate(d);
    Date_ result = Dal::GetEvaluationDate();

    // Use free functions Year(), Month(), Day() for Date_ access
    ASSERT_EQ(Dal::Date::Year(d), Dal::Date::Year(result));
    ASSERT_EQ(Dal::Date::Month(d), Dal::Date::Month(result));
    ASSERT_EQ(Dal::Date::Day(d), Dal::Date::Day(result));
}

TEST(PublicApiTest, TestPublicHeaderIncludeLinks) {
    // This test simply verifies the public types compile and link.
    // If we get here, the test binary linked against dal_public successfully.
    ASSERT_TRUE(true);
}

TEST(PublicApiTest, TestMonteCarloRejectsNonPositivePathCounts) {
    const Dal::Handle_<Dal::ScriptProductData_> product;
    const Dal::Handle_<Dal::ModelData_> model;

    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 0), Dal::Exception_);
    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, -1), Dal::Exception_);
}

TEST(PublicApiTest, TestMonteCarloAcceptsOnePath) {
    const Date_ previousEvaluationDate = Dal::GetEvaluationDate();
    Dal::SetEvaluationDate(Date_(2022, 9, 25));

    const Dal::Vector_<Dal::Cell_> dates = {
        Dal::Cell_("STRIKE"),
        Dal::Cell_(Date_(2023, 9, 25)),
    };
    const Dal::Vector_<Dal::String_> events = {
        Dal::String_("100.0"),
        Dal::String_("call pays MAX(spot() - STRIKE, 0.0)"),
    };
    const auto product = Dal::NewScriptProduct(Dal::String_("one_path"), dates, events);
    const auto model = Dal::NewBSModelData(Dal::String_("one_path_model"), 100.0, 0.2, 0.05, 0.02);

    const auto result = Dal::ValueByMonteCarlo(product, model, 1);
    const bool pvIsFinite = std::isfinite(result.at(Dal::String_("PV")));
    Dal::SetEvaluationDate(previousEvaluationDate);

    ASSERT_TRUE(pvIsFinite);
}
