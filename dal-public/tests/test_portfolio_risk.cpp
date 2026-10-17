//
// Created by Codex on 2026/10/6.
//

#include <gtest/gtest.h>

#include <dal-public/src/models.hpp>
#include <dal-public/src/portfoliorisk.hpp>
#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/json.hpp>

#include <script_test_observers.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(PortfolioRiskAxesTest, TestSharedModelAndPrivateSameNameConstantsHaveStableAxes) {
    const Handle_<ModelData_> model(new BSModelData_("same", 100.0, 0.2, 0.03, 0.01));
    const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2027, 1, 1))};
    const Handle_<ScriptProductData_> a(new ScriptProductData_("same", dates, {"5", "y = 2 * SPOT() + X pay PAYS y"}));
    const Handle_<ScriptProductData_> b(new ScriptProductData_("same", dates, {"7", "y = 3 * SPOT() + X pay PAYS y"}));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", a, model}, {"B", b, model}}));
    const auto axes = ScriptPortfolioRiskAxes(portfolio);
    ASSERT_EQ(axes.InputAxis().size(), 6);
    ASSERT_EQ(axes.InputAxis()[0].id_, "model:0:parameter:0");
    ASSERT_DOUBLE_EQ(axes.InputAxis()[0].value_, 100.0);
    ASSERT_EQ(axes.InputAxis()[1].physicalUnit_, std::optional<String_>("year^-1/2"));
    ASSERT_EQ(axes.InputAxis()[4].id_, "trade:0:constant:0");
    ASSERT_EQ(axes.InputAxis()[5].id_, "trade:1:constant:0");
    ASSERT_EQ(axes.InputAxis()[4].label_, axes.InputAxis()[5].label_);
    ASSERT_DOUBLE_EQ(axes.InputAxis()[4].value_, 5.0);
    ASSERT_DOUBLE_EQ(axes.InputAxis()[5].value_, 7.0);
    ASSERT_EQ(axes.TradeInputPositions()[0], Vector_<size_t>({0, 1, 2, 3, 4}));
    ASSERT_EQ(axes.TradeInputPositions()[1], Vector_<size_t>({0, 1, 2, 3, 5}));
    ASSERT_EQ(axes.OutputAxis().size(), 4);
    ASSERT_EQ(axes.OutputAxis()[0].id_, "trade:0:output:0");
    ASSERT_EQ(axes.OutputAxis()[1].id_, "trade:0:payoff");
    ASSERT_EQ(axes.OutputAxis()[2].id_, "trade:1:output:0");
    ASSERT_EQ(axes.OutputAxis()[3].id_, "trade:1:payoff");
    ASSERT_EQ(axes.OutputTrades(), Vector_<size_t>({0, 0, 1, 1}));
}

TEST(PortfolioRiskAxesTest, TestDistinctEqualModelOwnersAndConstantsRemainIndependent) {
    const Handle_<ModelData_> first(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ModelData_> equal(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("same", {Cell_("X"), Cell_(Date_(2027, 1, 1))}, {"5", "pay PAYS X * SPOT()"}));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, first}, {"B", product, equal}}));
    const auto axes = ScriptPortfolioRiskAxes(portfolio);
    ASSERT_EQ(axes.InputAxis().size(), 10);
    ASSERT_EQ(axes.InputAxis()[0].id_, "model:0:parameter:0");
    ASSERT_EQ(axes.InputAxis()[4].id_, "model:1:parameter:0");
    ASSERT_DOUBLE_EQ(axes.InputAxis()[0].value_, axes.InputAxis()[4].value_);
    ASSERT_EQ(axes.InputAxis()[8].id_, "trade:0:constant:0");
    ASSERT_EQ(axes.InputAxis()[9].id_, "trade:1:constant:0");
    ASSERT_DOUBLE_EQ(axes.InputAxis()[8].value_, axes.InputAxis()[9].value_);
    ASSERT_EQ(axes.TradeInputPositions()[0], Vector_<size_t>({0, 1, 2, 3, 8}));
    ASSERT_EQ(axes.TradeInputPositions()[1], Vector_<size_t>({4, 5, 6, 7, 9}));
}

TEST(PortfolioRiskAxesTest, TestAxesOutlivePortfolioAndGetterCopiesAreDetached) {
    auto model = std::make_shared<BSModelData_>("same", 100.0, 0.2);
    const auto axes = [&] {
        const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS SPOT()"}));
        const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, Handle_<ModelData_>(model)}}));
        return ScriptPortfolioRiskAxes(portfolio);
    }();
    model->spot_ = 150.0;
    model.reset();
    auto detached = axes.InputAxis();
    detached[0].value_ = 200.0;
    ASSERT_DOUBLE_EQ(axes.InputAxis()[0].value_, 100.0);
    ASSERT_EQ(axes.OutputAxis()[0].id_, "trade:0:payoff");
    ASSERT_EQ(axes.OutputTrades(), Vector_<size_t>({0}));
}

TEST(PortfolioRiskAxesTest, TestAxisQueryReadsNoHistoryDateOrWorkerState) {
    RegisterAll_::Init();
    const Handle_<ModelData_> model(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(
        new ScriptProductData_("", {Cell_(Date_(2025, 1, 1)), Cell_(Date_(2027, 1, 1))}, {"x = FIX(EQ[PORTFOLIO_UNREAD])", "pay PAYS x"}));
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ tasks;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeTasks(&tasks);
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2020, 1, 1));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, model}}));
    const auto first = ScriptPortfolioRiskAxes(portfolio);
    XGLOBAL::SetEvaluationDate(Date_(2030, 1, 1));
    const auto second = ScriptPortfolioRiskAxes(portfolio);
    ASSERT_EQ(first.OutputAxis().size(), second.OutputAxis().size());
    ASSERT_EQ(first.OutputAxis()[0].id_, second.OutputAxis()[0].id_);
    ASSERT_EQ(first.TradeInputPositions(), second.TradeInputPositions());
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(tasks.calls_, 0);
}

TEST(PortfolioRiskAxesTest, TestNullAndMalformedProductErrorsRetainContext) {
    ASSERT_THROW(static_cast<void>(ScriptPortfolioRiskAxes({})), ScriptError_);
    const Handle_<ModelData_> model(new BSModelData_("same", 100.0, 0.2));
    const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"x = 1"}));
    const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"offending", product, model}}));
    try {
        static_cast<void>(ScriptPortfolioRiskAxes(portfolio));
        FAIL() << "a script without a payoff must not expose a risk output axis";
    } catch (const ScriptError_& error) {
        ASSERT_NE(String_(error.what()).find("field=products; trade=offending; cause="), String_::npos);
    }
}

TEST(PortfolioRiskAxesTest, TestSixModelFamiliesRetainExactSnapshotsAndCoordinateUnits) {
    const Date_ today(2026, 1, 1);
    HybridSettings_ hybridSettings;
    hybridSettings.domesticCurrency_ = "USD";
    hybridSettings.components_ = {NewHybridBSEquityData("A", "EQ[A]", "USD", "FA", 100.0, 0.2, 0.0),
                                  NewHybridDeterministicRateData("RATE", "USD", 0.03)};
    hybridSettings.correlation_ = NewHybridConstantCorrelationData("correlation", {"FA"}, Matrix_<>(1, 1, 1.0));
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    const auto gsr = NewGSRModelData("gsr", curve, NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
    MultiFactorGSRVolSettings_ multiSettings;
    multiSettings.factorNames_ = {"level"};
    multiSettings.gKnotDates_ = multiSettings.hKnotDates_ = {today};
    multiSettings.gValues_ = Matrix_<>(1, 1, 0.02);
    multiSettings.hValues_ = multiSettings.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto multi = NewMultiFactorGSRModelData("multi", curve, NewMultiFactorGSRVolData("vol", multiSettings));
    GSRSLVSettings_ slvSettings;
    slvSettings.maxStep_ = 0.25;
    const auto slv = NewGSRSLVModelData("slv", multi, NewGSRLeverageData("leverage", {-0.02, 0.02}, {0.0}, Matrix_<>(2, 1, 1.0)), slvSettings);
    const Vector_<Handle_<ModelData_>> families{NewBSModelData("bs", 100.0, 0.2, 0.03, 0.0),
                                                NewCorrelatedBSModelData("correlated", {"EQ[A]"}, {100.0}, {0.2}, {0.0}, 0.03, Matrix_<>(1, 1, 1.0)),
                                                NewHybridModelData("hybrid", hybridSettings),
                                                gsr,
                                                multi,
                                                slv};
    const Handle_<ScriptProductData_> product(new ScriptProductData_("", {Cell_(Date_(2027, 1, 1))}, {"pay PAYS 1"}));
    for (const auto& model : families) {
        SCOPED_TRACE(model->Type());
        const Handle_<ScriptPortfolioData_> portfolio(new ScriptPortfolioData_("", {{"A", product, model}, {"B", product, model}}));
        ASSERT_EQ(portfolio->Models().size(), 1);
        ASSERT_EQ(JSON::WriteString(*portfolio->Models()[0]), JSON::WriteString(*model));
        const auto prototype = CreateModel<double>(model);
        const auto axes = ScriptPortfolioRiskAxes(portfolio);
        ASSERT_EQ(axes.InputAxis().size(), prototype->NumParams());
        ASSERT_EQ(axes.TradeInputPositions()[0], axes.TradeInputPositions()[1]);
        for (size_t column = 0; column < prototype->NumParams(); ++column) {
            ASSERT_DOUBLE_EQ(axes.InputAxis()[column].value_, *prototype->Parameters()[column]);
            ASSERT_EQ(axes.InputAxis()[column].label_, prototype->ParameterLabels()[column]);
            ASSERT_EQ(axes.InputAxis()[column].id_, "model:0:parameter:" + String_(std::to_string(column)));
            ASSERT_DOUBLE_EQ(axes.InputAxis()[column].reportScale_, 1.0);
        }
    }
}
