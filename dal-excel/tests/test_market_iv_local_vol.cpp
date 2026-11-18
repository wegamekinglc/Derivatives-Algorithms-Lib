#include <gtest/gtest.h>

#include <iomanip>

#include <dal-excel/src/__marketivlocalvol.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-excel/src/__xccy_test_api.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/storage/json.hpp>
#include <marketivlocalvolrequestgolden.hpp>

using namespace Dal;

namespace {
    Matrix_<Cell_> SettingsRows() {
        Matrix_<Cell_> rows(6, 2);
        rows(0, 0) = "index";
        rows(0, 1) = "EQ[A]";
        rows(1, 0) = "currency";
        rows(1, 1) = "USD";
        rows(2, 0) = "factor";
        rows(2, 1) = "W_EQ";
        rows(3, 0) = "horizon";
        rows(3, 1) = 1.25;
        rows(4, 0) = "max_step";
        return rows;
    }
} // namespace

TEST(ExcelMarketIvLocalVolTest, TestTypedSettingsAtomicAndStrictRows) {
    InitGlobalData(1);
    Handle_<StorableMarketIvLocalVolModelSettings_> settings;
    MarketIvLocalVolModelSettings_New("settings", SettingsRows(), &settings);
    ASSERT_NEAR(settings->val_.maxStep_, 1. / 52., 1e-15);
    const auto original = settings;
    for (const auto& key : {"index", "unknown", "max_step"}) {
        auto rows = SettingsRows();
        rows(5, 0) = key;
        rows(5, 1) = .5;
        ASSERT_THROW(MarketIvLocalVolModelSettings_New("bad", rows, &settings), Exception_);
        ASSERT_EQ(settings, original);
    }
    auto rows = SettingsRows();
    rows(1, 0) = Cell_();
    rows(1, 1) = Cell_();
    ASSERT_THROW(MarketIvLocalVolModelSettings_New("gap", rows, &settings), Exception_);
    for (const auto& cell : {Cell_(true), Cell_("1.25"), Cell_()}) {
        rows = SettingsRows();
        rows(3, 1) = cell;
        ASSERT_THROW(MarketIvLocalVolModelSettings_New("type", rows, &settings), Exception_);
    }
    ASSERT_THROW(MarketIvLocalVolModelSettings_New("empty", Matrix_<Cell_>(), &settings), Exception_);
    Matrix_<Cell_> config;
    MarketIvLocalVolModelSettings_Get_Configuration(settings, &config);
    ASSERT_EQ(config.Rows(), 10);
    ASSERT_EQ(Cell::ToString(config(0, 1)), "dal.market-iv-local-vol-settings/1");
}

TEST(ExcelMarketIvLocalVolTest, TestModelHandleReportsAndPassiveValuation) {
    InitGlobalData(1);
    const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
    const MarketIvMarket_ market(today, 100., {{expiry, 102., .98}});
    const MarketIvFitRequest_ request{market, {{"atm", expiry, 102., OptionType_("CALL"), MarketIvQuoteType_("DECIMAL_IV"), .2}}, {}};
    const auto source = CalibrateMarketIvDupire(FitMarketIvSurface(request).Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}});
    Handle_<StorableMarketIvDupireResult_> grid(new StorableMarketIvDupireResult_("grid", source));
    Handle_<StorableMarketIvLocalVolModelSettings_> settings;
    MarketIvLocalVolModelSettings_New("settings", SettingsRows(), &settings);
    Handle_<MarketIvLocalVolModelData_> model;
    MarketIvLocalVolModelData_New("market", grid, settings, &model);
    grid.reset();
    const auto original = model;
    ASSERT_THROW(MarketIvLocalVolModelData_New("bad", {}, settings, &model), Exception_);
    ASSERT_EQ(model, original);
    ASSERT_THROW(static_cast<void>(JSON::WriteString(*settings)), Exception_);
    ASSERT_THROW(static_cast<void>(JSON::WriteString(*model)), MarketIvLocalVolError_);
    Matrix_<Cell_> table;
    MarketIvLocalVolModelData_Get_Summary(model, &table);
    ASSERT_EQ(table.Rows(), 23);
    ASSERT_EQ(Cell::ToString(table(0, 1)), "dal.market-iv-local-vol-model-summary/1");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(table(3, 1)), Date::ToExcel(today));
    MarketIvLocalVolModelData_Get_Carry(model, &table);
    ASSERT_EQ(table.Cols(), 8);
    ASSERT_EQ(Cell::ToString(table(0, 5)), "log_forward_over_spot");
    ASSERT_TRUE(Cell::IsEmpty(table(table.Rows() - 1, 1)));
    table(1, 3) = 0.;
    MarketIvLocalVolModelData_Get_Carry(model, &table);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(table(1, 3)), 100.);
    MarketIvLocalVolModelData_Get_Configuration(model, &table);
    ASSERT_EQ(table.Rows(), 10);
    MarketIvLocalVolModelData_Get_Capabilities(model, &table);
    ASSERT_EQ(table.Rows(), 13);
    MarketIvLocalVolModelData_Get_Warnings(model, &table);
    ASSERT_EQ(table.Cols(), 3);
    MarketIvLocalVolModelData_Get_ConstructionExecution(model, &table);
    ASSERT_EQ(table.Rows(), 5);
    const auto product = NewScriptProduct("df", {Cell_(expiry)}, {"pay PAYS 1"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const Handle_<StorableScriptValuationSettings_> v(new StorableScriptValuationSettings_("valuation", valuation));
    MarketIvLocalVolModelData_Get_Execution(product, model, v, {}, &table);
    const Vector_<String_> executionFields{
        "schema", "source_identity", "model_identity", "request_identity", "evaluation_date", "requested_max_step", "resolved_max_step",
        "required_time", "carry_knots", "grid_time_knots", "anchor_count", "internal_steps", "simulation_dimension", "workers",
        "shared_numeric_bytes", "per_worker_numeric_bytes", "planned_numeric_payload_bytes", "numeric_payload_bytes",
        "planned_workspace_bytes", "peak_workspace_bytes", "prepared_columns", "prepared_market_queries", "per_path_market_queries"};
    ASSERT_EQ(table.Rows(), executionFields.size());
    for (int r = 0; r < table.Rows(); ++r)
        ASSERT_EQ(Cell::ToString(table(r, 0)), executionFields[r]);
    ASSERT_EQ(Cell::ToString(table(0, 1)), "dal.market-iv-local-vol-execution/1");
    ASSERT_NEAR(ValueByMonteCarlo(product, Handle_<ModelData_>(model), 16, valuation).at("PV"), .98, 2e-12);
    {
        struct RestoreThreads_ {
            size_t count_ = ThreadPool_::GetInstance()->NumThreads();
            // The XLL and test executable own separate native thread pools on Windows.
            std::pair<size_t, bool> excelState_ = Excel::ScriptTestStartWorkers(1);
            ~RestoreThreads_() {
                Excel::ScriptTestRestoreWorkers(excelState_);
                ThreadPool_::GetInstance()->Start(count_, true);
            }
        } restore;
        ThreadPool_::GetInstance()->Start(1, true);
        const auto identityProduct = NewScriptProduct("ScriptProductData_", {Cell_(expiry)}, {"pay PAYS 1"});
        MarketIvLocalVolModelData_Get_Execution(identityProduct, model, v, {}, &table);
        const auto emptyGolden = Dal::Test::MarketIvLocalVolRequestGolden(model->ModelIdentity(), false);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(table(13, 1)), 1.);
        ASSERT_EQ(Cell::ToString(table(3, 1)), emptyGolden);
        Handle_<StorableMarketFixingSnapshot_> fixings;
        MarketFixingSnapshot_New({"EQ[A]"}, {Cell_(Date::ToExcel(today.AddDays(-1)) + .5)}, {99.}, &fixings);
        ASSERT_DOUBLE_EQ(fixings->val_->Values().at("EQ[A]").begin()->first.Frac(), .5);
        auto frozenValuation = valuation;
        frozenValuation.fixings_ = fixings->val_;
        const Handle_<StorableScriptValuationSettings_> noonSettings(new StorableScriptValuationSettings_("noon", frozenValuation));
        MarketIvLocalVolModelData_Get_Execution(identityProduct, model, noonSettings, {}, &table);
        ASSERT_EQ(Cell::ToString(table(3, 1)), Dal::Test::MarketIvLocalVolRequestGolden(model->ModelIdentity(), true));
        ASSERT_EQ(table.Cols(), 2);
        ASSERT_EQ(table.Rows(), executionFields.size());
        for (int r = 0; r < table.Rows(); ++r)
            ASSERT_EQ(Cell::ToString(table(r, 0)), executionFields[r]);
        ASSERT_EQ(Cell::ToString(table(0, 1)), "dal.market-iv-local-vol-execution/1");
        ASSERT_NEAR(ValueByMonteCarlo(identityProduct, Handle_<ModelData_>(model), 16, frozenValuation).at("PV"), .98, 2e-12);
    }
    const auto vanilla = NewScriptProduct("call", {Cell_(expiry)}, {"pay PAYS MAX(FIX(EQ[A]) - 102, 0)"});
    MonteCarloSettings_ simulation;
    simulation.rsg_ = "mrg32";
    const double pv = ValueByMonteCarlo(vanilla, Handle_<ModelData_>(model), 64, valuation, simulation).at("PV");
    std::cout << std::setprecision(17) << "DAL543_ALIGNMENT " << model->SourceIdentity() << " " << model->ModelIdentity() << " " << pv << "\n";
}
