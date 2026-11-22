#include <gtest/gtest.h>

#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/marketivlocalvol.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    MarketIvScriptRefitRequest_ Request(bool premiumBand = false) {
        const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, 100., {{expiry, 105., .96}});
        const OptionType_ call("CALL");
        MarketIvFitRequest_ fit{market, {}, {}};
        for (const double strike : {90., 100., 110.}) {
            const bool premium = strike == 110.;
            const double mid = premium ? .96 * 105. * MarketIvDetail::NormalizedBlack(std::log(strike / 105.), .04) : .2;
            fit.quotes_.push_back({String_(std::to_string(static_cast<int>(strike))), expiry, strike, call, MarketIvQuoteType_(premium ? "PRICE" : "DECIMAL_IV"), mid});
            if (premium && premiumBand) {
                fit.quotes_.back().bid_ = fit.quotes_.back().mid_;
                fit.quotes_.back().ask_ = 16.;
            }
        }
        const auto result = FitMarketIvSurface(fit);
        REQUIRE(result.IsSuccess(), result.Failure());
        MarketIvScriptRefitRequest_ request{result.Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}}, {"EQ[A]", "USD", "W_EQ", 1.},
                                            Handle_<ScriptProductData_>(new ScriptProductData_("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 100, 0)"}))};
        request.valuation_.evaluationDate_ = today;
        request.quoteIds_ = {"110", "90"};
        return request;
    }
} // namespace

TEST(MarketIvScriptRefitTest, TestMixedQuoteOrderAndSteps) {
    auto request = Request();
    request.steps_ = {.012, .0003};
    const auto explicitPlan = PlanMarketIvScriptRefit(request);
    const auto& rows = explicitPlan.rows_;
    ASSERT_EQ(rows.size(), 2U);
    ASSERT_EQ(rows[0].id_, "90");
    ASSERT_EQ(rows[0].quoteIndex_, 0U);
    ASSERT_EQ(rows[0].quoteType_, MarketIvQuoteType_::Value_::DECIMAL_IV);
    ASSERT_EQ(rows[0].inputUnit_, "decimal_iv");
    ASSERT_EQ(rows[0].step_, .0003);
    ASSERT_EQ(rows[1].id_, "110");
    ASSERT_EQ(rows[1].quoteIndex_, 2U);
    ASSERT_EQ(rows[1].quoteType_, MarketIvQuoteType_::Value_::PRICE);
    ASSERT_EQ(rows[1].inputUnit_, "premium");
    ASSERT_EQ(rows[1].step_, .012);
    ASSERT_DOUBLE_EQ(rows[1].forward_, 105.);
    ASSERT_DOUBLE_EQ(rows[1].discount_, .96);

    request.steps_.clear();
    const auto defaults = PlanMarketIvScriptRefit(request);
    ASSERT_EQ(defaults.rows_[0].step_, 1e-4);
    ASSERT_DOUBLE_EQ(defaults.rows_[1].step_, 1e-4 * .96 * 105.);
    ASSERT_EQ(explicitPlan.request_.steps_.size(), 2U);
}

TEST(MarketIvScriptRefitTest, TestPreflightRejectsInvalidFieldsBeforeWork) {
    size_t fitStarts = 0;
    const MarketIvDetail::ScopedFitStartCounter_ observeFits(&fitStarts);
    const auto base = Request();
    ASSERT_EQ(fitStarts, 1U);
    fitStarts = 0;
    TestSupport::SubmissionCounter_ sampling;
    const Script::Detail::ScopedSimulationObserver_ observeSampling(&sampling);
    const auto reject = [&](const char* field, auto change) {
        SCOPED_TRACE(field);
        auto request = base;
        change(&request);
        try {
            static_cast<void>(PlanMarketIvScriptRefit(request));
            FAIL() << "Expected preflight failure for " << field;
        } catch (const Exception_& e) {
            ASSERT_NE(std::string(e.what()).find(field), std::string::npos) << e.what();
        }
        ASSERT_EQ(fitStarts, 0U);
        ASSERT_EQ(sampling.submissions_, 0U);
    };
    reject("/quote_ids", [](auto* r) { r->quoteIds_ = {"90", "90"}; });
    reject("/quote_ids", [](auto* r) { r->quoteIds_ = {"100", "missing"}; });
    reject("/steps", [](auto* r) { r->steps_ = {1.}; });
    for (double h : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::denorm_min()})
        reject("/steps", [h](auto* r) { r->steps_ = {h, .01}; });
    reject("/valuation/evaluation_date", [](auto* r) { r->valuation_.evaluationDate_ = r->valuation_.evaluationDate_->AddDays(1); });
    reject("/simulation/enable_aad", [](auto* r) { r->simulation_.enableAad_ = true; });
    reject("/product/exercise", [](auto* r) {
        r->product_ = Handle_<ScriptProductData_>(new ScriptProductData_("exercise", r->product_->Dates(), {"EXERCISE MAX(spot() - 100, 0)"}));
    });
    reject("/budget/max_scenarios", [](auto* r) { r->budget_.maxScenarios_ = 4; });
    reject("/budget/workspace_bytes", [](auto* r) { r->budget_.workspaceBudgetBytes_ = 0; });
    reject("/budget/max_total_paths", [](auto* r) { r->budget_.maxTotalPaths_ = 81919; });
    reject("/budget/max_model_row_evaluations", [](auto* r) { r->budget_.maxModelRowEvaluations_ = 19999999; });
    reject("/simulation/rsg", [](auto* r) { r->simulation_.rsg_ = "sobol"; });
    reject("/simulation/normal_precision", [](auto* r) { r->simulation_.normalPrecision_ = "Fast"; });
    reject("/blocks", [](auto* r) { r->blocks_ = 1; });
    reject("/paths_per_block", [](auto* r) { r->pathsPerBlock_ = 0; });
}

TEST(MarketIvScriptRefitTest, TestBothBumpsMustMoveTheWholeBand) {
    auto request = Request(true);
    request.steps_ = {1e-15, .0003};
    const auto& quote = request.snapshot_.Request().quotes_[2];
    ASSERT_NE(quote.mid_ + request.steps_[0], quote.mid_);
    ASSERT_EQ(*quote.ask_ + request.steps_[0], *quote.ask_);
    ASSERT_THROW(static_cast<void>(PlanMarketIvScriptRefit(request)), Exception_);
}

TEST(MarketIvScriptRefitTest, TestBudgetOverflowsIdentifyPreflightFieldBeforeWork) {
    const auto base = Request();
    const auto maximum = std::numeric_limits<size_t>::max();
    const auto rejectOverflow = [&](const char* operation, const char* field, auto configure) {
        SCOPED_TRACE(operation);
        auto request = base;
        configure(&request);
        size_t fitStarts = 0;
        const MarketIvDetail::ScopedFitStartCounter_ observeFits(&fitStarts);
        TestSupport::SubmissionCounter_ sampling;
        const Script::Detail::ScopedSimulationObserver_ observeSampling(&sampling);
        try {
            static_cast<void>(PlanMarketIvScriptRefit(request));
            FAIL() << "Expected budget overflow";
        } catch (const MarketIvLocalVolError_& error) {
            EXPECT_STREQ(error.code_.c_str(), "MarketIvLocalVolBudgetExceeded");
            EXPECT_STREQ(error.stage_.c_str(), "Preflight");
            EXPECT_STREQ(error.field_.c_str(), field);
            EXPECT_STREQ(error.actual_.c_str(), "overflow");
        }
        EXPECT_EQ(fitStarts, 0U);
        EXPECT_EQ(sampling.submissions_, 0U);
    };

    rejectOverflow("paths per scenario", "/budget/max_total_paths", [&](auto* r) {
        r->blocks_ = 2;
        r->pathsPerBlock_ = maximum;
    });
    rejectOverflow("paths across all scenarios", "/budget/max_total_paths", [&](auto* r) {
        r->blocks_ = 2;
        r->pathsPerBlock_ = maximum / 2;
        r->budget_.maxTotalPaths_ = maximum;
    });
    auto fitRequest = base.snapshot_.Request();
    fitRequest.settings_.maxModelRowEvaluations_ = maximum;
    const auto fit = FitMarketIvSurface(fitRequest);
    ASSERT_TRUE(fit.IsSuccess()) << fit.Failure().c_str();
    rejectOverflow("fit rows across refits", "/budget/max_model_row_evaluations", [&](auto* r) { r->snapshot_ = fit.Snapshot(); });
    rejectOverflow("three model payloads", "/budget/workspace_bytes", [&](auto* r) { r->model_.numericPayloadBudgetBytes_ = maximum; });
    rejectOverflow("grid plus model payload", "/budget/workspace_bytes", [&](auto* r) { r->grid_.settings_.numericPayloadBudgetBytes_ = maximum; });
}

TEST(MarketIvScriptRefitTest, TestPlanOwnsInputsAndAdmitsExactBudget) {
    const auto plan = [] {
        auto request = Request();
        request.blocks_ = 2;
        request.pathsPerBlock_ = 3;
        request.budget_.maxScenarios_ = 5;
        request.budget_.maxTotalPaths_ = 30;
        request.budget_.maxModelRowEvaluations_ = 20000000;
        const auto initial = PlanMarketIvScriptRefit(request);
        request.budget_.workspaceBudgetBytes_ = initial.work_.reservedWorkspaceBytes_;
        const auto exact = PlanMarketIvScriptRefit(request);
        --request.budget_.workspaceBudgetBytes_;
        EXPECT_THROW(static_cast<void>(PlanMarketIvScriptRefit(request)), Exception_);
        request.grid_.spots_[0] = 1.;
        request.model_.maxStep_ = .5;
        request.simulation_.useBb_ = true;
        return exact;
    }();
    ASSERT_EQ(plan.rows_[0].id_, "90");
    ASSERT_EQ(plan.request_.product_->EventTexts().front(), "pay PAYS MAX(spot() - 100, 0)");
    ASSERT_EQ(plan.request_.grid_.spots_, (Vector_<>{80., 100., 120.}));
    ASSERT_EQ(plan.request_.grid_.times_, (Vector_<>{.1, 1.}));
    ASSERT_EQ(plan.request_.model_.maxStep_, 1. / 52.);
    ASSERT_FALSE(plan.request_.simulation_.useBb_);
    ASSERT_TRUE(plan.baseModel_);
    ASSERT_EQ(plan.work_.refits_, 4U);
    ASSERT_EQ(plan.work_.grids_, 5U);
    ASSERT_EQ(plan.work_.scenarios_, 5U);
    ASSERT_EQ(plan.work_.pathsPerScenario_, 6U);
    ASSERT_EQ(plan.work_.totalPaths_, 30U);
    ASSERT_EQ(plan.work_.modelRowEvaluationLimit_, 20000000U);
    ASSERT_EQ(plan.work_.reservedWorkspaceBytes_, plan.request_.budget_.workspaceBudgetBytes_);
}

TEST(MarketIvScriptRefitTest, TestMovedRequestCannotChangePlanThroughSavedReferences) {
    auto request = Request();
    request.steps_ = {.012, .0003};
    auto& time = request.grid_.times_.front();
    auto& step = request.steps_.front();
    auto& id = request.quoteIds_.front();

    const auto plan = PlanMarketIvScriptRefit(std::move(request));
    time = .2;
    step = .04;
    id = "100";

    const auto summary = plan.baseModel_->Summary();
    ASSERT_EQ(summary.gridFirstTime_, .1);
    EXPECT_EQ(plan.request_.grid_.times_.front(), summary.gridFirstTime_);
    ASSERT_EQ(plan.rows_.size(), 2U);
    ASSERT_EQ(plan.rows_[1].id_, "110");
    ASSERT_EQ(plan.rows_[1].step_, .012);
    EXPECT_EQ(plan.request_.quoteIds_.front(), plan.rows_[1].id_);
    EXPECT_EQ(plan.request_.steps_.front(), plan.rows_[1].step_);
}

TEST(MarketIvScriptRefitTest, TestPlanFreezesGlobalFixingAndValuationDate) {
    auto request = Request();
    const Date_ today = *request.valuation_.evaluationDate_;
    const auto dateScope = XGLOBAL::SetEvaluationDateInScope(today);
    request.valuation_.evaluationDate_.reset();
    const String_ index = "EQ[DAL561_HISTORY]";
    const XGLOBAL::ScopedOverride_<FixHistory_> restore([](const FixHistory_& saved) { XGLOBAL::StoreFixings("EQ[DAL561_HISTORY]", saved, false); },
                                                        Global::Fixings_().History(index));
    const DateTime_ fixingTime(today.AddDays(-1), 0.);
    request.product_ = Handle_<ScriptProductData_>(new ScriptProductData_("fixed", request.product_->Dates(), {"pay PAYS FIX(" + index + ", 2026-11-15)"}));
    TestSupport::StoreScriptTestFixing(index, 87., fixingTime);
    const auto plan = PlanMarketIvScriptRefit(request);
    TestSupport::StoreScriptTestFixing(index, 91., fixingTime);
    const auto changed = PlanMarketIvScriptRefit(request);
    ASSERT_EQ(plan.request_.valuation_.evaluationDate_, today);
    ASSERT_TRUE(plan.request_.valuation_.fixings_);
    ASSERT_EQ(plan.request_.valuation_.fixings_->Find(index, fixingTime), 87.);
    ASSERT_EQ(changed.request_.valuation_.fixings_->Find(index, fixingTime), 91.);
    request.valuation_.fixings_ = plan.request_.valuation_.fixings_;
    ASSERT_EQ(PlanMarketIvScriptRefit(request).request_.valuation_.fixings_, request.valuation_.fixings_);
}
