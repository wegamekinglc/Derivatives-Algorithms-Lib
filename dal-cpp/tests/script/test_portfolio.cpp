//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <limits>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/script/portfolio.hpp>
#include <dal/storage/json.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;
using namespace Dal::Script::TestSupport;

namespace {
    class UnsupportedPortfolioModel_ : public ModelData_ {
    public:
        UnsupportedPortfolioModel_() : ModelData_("unsupported", "same") {}
        void Write(Archive::Store_&) const override {}

    private:
        std::unique_ptr<ModelData_> MutantModel(const String_*, const Slide_*) const override {
            return std::make_unique<UnsupportedPortfolioModel_>();
        }
    };

    template <class F_> void AssertPortfolioError(const F_& action, const String_& expected) {
        try {
            action();
            FAIL() << "expected " << expected;
        } catch (const ScriptError_& error) {
            ASSERT_NE(String_(error.what()).find(expected), String_::npos) << error.what();
        }
    }
} // namespace

TEST(ScriptPortfolioTest, TestRepeatedModelOwnerAndIndependentSealedSnapshots) {
    auto shared = std::make_shared<BSModelData_>("same", 100.0, 0.2, 0.03, 0.01);
    auto equal = std::make_shared<BSModelData_>("same", 100.0, 0.2, 0.03, 0.01);
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const ScriptPortfolioData_ portfolio(
        "portfolio",
        {{"A", product, Handle_<ModelData_>(shared)}, {"B", product, Handle_<ModelData_>(shared)}, {"C", product, Handle_<ModelData_>(equal)}});
    ASSERT_EQ(portfolio.TradeIds(), Vector_<String_>({"A", "B", "C"}));
    ASSERT_EQ(portfolio.ModelOwners(), Vector_<int>({0, 0, 1}));
    ASSERT_EQ(portfolio.Models().size(), 2);
    ASSERT_NE(portfolio.Models()[0].get(), shared.get());
    ASSERT_NE(portfolio.Models()[1].get(), equal.get());
    ASSERT_NE(portfolio.Models()[0].get(), portfolio.Models()[1].get());
    ASSERT_NE(portfolio.Products()[0].get(), product.get());
    ASSERT_NE(portfolio.Products()[0].get(), portfolio.Products()[1].get());
    shared->spot_ = 150.0;
    equal->vol_ = 0.4;
    ASSERT_DOUBLE_EQ(dynamic_cast<const BSModelData_&>(*portfolio.Models()[0]).spot_, 100.0);
    ASSERT_DOUBLE_EQ(dynamic_cast<const BSModelData_&>(*portfolio.Models()[1]).vol_, 0.2);
    ASSERT_EQ(portfolio.Products()[0]->EventTexts(), Vector_<String_>({"pay PAYS SPOT()"}));
}

TEST(ScriptPortfolioTest, TestInvalidTradeTableFailsWithoutHistoryOrTasks) {
    const Handle_<ModelData_> model(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    RejectFixingReads_ fixingReads;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&fixingReads);
    SubmissionCounter_ submissions;
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&submissions);
    for (const auto& [trades, error] :
         Vector_<std::pair<Vector_<PortfolioTrade_>, String_>>{{{}, "field=trades"},
                                                               {{{"", product, model}}, "field=tradeIds"},
                                                               {{{"A", product, model}, {"a", product, model}}, "field=tradeIds; trade=a"},
                                                               {{{"A", {}, model}}, "field=products; trade=A"},
                                                               {{{"A", product, {}}}, "field=models; trade=A"}}) {
        SCOPED_TRACE(error);
        AssertPortfolioError([&] { static_cast<void>(ScriptPortfolioData_("", trades)); }, error);
    }
    ASSERT_EQ(submissions.submissions_, 0);
}

TEST(ScriptPortfolioTest, TestUnsupportedAndNonfiniteModelsRetainTradeContext) {
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    for (const Handle_<ModelData_>& model :
         Vector_<Handle_<ModelData_>>{Handle_<ModelData_>(new UnsupportedPortfolioModel_),
                                      Handle_<ModelData_>(new BSModelData_("same", std::numeric_limits<double>::quiet_NaN(), 0.2)),
                                      Handle_<ModelData_>(new BSModelData_("same", 100.0, std::numeric_limits<double>::infinity()))}) {
        AssertPortfolioError([&] { static_cast<void>(ScriptPortfolioData_("", {{"offending", product, model}})); },
                             "field=models; trade=offending; cause=");
    }
}

TEST(ScriptPortfolioTest, TestArchivePreservesOwnershipAndSealedValues) {
    const Handle_<ModelData_> shared(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ModelData_> equal(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const ScriptPortfolioData_ portfolio("archive", {{"A", product, shared}, {"B", product, shared}, {"C", product, equal}});
    const String_ serialized = JSON::WriteString(portfolio);
    const auto restored = handle_cast<ScriptPortfolioData_>(JSON::ReadString(serialized, true));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->ModelOwners(), Vector_<int>({0, 0, 1}));
    ASSERT_EQ(restored->TradeIds(), portfolio.TradeIds());
    ASSERT_EQ(restored->Models().size(), 2);
    ASSERT_NE(restored->Models()[0].get(), restored->Models()[1].get());
    ASSERT_NE(restored->Models()[0].get(), portfolio.Models()[0].get());
    ASSERT_EQ(JSON::WriteString(*restored), serialized);
}

TEST(ScriptPortfolioTest, TestArchiveRejectsMalformedOwnerOrdinals) {
    const Handle_<ModelData_> shared(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ModelData_> equal(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
    const ScriptPortfolioData_ portfolio("archive", {{"A", product, shared}, {"B", product, shared}, {"C", product, equal}});
    const String_ serialized = JSON::WriteString(portfolio);
    const size_t field = serialized.find("\"modelOwners\"");
    ASSERT_NE(field, String_::npos);
    const size_t begin = serialized.find('[', field);
    const size_t end = serialized.find(']', begin);
    ASSERT_NE(begin, String_::npos);
    ASSERT_NE(end, String_::npos);
    for (const String_& invalid : Vector_<String_>{"[-1,0,1]", "[1,0,1]", "[0,0,0]", "[0,0]", "[0,0,2]", "[0,0,1.5]", "[0,0,true]"}) {
        SCOPED_TRACE(invalid);
        String_ mutated = serialized;
        mutated.replace(begin, end - begin + 1, invalid);
        ASSERT_THROW(static_cast<void>(JSON::ReadString(mutated, true)), Exception_);
    }
}
