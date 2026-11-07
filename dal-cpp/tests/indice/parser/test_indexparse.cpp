//
// Created by wegam on 2026/7/19.
//

#include <dal/indice/index.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <gtest/gtest.h>

using namespace Dal;

TEST(IndexParseTest, TestParseDispatchesEquityForms) {
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]"));
    }
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]@2022-01-24")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]@2022-01-24"));
    }
    {
        std::unique_ptr<Index_> index(Index::Parse(String_("EQ[IBM]>3M")));
        ASSERT_EQ(index->Name(), String_("EQ[IBM]>3M"));
    }
}

TEST(IndexParseTest, TestParseDispatchesFxForm) {
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[EUR/GBP]")));
    ASSERT_EQ(index->Name(), String_("FX[EUR/GBP]"));
}

TEST(IndexParseTest, TestParseRejectsUnknownPrefix) {
    ASSERT_THROW((void)Index::Parse(String_("XX[ABC]")), Dal::Exception_);
    ASSERT_THROW((void)Index::Parse(String_("IR:USD,3M")), Dal::Exception_);
}

TEST(IndexParseTest, TestParseRejectsBareNameAndNullParser) {
    ASSERT_THROW(Index::Parse("IBM"), Dal::Exception_);
    ASSERT_THROW(Index::Parse(""), Dal::Exception_);
    Index::RegisterParser("NULL_TEST", [](const String_&) -> std::unique_ptr<Index_> { return nullptr; });
    ASSERT_THROW(Index::Parse("NULL_TEST[ABC]"), Dal::Exception_);
}

TEST(IndexParseTest, TestParseRejectsMalformedAndTrailingInput) {
    for (const auto* text : {"EQ[]", "EQ[AAPL", "EQ[[AAPL]]", "EQ[AAPL]]", "EQ[AAPL]junk", "EQ[AAPL]junk>3M", "EQ[AAPL]@2026-12-31junk", "EQ[AAPL]>",
                             "EQ[AAPL]>3Mjunk", "EQ[AAPL]@", "EQ[\"AAPL\"]", "FX[EUR/USD]junk", "FX[[EUR/USD]]", "FX[EUR/USD]]", "FX[/USD]",
                             "FX[EUR/]", "FX[EUR/USD/JPY]", "FX[EUR/USD]>3M"}) {
        SCOPED_TRACE(text);
        ASSERT_THROW(Index::Parse(text), Dal::Exception_);
    }
}

TEST(IndexParseTest, TestCloneRoundTripsThroughName) {
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[USD/JPY]")));
    const Handle_<Index_> cloned = Index::Clone(*index);
    ASSERT_FALSE(cloned.IsEmpty());
    ASSERT_EQ(cloned->Name(), String_("FX[USD/JPY]"));
}

TEST(IndexParseTest, TestOvernightLiteralCanonicalAndCloneRoundTrip) {
    const String_ name = "IR[USD,ON,USD-SOFR,USD.SOFR,2024-03-02,2024-03-04,2024-03-01,2024-03-04,ACT_360]";
    const std::unique_ptr<Index_> index(Index::Parse(name));
    ASSERT_EQ(index->Name(), name);
    const auto clone = Index::Clone(*index);
    ASSERT_EQ(clone->Name(), name);
}

TEST(IndexParseTest, TestOvernightTypedConstructorRejectsUnsupportedCurrency) {
    for (const auto& currency : {Ccy_(), Ccy_(static_cast<Ccy_::Value_>(-2))}) {
        std::exception_ptr failure;
        {
            const String_ fixing = "USD-SOFR", route = "USD.SOFR";
            try {
                const Index::Overnight_ index(currency, fixing, route, Date_(2024, 3, 1), Date_(2024, 3, 4), Date_(2024, 3, 1), Date_(2024, 3, 4),
                                              DayBasis_("ACT_360"));
                FAIL() << "Unsupported currency accepted at typed construction";
            } catch (const Exception_&) {
                failure = std::current_exception();
            }
        }
        ASSERT_TRUE(failure);
        try {
            std::rethrow_exception(failure);
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("InvalidIndex"), std::string::npos);
        }
    }
}

TEST(IndexParseTest, TestOvernightTypedCurrencyNameAndClone) {
    for (const auto& currency : CcyListAll()) {
        const Index::Overnight_ index(currency, "fixing", "route", Date_(2024, 3, 1), Date_(2024, 3, 4), Date_(2024, 3, 1), Date_(2024, 3, 4),
                                      DayBasis_("ACT_365F"));
        const auto clone = Index::Clone(index);
        ASSERT_EQ(clone->Name(), index.Name());
        ASSERT_EQ(index.Name(), "IR[" + String_(currency.String()) + ",ON,fixing,route,2024-03-01,2024-03-04,2024-03-01,2024-03-04,ACT_365F]");
    }
}

TEST(IndexParseTest, TestOvernightRejectsInvalidIntervalsAndBasis) {
    for (const auto* text : {"IR[USD,ON,SOFR,route,2024-03-04,2024-03-01,2024-03-01,2024-03-04,ACT_360]",
                             "IR[USD,ON,SOFR,route,2024-02-29,2024-03-04,2024-03-01,2024-03-04,ACT_360]",
                             "IR[USD,ON,SOFR,route,2024-03-01,2024-03-05,2024-03-01,2024-03-04,ACT_360]",
                             "IR[USD,ON,SOFR,route,2024-03-01,2024-03-04,2024-03-01,2024-03-04,ACT_ACT]"}) {
        SCOPED_TRACE(text);
        ASSERT_THROW(Index::Parse(text), Exception_);
    }
}

TEST(IndexParseTest, TestOvernightEscapedIdsPreserveExactBytes) {
    const Index::Overnight_ index(Ccy_("USD"), "Mixed,Fix%键", "route] x", Date_(2024, 3, 1), Date_(2024, 3, 4), Date_(2024, 3, 1), Date_(2024, 3, 4),
                                  DayBasis_("ACT_360"));
    const auto clone = Index::Clone(index);
    const auto* on = dynamic_cast<const Index::Overnight_*>(clone.get());
    ASSERT_NE(on, nullptr);
    ASSERT_EQ(std::string(on->fixingKey_.c_str()), "Mixed,Fix%键");
    ASSERT_EQ(std::string(on->route_.c_str()), "route] x");
    ASSERT_EQ(on->Name(), index.Name());
}

TEST(IndexParseTest, TestIrDiscountWithExplicitStartPreservesDateOrder) {
    const DateTime_ fixing(Date_(2026, 9, 28));
    for (const String_& name : {String_("IR[DF]:USD,2027-09-28,2028-09-28"), String_("IR[USD,DF,2028-09-28,2027-09-28]")}) {
        const std::unique_ptr<Index_> index(Index::Parse(name));
        const auto* discount = dynamic_cast<const Index::DF_*>(index.get());
        ASSERT_NE(discount, nullptr);
        ASSERT_EQ(discount->StartDate(fixing), Date_(2027, 9, 28));
        ASSERT_EQ(discount->Maturity(fixing), Date_(2028, 9, 28));

        const Handle_<Index_> cloned = Index::Clone(*index);
        const auto* clonedDiscount = dynamic_cast<const Index::DF_*>(cloned.get());
        ASSERT_NE(clonedDiscount, nullptr);
        ASSERT_EQ(clonedDiscount->StartDate(fixing), Date_(2027, 9, 28));
        ASSERT_EQ(clonedDiscount->Maturity(fixing), Date_(2028, 9, 28));
    }
}

TEST(IndexParseTest, TestMonthTenorSwapAliasRoundTripsThroughName) {
    const std::unique_ptr<Index_> index(Index::Parse("IR[USD,SWAP,18M]"));
    const auto* swap = dynamic_cast<const Index::Swap_*>(index.get());
    ASSERT_NE(swap, nullptr);
    ASSERT_EQ(swap->tenor_, "18M");

    const Handle_<Index_> cloned = Index::Clone(*index);
    const auto* clonedSwap = dynamic_cast<const Index::Swap_*>(cloned.get());
    ASSERT_NE(clonedSwap, nullptr);
    ASSERT_EQ(clonedSwap->tenor_, "18M");
}

TEST(IndexParseTest, TestRegisterParserAddsNewPrefix) {
    Index::RegisterParser(String_("TST"),
                          [](const String_&) -> std::unique_ptr<Index_> { return std::make_unique<Index::Fx_>(Ccy_("USD"), Ccy_("EUR")); });
    std::unique_ptr<Index_> index(Index::Parse(String_("TST[ANYTHING]")));
    ASSERT_EQ(index->Name(), String_("FX[EUR/USD]"));
}

TEST(IndexParseTest, TestRegisterParserKeepsFirstRegistration) {
    // std::map::insert does not overwrite, so re-registering an existing prefix is a no-op
    Index::RegisterParser(String_("FX"),
                          [](const String_&) -> std::unique_ptr<Index_> { return std::make_unique<Index::Fx_>(Ccy_("USD"), Ccy_("EUR")); });
    std::unique_ptr<Index_> index(Index::Parse(String_("FX[USD/JPY]")));
    ASSERT_EQ(index->Name(), String_("FX[USD/JPY]"));
}
