//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/preparation.hpp>

using namespace Dal;

TEST(FixingEnvironmentTest, TestRawProjectionPreservesVirtualInverse) {
    const Date_ d = Script::CaptureScriptEvaluationDate();
    const DateTime_ h(d.AddDays(-1), 0.0);
    const DateTime_ future(d.AddDays(1), 0.0);
    const Handle_<MarketFixingSnapshot_> snapshot(
        new MarketFixingSnapshot_({{"FX[USD/EUR]", {{h, 0.8}, {future, 0.5}}}, {"EQ[UNRELATED]", {{h, 99.0}}}}));
    const auto environment = SnapshotFixingEnvironment(*snapshot, {{"FX[EUR/USD]", h}});
    ASSERT_TRUE(environment);
    const auto* access = Environment::Find<FixingsAccess_>(environment.get());
    ASSERT_NE(access, nullptr);
    ASSERT_EQ(access->fixings_.size(), 1);
    ASSERT_FALSE(access->Fetch("FX[EUR/USD]"));
    ASSERT_FALSE(access->Fetch("EQ[UNRELATED]"));
    const auto inverse = access->Fetch("FX[USD/EUR]");
    ASSERT_TRUE(inverse);
    ASSERT_EQ(inverse->vals_.size(), 1);
    ASSERT_DOUBLE_EQ(inverse->vals_.at(h), 0.8);
    const Handle_<Index_> index(Index::Parse("FX[EUR/USD]"));
    ASSERT_NEAR(index->Fixing(environment.get(), h), 1.25, 1.0e-12);

    struct CheckVirtual_ : Dal::Detail::FixingReadObserver_ {
        size_t calls_ = 0;
        void BeforeFixing(const Index_& index, const Environment_* environment, const DateTime_&) override {
            ++calls_;
            ASSERT_NE(dynamic_cast<const Index::Fx_*>(&index), nullptr);
            const auto* access = Environment::Find<FixingsAccess_>(environment);
            ASSERT_NE(access, nullptr);
            ASSERT_FALSE(access->Fetch("FX[EUR/USD]"));
            ASSERT_TRUE(access->Fetch("FX[USD/EUR]"));
        }
    } observer;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&observer);
    const Script::ScriptProductData_ data("", {Cell_(d.AddDays(1))}, {"payoff PAYS FIX(FX[EUR/USD], " + Date::ToString(d.AddDays(-1)) + ")"});
    const auto prepared = Script::PrepareScript(data, {}, snapshot);
    ASSERT_EQ(observer.calls_, 1);
    ASSERT_NEAR(prepared.Plan().KnownValue(0), 1.25, 1.0e-12);
}

TEST(FixingEnvironmentTest, TestEmptyEnvironmentStillHasFixingAccess) {
    const DateTime_ h(Date_(2026, 9, 11), 0.0);
    const auto environment = SnapshotFixingEnvironment(MarketFixingSnapshot_(), {{"FX[EUR/USD]", h}});
    ASSERT_TRUE(environment);
    const auto* access = Environment::Find<FixingsAccess_>(environment.get());
    ASSERT_NE(access, nullptr);
    ASSERT_TRUE(access->fixings_.empty());
    const Handle_<Index_> index(Index::Parse("FX[EUR/USD]"));
    ASSERT_THROW(index->Fixing(environment.get(), h), Exception_);
}

TEST(FixingEnvironmentTest, TestHistoricalWhitelistAndNullParser) {
    struct Unsupported_ : Index_ {
        String_ Name() const override { return "DAL199_CUSTOM[X]"; }
        double Fixing(const Environment_*, const DateTime_&) const override { THROW("must not invoke a third-party index"); }
    };
    Index::RegisterParser("DAL199_CUSTOM", [](const String_&) -> std::unique_ptr<Index_> { return std::make_unique<Unsupported_>(); });
    Index::RegisterParser("DAL199_NULL", [](const String_&) -> std::unique_ptr<Index_> { return {}; });
    const Date_ d = Script::CaptureScriptEvaluationDate();
    for (const auto* index : {"DAL199_CUSTOM[X]", "DAL199_NULL[X]"}) {
        for (const auto eventDate : {d.AddDays(-1), d.AddDays(1)}) {
            const Script::ScriptProductData_ data("", {Cell_(eventDate)},
                                                  {"payoff PAYS FIX(" + String_(index) + ", " + Date::ToString(d.AddDays(-1)) + ")"});
            ASSERT_THROW(Script::PrepareScript(data), ScriptError_);
        }
    }
}
