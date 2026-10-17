//
// Created by Codex on 2026/10/7.
//

#include <gtest/gtest.h>

#include <dal-excel/src/__portfoliorisk.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/models.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct PortfolioFixture_ {
        Handle_<Script::ScriptPortfolioData_> portfolio_;
        Handle_<StorableScriptValuationSettings_> valuation_;
        Matrix_<Cell_> trades_{2, 3};
        PortfolioFixture_() {
            Excel::ScriptTestInitialize(1);
            const auto model = NewBSModelData("portfolio-model", 1.0, 0.0, 0.0, 0.0);
            const auto modelTag = Excel::PortfolioTestStore(handle_cast<Storable_>(model));
            for (int trade = 0; trade < 2; ++trade) {
                Handle_<ScriptProductData_> product;
                Product_New("portfolio-product-" + String_(std::to_string(trade)), {Cell_("X"), Cell_(double(Date::ToExcel(Date_(2027, 1, 1))))},
                            {trade == 0 ? "5" : "7", trade == 0 ? "a = 2 * SPOT() + X pay PAYS a" : "a = 3 * SPOT() + X pay PAYS a"}, &product);
                trades_(trade, 0) = trade == 0 ? "A" : "B";
                trades_(trade, 1) = Excel::PortfolioTestStore(handle_cast<Storable_>(product));
                trades_(trade, 2) = modelTag;
            }
            ScriptPortfolio_New("portfolio", trades_, &portfolio_);
            auto settings = ScriptValuationSettings_();
            settings.evaluationDate_ = Date_(2026, 1, 1);
            valuation_.reset(new StorableScriptValuationSettings_("valuation", settings));
        }
    };

    Matrix_<Cell_> RequestSettings(bool native, bool attribution) {
        Matrix_<Cell_> settings(4, 2);
        settings(0, 0) = "outputs";
        settings(0, 1) = attribution ? "trade:1:payoff;trade:0:output:0;trade:0:payoff" : "trade:1:payoff;trade:0:payoff";
        settings(1, 0) = "inputs";
        settings(1, 1) = native ? Cell_("model:0:parameter:0;trade:0:constant:0;trade:1:constant:0") : Cell_();
        settings(2, 0) = attribution ? "max_block_width" : "weights";
        settings(2, 1) = attribution ? Cell_(2.0) : Cell_("-1;2");
        settings(3, 0) = "report_factors";
        settings(3, 1) = native ? Cell_("2;0.5;3") : Cell_();
        return settings;
    }

    struct RejectPortfolioGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectPortfolioGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectPortfolioGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };
} // namespace

TEST(ExcelPortfolioRiskTest, TestSealedOwnerRegistryAndStrictTradeCellsPreservePriorHandles) {
    const PortfolioFixture_ fixture;
    ASSERT_EQ(fixture.portfolio_->TradeIds(), (Vector_<String_>{"A", "B"}));
    ASSERT_EQ(fixture.portfolio_->ModelOwners(), (Vector_<int>{0, 0}));
    auto prior = fixture.portfolio_;
    for (const Matrix_<Cell_>& malformed : {Matrix_<Cell_>(0, 3), Matrix_<Cell_>(1, 2), Matrix_<Cell_>(1, 3)})
        ASSERT_THROW(ScriptPortfolio_New("bad", malformed, &prior), Exception_);
    for (const Cell_& malformed : {Cell_(true), Cell_(1.0), Cell_(String_(std::string("A\0B", 3)))}) {
        auto table = fixture.trades_;
        table(0, 0) = malformed;
        ASSERT_THROW(ScriptPortfolio_New("bad", table, &prior), Exception_);
    }
    auto table = fixture.trades_;
    table(1, 0) = "a";
    ASSERT_THROW(ScriptPortfolio_New("bad", table, &prior), Exception_);
    table = fixture.trades_;
    table(0, 1) = table(0, 2);
    ASSERT_THROW(ScriptPortfolio_New("bad", table, &prior), Exception_);
    ASSERT_EQ(prior.get(), fixture.portfolio_.get());
}

TEST(ExcelPortfolioRiskTest, TestWeightedAndBlockedRowsAcrossNativeAndPassiveModes) {
    const PortfolioFixture_ fixture;
    for (const bool compiled : {false, true})
        for (const bool native : {false, true}) {
            auto execution = DefaultRiskMonteCarloSettings();
            execution.compiled_ = compiled;
            execution.enableAad_ = native;
            const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("simulation", execution));
            Handle_<StorablePortfolioWeightedRiskRequest_> weightedRequest;
            PortfolioWeightedRiskRequest_New("weighted", RequestSettings(native, false), &weightedRequest);
            Handle_<StorablePortfolioWeightedRiskResult_> weighted;
            PortfolioMonteCarlo_ValueWithWeightedRisk(fixture.portfolio_, 17, weightedRequest, fixture.valuation_, simulation, &weighted);
            const auto weightedHandle = handle_cast<Storable_>(weighted);
            Matrix_<Cell_> values, shape, raw, reported;
            double objective = 0.0;
            PortfolioRiskResult_Get_Objective(weightedHandle, &objective);
            PortfolioRiskResult_Get_Values(weightedHandle, &values);
            PortfolioRiskResult_Get_Shape(weightedHandle, &shape);
            PortfolioRiskResult_Get_Jacobian(weightedHandle, false, &raw);
            ASSERT_DOUBLE_EQ(objective, 4.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 10.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values(2, 3)), 7.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 1.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), native ? 3.0 : 0.0);
            if (native) {
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 0)), 1.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 1)), 2.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 2)), -1.0);
            }
            Handle_<StorablePortfolioJacobianRiskRequest_> request;
            PortfolioJacobianRiskRequest_New("rows", RequestSettings(native, true), &request);
            Handle_<StorablePortfolioJacobianRiskResult_> result;
            PortfolioMonteCarlo_ValueWithJacobianRisk(fixture.portfolio_, 17, request, fixture.valuation_, simulation, &result);
            const auto resultHandle = handle_cast<Storable_>(result);
            PortfolioRiskResult_Get_Values(resultHandle, &values);
            PortfolioRiskResult_Get_Shape(resultHandle, &shape);
            PortfolioRiskResult_Get_Jacobian(resultHandle, false, &raw);
            PortfolioRiskResult_Get_Jacobian(resultHandle, true, &reported);
            ASSERT_EQ(values.Rows(), 4);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 10.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values(2, 3)), 7.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(values(3, 3)), 7.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 0)), 3.0);
            ASSERT_DOUBLE_EQ(Cell::ToDouble(shape(0, 1)), native ? 3.0 : 0.0);
            if (native) {
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 0)), 3.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 1)), 0.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(raw(0, 2)), 1.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(1, 0)), 4.0);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(1, 1)), 0.5);
                ASSERT_DOUBLE_EQ(Cell::ToDouble(reported(1, 2)), 0.0);
            } else {
                ASSERT_EQ(raw.Rows(), 1);
                ASSERT_EQ(raw.Cols(), 1);
                ASSERT_TRUE(Cell::IsEmpty(raw(0, 0)));
            }
        }
}

TEST(ExcelPortfolioRiskTest, TestDetachedFrozenGettersRejectWorkAndInvalidRequestsRecover) {
    const PortfolioFixture_ fixture;
    Handle_<StorablePortfolioJacobianRiskRequest_> request;
    auto settings = RequestSettings(true, true);
    PortfolioJacobianRiskRequest_New("request", settings, &request);
    settings.Fill(Cell_());
    Handle_<StorablePortfolioJacobianRiskResult_> result;
    PortfolioMonteCarlo_ValueWithJacobianRisk(fixture.portfolio_, 17, request, fixture.valuation_, {}, &result);
    const auto original = result;
    const auto handle = handle_cast<Storable_>(result);
    {
        const RejectPortfolioGetterWork_ reject;
        Matrix_<Cell_> values, inputs, outputs, execution, sampling, provenance, trades, history, product;
        Vector_<String_> model;
        PortfolioRiskResult_Get_Values(handle, &values);
        values.Fill(Cell_());
        PortfolioRiskResult_Get_Values(handle, &values);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(values(1, 3)), 10.0);
        PortfolioRiskResult_Get_Inputs(handle, true, &inputs);
        PortfolioRiskResult_Get_Outputs(handle, true, &outputs);
        ASSERT_EQ(inputs.Rows(), 7);
        ASSERT_EQ(outputs.Rows(), 5);
        PortfolioRiskResult_Get_Execution(handle, &execution);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(execution(1, 6)), 34.0);
        ASSERT_DOUBLE_EQ(Cell::ToDouble(execution(1, 7)), 51.0);
        ASSERT_EQ(Cell::ToString(execution(1, 11)), "2;2");
        PortfolioRiskResult_Get_Sampling(handle, &sampling);
        ASSERT_GT(sampling.Rows(), 1);
        ASSERT_EQ(sampling.Cols(), 6);
        PortfolioRiskResult_Get_Provenance(handle, &provenance);
        ASSERT_EQ(Cell::ToString(provenance(0, 1)), "NativeAADJacobianPortfolio");
        PortfolioRiskResult_Get_Trades(handle, &trades);
        ASSERT_EQ(Cell::ToString(trades(1, 1)), "A");
        ASSERT_DOUBLE_EQ(Cell::ToDouble(trades(2, 2)), 0.0);
        PortfolioRiskResult_Get_TradeProvenance(handle, 0, &provenance);
        PortfolioRiskResult_Get_History(handle, 0, &history);
        PortfolioRiskResult_Get_Product(handle, 0, &product);
        PortfolioRiskResult_Get_ModelSnapshot(handle, 0, &model);
        ASSERT_EQ(product.Rows(), 2);
        ASSERT_FALSE(model.empty());
        ASSERT_EQ(reject.history_.historyCalls_, 0);
        ASSERT_EQ(reject.history_.fixingCalls_, 0);
        ASSERT_EQ(reject.workers_.calls_, 0);
        double objective = 0.0;
        ASSERT_THROW(PortfolioRiskResult_Get_Objective(handle, &objective), Exception_);
        for (const double ordinal : {-1.0, 0.5, 2.0})
            ASSERT_THROW(PortfolioRiskResult_Get_Product(handle, ordinal, &product), Exception_);
        ASSERT_THROW(PortfolioRiskResult_Get_Values({}, &values), Exception_);
        auto impossible = PortfolioJacobianRiskRequest_();
        impossible.scratchCapacityBudgetBytes_ = 0;
        const Handle_<StorablePortfolioJacobianRiskRequest_> capacity(new StorablePortfolioJacobianRiskRequest_("capacity", impossible));
        ASSERT_THROW(PortfolioMonteCarlo_ValueWithJacobianRisk(fixture.portfolio_, 17, capacity, fixture.valuation_, {}, &result), Exception_);
        ASSERT_EQ(result.get(), original.get());
    }
    Matrix_<Cell_> malformed(1, 2);
    malformed(0, 0) = "max_block_width";
    const auto priorRequest = request;
    for (const Cell_& cell : {Cell_(true), Cell_(0.0), Cell_(-1.0), Cell_(0.5), Cell_(9007199254740992.0)}) {
        malformed(0, 1) = cell;
        ASSERT_THROW(PortfolioJacobianRiskRequest_New("bad", malformed, &request), Exception_);
        ASSERT_EQ(request.get(), priorRequest.get());
    }
    Handle_<StorablePortfolioJacobianRiskResult_> recovered;
    PortfolioMonteCarlo_ValueWithJacobianRisk(fixture.portfolio_, 17, request, fixture.valuation_, {}, &recovered);
    Matrix_<Cell_> rows;
    PortfolioRiskResult_Get_Values(handle_cast<Storable_>(recovered), &rows);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(rows(1, 3)), 10.0);
}

TEST(ExcelPortfolioRiskTest, TestRepositoryLookupErrorsRetainOriginalTradeAndCellContext) {
    const PortfolioFixture_ fixture;
    auto table = fixture.trades_;
    table(0, 1) = "no-such-portfolio-product";
    auto prior = fixture.portfolio_;
    try {
        ScriptPortfolio_New("bad", table, &prior);
        FAIL() << "missing repository handle must fail with original row/trade context";
    } catch (const Exception_& error) {
        ASSERT_NE(String_(error.what()).find("trade=A"), String_::npos) << error.what();
        ASSERT_NE(String_(error.what()).find("column=2"), String_::npos) << error.what();
    }
    ASSERT_EQ(prior.get(), fixture.portfolio_.get());
}
