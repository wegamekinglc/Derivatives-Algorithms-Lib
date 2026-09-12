//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <atomic>
#include <limits>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    struct RejectHistory_ : Dal::Detail::FixingReadObserver_ {
        void BeforeHistory(const String_&) override { THROW("unexpected history read"); }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { THROW("unexpected fixing read"); }
    };

    struct ReadCounter_ : Dal::Detail::FixingReadObserver_ {
        size_t histories_ = 0;
        size_t fixings_ = 0;
        void BeforeHistory(const String_&) override { ++histories_; }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { ++fixings_; }
    };

    struct SubmissionCounter_ : Dal::Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override { ++submissions_; }
    };

    struct ControlledModel_ : AAD::BlackScholes_<double> {
        std::shared_ptr<std::atomic<size_t>> paths_ = std::make_shared<std::atomic<size_t>>(0);
        size_t allocations_ = 0;
        bool badPath_ = false;
        double fixing_ = 120.0;
        Vector_<> dates_;
        ControlledModel_() : AAD::BlackScholes_<double>(999.0, 0.0) {}
        void Allocate(const Vector_<>& dates, const Vector_<AAD::SampleDef_>& defs) override {
            ++allocations_;
            dates_ = dates;
            AAD::BlackScholes_<double>::Allocate(dates, defs);
        }
        void GeneratePath(const Vector_<>& gauss, AAD::Scenario_<double>* path) const override {
            paths_->fetch_add(1);
            AAD::BlackScholes_<double>::GeneratePath(gauss, path);
            for (size_t i = 0; i < path->size(); ++i)
                for (auto& value : (*path)[i].observations_)
                    value = badPath_ ? std::numeric_limits<double>::quiet_NaN() : (dates_[i] == 3.0 / DAYS_PER_YEAR ? fixing_ : 999.0);
        }
    };

    ScriptValuationSettings_ BoundSettings() {
        ScriptValuationSettings_ result;
        result.modelBindings_ = {{"spot", "EQ[DAL196_TEST]"}};
        return result;
    }

    void StoreHistory() {
        FixHistory_ history;
        history.vals_ = {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}};
        XGLOBAL::StoreFixings("EQ[DAL196_TEST]", history, false);
    }

    ScriptProductData_ Product(const String_& text, const Date_& date = Date_(2026, 9, 22)) { return {"", {Cell_(date)}, {text}}; }

    struct PoolRestore_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        ~PoolRestore_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    struct PaymentModel_ : ControlledModel_ {
        void GeneratePath(const Vector_<>& gauss, AAD::Scenario_<double>* path) const override {
            ControlledModel_::GeneratePath(gauss, path);
            for (size_t i = 0; i < path->size(); ++i)
                (*path)[i].numeraire_ = i == 0 ? 2.0 : (i == 1 ? 4.0 : 5.0);
        }
    };

    struct SetupFailureModel_ : ControlledModel_ {
        bool failAllocate_ = false;
        size_t initializations_ = 0;
        void Allocate(const Vector_<>& dates, const Vector_<AAD::SampleDef_>& defs) override {
            ControlledModel_::Allocate(dates, defs);
            REQUIRE(!failAllocate_, "injected allocation failure");
        }
        void Init(const Vector_<>&, const Vector_<AAD::SampleDef_>&) override {
            ++initializations_;
            THROW("injected initialization failure");
        }
    };

    struct WorkerStorage_ {
        Vector_<const double*> cells_;
        Vector_<size_t> evaluations_;
        explicit WorkerStorage_(size_t threads) : cells_(threads, nullptr), evaluations_(threads, 0) {}
    };

    struct GuardedPrepared_ : PreparedScript_ {
        WorkerStorage_* storage_;
        GuardedPrepared_(PreparedScript_&& prepared, WorkerStorage_* storage) : PreparedScript_(std::move(prepared)), storage_(storage) {}
        void Evaluate(const AAD::Scenario_<double>& path, Evaluator_<double>& evaluator) const {
            RejectHistory_ reject;
            const Dal::Detail::ScopedFixingReadObserver_ guard(&reject);
            const auto thread = ThreadPool_::ThreadNum();
            const auto* cell = &path[0].observations_[0];
            REQUIRE(!storage_->cells_[thread] || storage_->cells_[thread] == cell, "observation storage moved between paths");
            REQUIRE(path[0].observations_.size() == 1 && path[1].observations_.empty(), "observation storage grew");
            storage_->cells_[thread] = cell;
            ++storage_->evaluations_[thread];
            PreparedScript_::Evaluate(path, evaluator);
        }
    };

    struct HistoryRestore_ {
        String_ index_;
        FixHistory_ previous_;
        explicit HistoryRestore_(const String_& index) : index_(index), previous_(Global::Fixings_().History(index)) {}
        ~HistoryRestore_() { XGLOBAL::StoreFixings(index_, previous_, false); }
    };
} // namespace

TEST(ScriptObservationSimulationTest, TestFutureOnlyNoHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"});
    const Handle_<ModelData_> model(new BSModelData_("unrelated model name", 123.0, 0.0));
    ScriptValuationSettings_ settings;
    settings.modelBindings_ = {{"spot", "EQ[DAL196_TEST]"}};
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reject);
    for (const auto& snapshot : {Handle_<MarketFixingSnapshot_>(), Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_())}) {
        const auto result = MCSimulation<double>(product, model, 1, settings, MonteCarloSettings_(), snapshot);
        ASSERT_NEAR(result.aggregated_, 123.0, 123.0e-12);
        ControlledModel_ controlled;
        controlled.fixing_ = 123.0;
        const auto prepared = PrepareScript(product, &controlled, settings, {}, snapshot);
        ASSERT_EQ(MCDoubleSimulation(prepared, &controlled, 1, "sobol", false, false, true).aggregated_, 123.0);
    }
}

TEST(ScriptObservationSimulationTest, TestMixedAndRepeatedHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    for (const bool future : {false, true}) {
        ReadCounter_ reads;
        const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
        ControlledModel_ model;
        const auto product =
            Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], " + String_(future ? "2026-09-15)" : "2026-09-11)"));
        const auto prepared = PrepareScript(product, &model, BoundSettings(), {});
        ASSERT_EQ(reads.histories_, 1);
        ASSERT_EQ(reads.fixings_, 1);
        RejectHistory_ reject;
        const Dal::Detail::ScopedFixingReadObserver_ noMoreReads(&reject);
        const auto result = MCDoubleSimulation(prepared, &model, 257, "sobol", false, false, true);
        ASSERT_EQ(result.aggregated_, 257 * (future ? 200.0 : 160.0));
        ASSERT_EQ(model.allocations_, 1);
        ASSERT_EQ(model.paths_->load(), 257);
    }
}

TEST(ScriptObservationSimulationTest, TestRetainedFutureFixing) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 23))},
                                     {"x = FIX(EQ[DAL196_TEST], 2026-09-15) pay PAYS x", "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) - x"});
    ControlledModel_ model;
    const auto prepared = PrepareScript(product, &model, BoundSettings(), {});
    ASSERT_EQ(prepared.Plan().Requests().size(), 1);
    ASSERT_EQ(prepared.Plan().Request(0).uses_.size(), 2);
    ASSERT_EQ(prepared.Plan().SampleDates().size(), 3);
    ASSERT_EQ(prepared.Plan().SampleDates()[0], Date_(2026, 9, 15));
    ASSERT_EQ(prepared.Plan().EventToSample()[0], 1);
    ASSERT_EQ(prepared.Plan().EventToSample()[1], 2);
    AAD::Scenario_<double> path;
    AAD::AllocatePath(prepared.DefLine(), path);
    AAD::InitializePath(path);
    auto evaluator = prepared.BuildEvaluator<double>();
    const Vector_<> gauss(model.SimDim(), 0.0);
    const double* retainedCell = &path[0].observations_[0];
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reject);
    for (size_t i = 0; i < 257; ++i) {
        model.GeneratePath(gauss, &path);
        prepared.Evaluate(path, evaluator);
        ASSERT_EQ(evaluator.VarVals()[prepared.PayOffIdx()], 120.0);
        ASSERT_EQ(&path[0].observations_[0], retainedCell);
        ASSERT_TRUE(path[1].observations_.empty());
        ASSERT_TRUE(path[2].observations_.empty());
    }
    const ScriptProductData_ zeroProduct(
        "", {Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 23))},
        {"x = FIX(EQ[DAL196_TEST], 2026-09-15) + 0 * FIX(EQ[DAL196_TEST])", "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) - x"});
    const auto zero = PrepareScript(zeroProduct, &model, BoundSettings(), {});
    AAD::AllocatePath(zero.DefLine(), path);
    AAD::InitializePath(path);
    model.GeneratePath(gauss, &path);
    ASSERT_EQ(path[0].observations_[0], 120.0);
    ASSERT_EQ(path[1].observations_[0], 999.0);
    ASSERT_EQ(MCDoubleSimulation(zero, &model, 257, "sobol", false, false, true).aggregated_, 0.0);
}

TEST(ScriptObservationSimulationTest, TestKnownFixingStillDiscounts) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2, 0.05));
    const auto result = MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11)"), model, 1, ScriptValuationSettings_());
    const double expected = 80.0 * std::exp(-0.05 * 10.0 / DAYS_PER_YEAR);
    ASSERT_NEAR(result.aggregated_, expected, 1.0e-12 * expected);
}

TEST(ScriptObservationSimulationTest, TestPastPaysAndSameDateOrdering) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    String_ past = "x = FIX(EQ[DAL196_TEST]) ";
    for (size_t i = 0; i < 1024; ++i)
        past += "pay PAYS 5 ";
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {past, "pay PAYS x"});
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.0));
    ASSERT_EQ(MCSimulation<double>(product, model, 1, ScriptValuationSettings_()).aggregated_, 80.0);
    const ScriptProductData_ ordered("", {Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))},
                                     {"x = 1", "x = x + 1", "pay PAYS x"});
    ASSERT_EQ(MCSimulation<double>(ordered, model, 1, ScriptValuationSettings_()).aggregated_, 2.0);
}

TEST(ScriptObservationSimulationTest, TestTodayZeroDimension) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<ModelData_> model(new BSModelData_("", 123.0, 0.2));
    for (const String_ method : {"sobol", "mrg32", "irn"})
        for (const bool bb : {false, true}) {
            MonteCarloSettings_ simulation;
            simulation.rsg_ = method;
            simulation.useBb_ = bb;
            ASSERT_EQ(
                MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 12)), model, 1, BoundSettings(), simulation).aggregated_,
                123.0);
        }
}

TEST(ScriptObservationSimulationTest, TestExpiredConfigurationAndNoWork) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 11));
    const Handle_<ModelData_> modelData(new BSModelData_("", 100.0, 0.2));
    MonteCarloSettings_ invalid;
    invalid.rsg_ = "unknown";
    ASSERT_THROW(MCSimulation<double>(product, modelData, 1, ScriptValuationSettings_(), invalid), Exception_);
    ASSERT_THROW(MCSimulation<double>(product, modelData, 0, ScriptValuationSettings_()), Exception_);
    ControlledModel_ model;
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reject);
    const auto prepared = PrepareScript(product, &model, {}, {});
    ASSERT_EQ(MCDoubleSimulation(prepared, &model, 8193, "sobol", false, false, true).aggregated_, 0.0);
    ASSERT_EQ(model.allocations_, 0);
    ASSERT_EQ(model.paths_->load(), 0);
    ASSERT_EQ(workers.submissions_, 0);
    const auto aad = MCSimulation<AAD::Number_>(product, modelData, 8193, ScriptValuationSettings_());
    ASSERT_EQ(aad.aggregated_, 0.0);
    for (const auto risk : aad.risks_)
        ASSERT_EQ(risk, 0.0);
}

TEST(ScriptObservationSimulationTest, TestInvalidModelBeforeHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    ReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
    for (double spot : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        const Handle_<ModelData_> model(new BSModelData_("", spot, 0.2));
        ASSERT_THROW(MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11)"), model, 1, ScriptValuationSettings_()), Exception_);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
    }
}

TEST(ScriptObservationSimulationTest, TestDupireOutputCapability) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    AAD::Dupire_<double> model(120.0, 0.0, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.0));
    const auto prepared = PrepareScript(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"), &model, BoundSettings(), {});
    ASSERT_NEAR(MCDoubleSimulation(prepared, &model, 1, "sobol", false, false, true).aggregated_, 120.0, 120.0e-12);
}

TEST(ScriptObservationSimulationTest, TestModelBindingsBeforeHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    const Handle_<ModelData_> model(new BSModelData_("EQ[DAL196_TEST]", 123.0, 0.2));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
    for (const Vector_<ModelIndexBinding_> bindings : {Vector_<ModelIndexBinding_>(),
                                                       Vector_<ModelIndexBinding_>{{"spot", "EQ[OTHER]"}},
                                                       {{"other", "EQ[DAL196_TEST]"}},
                                                       {{"spot", "EQ[DAL196_TEST]"}, {"spot", "EQ[DAL196_TEST]"}},
                                                       {{"spot", "FX[EUR/USD]"}}}) {
        ScriptValuationSettings_ settings;
        settings.modelBindings_ = bindings;
        ASSERT_THROW(MCSimulation<double>(product, model, 1, settings), Exception_);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
        ASSERT_EQ(workers.submissions_, 0);
    }
    for (const String_ index : {"EQ[OTHER]", "FX[EUR/USD]", "EQ[DAL196_TEST]>3M", "EQ[DAL196_TEST]@2026-12-31"}) {
        ASSERT_THROW(
            MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(" + index + ", 2026-09-15)"), model, 1, BoundSettings()),
            Exception_);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
        ASSERT_EQ(workers.submissions_, 0);
    }
}

TEST(ScriptObservationSimulationTest, TestLookAheadAndModeBarriers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<ModelData_> model(new BSModelData_("", 123.0, 0.0));
    RejectHistory_ reject;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reject);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (const auto event : {Date_(2026, 9, 10), Date_(2026, 9, 14)})
        ASSERT_THROW(MCSimulation<double>(Product("IF 1 = 0 THEN pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) ELSE pay PAYS 0 END", event), model, 1,
                                          BoundSettings()),
                     ScriptError_);
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
    MonteCarloSettings_ compiled;
    compiled.compiled_ = true;
    ASSERT_THROW(MCSimulation<double>(product, model, 1, BoundSettings(), compiled), ScriptError_);
    ASSERT_THROW(MCSimulation<AAD::Number_>(product, model, 1, BoundSettings()), ScriptError_);
    ASSERT_EQ(workers.submissions_, 0);
    ASSERT_NEAR(MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 15)), model, 1, BoundSettings()).aggregated_, 123.0,
                123.0e-12);
}

TEST(ScriptObservationSimulationTest, TestDefaultSpotSharesFixing) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"x = SPOT() + FIX(EQ[DAL196_TEST])", "pay PAYS x"});
    ScriptProductSettings_ contract;
    contract.defaultIndex_ = "EQ[DAL196_TEST]";
    ControlledModel_ model;
    ReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const auto prepared = PrepareScript(product, &model, {}, {}, {}, contract);
    ASSERT_EQ(prepared.Plan().Requests().size(), 1);
    ASSERT_EQ(prepared.Plan().Request(0).uses_.size(), 2);
    ASSERT_EQ(reads.fixings_, 1);
    ASSERT_EQ(MCDoubleSimulation(prepared, &model, 1, "sobol", false, false, true).aggregated_, 160.0);
}

TEST(ScriptObservationSimulationTest, TestUnboundAndExpiredSpotGuards) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<ModelData_> model(new BSModelData_("", 123.0, 0.0));
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reject);
    for (const auto& product : {Product("pay PAYS SPOT()", Date_(2026, 9, 11)), Product("pay PAYS SPOT() + FIX(EQ[DAL196_TEST])")})
        ASSERT_THROW(MCSimulation<double>(product, model, 1, BoundSettings()), ScriptError_);
    ScriptValuationSettings_ invalid;
    invalid.modelBindings_ = {{"unknown", "EQ[DAL196_TEST]"}};
    ASSERT_THROW(MCSimulation<double>(Product("pay PAYS 1", Date_(2026, 9, 11)), model, 1, invalid), Exception_);
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        ASSERT_EQ(MCSimulation<double>(Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 11)), model, 1, ScriptValuationSettings_(), simulation)
                      .aggregated_,
                  0.0);
    }
}

TEST(ScriptObservationSimulationTest, TestPathFailureDrainsWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ControlledModel_ model;
    model.badPath_ = true;
    const auto prepared = PrepareScript(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"), &model, BoundSettings(), {});
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&workers);
    ASSERT_THROW(MCDoubleSimulation(prepared, &model, 257, "sobol", false, false, true), Exception_);
    ASSERT_GT(workers.submissions_, 0);
    ASSERT_EQ(model.paths_->load(), workers.submissions_);
    model.badPath_ = false;
    ASSERT_EQ(MCDoubleSimulation(prepared, &model, 1, "sobol", false, false, true).aggregated_, 120.0);
}

TEST(ScriptObservationSimulationTest, TestFiniteModelInitializationBeforeHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    StoreHistory();
    ReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11)");
    for (const auto& model :
         {Handle_<ModelData_>(new BSModelData_("", 100.0, 0.2, 1.0e308)), Handle_<ModelData_>(new BSModelData_("", 100.0, 1.0e308)),
          Handle_<ModelData_>(new DupireModelData_("", -1.0, 0.0, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.2)))}) {
        ASSERT_THROW(MCSimulation<double>(product, model, 1, ScriptValuationSettings_()), Exception_);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
    }
}

TEST(ScriptObservationSimulationTest, TestUniqueHistoryAcrossPathsAndThreads) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ restorePool;
    StoreHistory();
    String_ event = "pay PAYS ";
    for (size_t use = 0; use < 100; ++use)
        event += (use ? "+" : "") + String_("FIX(EQ[DAL196_TEST], 2026-09-11)");
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))}, {event, event, event});
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    for (const size_t threads : {1, 2, 4}) {
        restorePool.pool_->Start(threads, true);
        for (const size_t paths : {1, 257, 8193}) {
            ReadCounter_ reads;
            const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
            ASSERT_EQ(MCSimulation<double>(product, model, paths, ScriptValuationSettings_()).aggregated_, paths * 16000.0);
            ASSERT_EQ(reads.histories_, 1);
            ASSERT_EQ(reads.fixings_, 1);
        }
    }
}

TEST(ScriptObservationSimulationTest, TestPreparationValidatesConfiguration) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ControlledModel_ model;
    MonteCarloSettings_ invalid;
    invalid.rsg_ = "unknown";
    ASSERT_THROW(PrepareScript(Product("pay PAYS 1", Date_(2026, 9, 11)), &model, {}, invalid), Exception_);
    ASSERT_EQ(model.allocations_, 0);
}

TEST(ScriptObservationSimulationTest, TestRawHistoricalDeadBranchSpotRejected) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ScriptProduct_ product({Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"IF 1 = 0 THEN x = SPOT() END", "pay PAYS 1"});
    ASSERT_THROW(product.PreProcess(false, true), ScriptError_);
}

TEST(ScriptObservationSimulationTest, TestTodayLegacyModesAndRngBarrier) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<ModelData_> model(new BSModelData_("", 123.0, 0.2));
    ScriptProduct_ product({Cell_(Date_(2026, 9, 12))}, {"pay PAYS SPOT()"});
    product.PreProcess(false, true);
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&workers);
    ASSERT_THROW(MCSimulation<AAD::Number_>(product, model, 1, "unknown"), Exception_);
    ASSERT_EQ(workers.submissions_, 0);
    for (const String_ method : {"sobol", "mrg32", "irn"})
        for (const bool bb : {false, true})
            for (const bool compiled : {false, true}) {
                ASSERT_EQ(MCSimulation<double>(product, model, 1, method, bb, compiled).aggregated_, 123.0);
                const auto aad = MCSimulation<AAD::Number_>(product, model, 1, method, bb, compiled, 0);
                ASSERT_EQ(aad.aggregated_, 123.0);
                ASSERT_NEAR(aad["spot"], 1.0, 1.0e-12);
            }
}

TEST(ScriptObservationSimulationTest, TestRequestedOutputsAreValidatedByAdapter) {
    AAD::BlackScholes_<double> model(100.0, 0.2);
    AAD::SampleDef_ def;
    def.indexNames_ = {"FX[EUR/USD]"};
    ASSERT_THROW(model.Allocate({1.0}, {def}), Exception_);
}

TEST(ScriptObservationSimulationTest, TestHistoricalAadRejected) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ScriptProduct_ product({Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"2", "x = SCALE * 80", "pay PAYS x"});
    product.PreProcess(false, true);
    const Handle_<ModelData_> model(new BSModelData_("", 100.0, 0.2));
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&workers);
    for (const bool compiled : {false, true})
        ASSERT_THROW(MCSimulation<AAD::Number_>(product, model, 1, "sobol", false, compiled, 0), ScriptError_);
    ASSERT_EQ(workers.submissions_, 0);
}

TEST(ScriptObservationSimulationTest, TestHistoricalIndicesAndFrozenSnapshot) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const DateTime_ h(Date_(2026, 9, 11), 0.0);
    const Handle_<MarketFixingSnapshot_> snapshot(
        new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{h, 80.0}}}, {"EQ[OTHER]>3M", {{h, 7.0}}}, {"FX[USD/EUR]", {{h, 0.8}}}}));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[OTHER]>3M, 2026-09-11)"
                                 " + FIX(FX[EUR/USD], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
    ReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
    ControlledModel_ model;
    const auto prepared = PrepareScript(product, &model, BoundSettings(), {}, snapshot);
    ASSERT_EQ(reads.histories_, 0);
    ASSERT_EQ(reads.fixings_, 3);
    StoreHistory();
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ noReads(&reject);
    ASSERT_EQ(MCDoubleSimulation(prepared, &model, 257, "sobol", false, false, true).aggregated_, 257 * 208.25);
}

TEST(ScriptObservationSimulationTest, TestOriginalPreparationBraceDefaults) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("pay PAYS 1");
    ASSERT_FALSE(PrepareScript(product, {}).AllExpired());
    ASSERT_FALSE(PrepareScript(product, {}, {}).AllExpired());
}

TEST(ScriptObservationSimulationTest, TestPaymentNumeraireIsIndependentOfRetainedObservation) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 23))},
                                     {"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)", "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)"});
    PaymentModel_ model;
    const auto prepared = PrepareScript(product, &model, BoundSettings(), {});
    ASSERT_EQ(prepared.Plan().Requests().size(), 1);
    ASSERT_EQ(prepared.Plan().SampleDates().size(), 3);
    for (const size_t paths : {1, 257})
        ASSERT_NEAR(MCDoubleSimulation(prepared, &model, paths, "sobol", false, false, true).aggregated_ / paths, 120.0 / 4.0 + 120.0 / 5.0,
                    54.0e-12);
}

TEST(ScriptObservationSimulationTest, TestFinalHistoryFailureAfterModelSetupSubmitsNoWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL217_MISSING], 2026-09-11)"
                                 " + FIX(EQ[DAL196_TEST], 2026-09-15)");
    ControlledModel_ model;
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    ASSERT_THROW(PrepareScript(product, &model, BoundSettings(), {}, snapshot), ScriptError_);
    ASSERT_EQ(model.allocations_, 1);
    ASSERT_EQ(reads.histories_, 0);
    ASSERT_EQ(reads.fixings_, 2);
    ASSERT_EQ(workers.submissions_, 0);
    ASSERT_EQ(model.paths_->load(), 0);
    const Handle_<ModelData_> modelData(new BSModelData_("", 123.0, 0.2));
    ASSERT_THROW(MCSimulation<double>(product, modelData, 8193, BoundSettings(), {}, snapshot), ScriptError_);
    ASSERT_EQ(reads.fixings_, 4);
    ASSERT_EQ(workers.submissions_, 0);
}

TEST(ScriptObservationSimulationTest, TestModelSetupFailuresPrecedeHistoryAndWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (const bool allocation : {false, true}) {
        SetupFailureModel_ model;
        model.failAllocate_ = allocation;
        ASSERT_THROW(
            PrepareScript(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)"), &model, BoundSettings(), {}),
            Exception_);
        ASSERT_EQ(model.allocations_, 1);
        ASSERT_EQ(model.initializations_, allocation ? 0 : 1);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
        ASSERT_EQ(workers.submissions_, 0);
        ASSERT_EQ(model.paths_->load(), 0);
    }
}

TEST(ScriptObservationSimulationTest, TestFrozenObservationStorageOnWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ restorePool;
    restorePool.pool_->Start(4, true);
    Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
    ControlledModel_ model;
    WorkerStorage_ storage(restorePool.pool_->NumThreads());
    const GuardedPrepared_ prepared(PrepareScript(product, &model, BoundSettings(), {}, snapshot), &storage);
    snapshot = {};
    const auto changedDate = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
    RejectHistory_ reject;
    const Dal::Detail::ScopedFixingReadObserver_ callerGuard(&reject);
    ASSERT_NEAR(MCDoubleSimulation(prepared, &model, 8193, "sobol", false, false, true).aggregated_ / 8193, 200.0, 200.0e-12);
    ASSERT_EQ(Accumulate(storage.evaluations_), 8193);
    ASSERT_EQ(model.allocations_, 1);
    ASSERT_EQ(model.paths_->load(), 8193);
}

TEST(ScriptObservationSimulationTest, TestBothAdaptersRejectUnsupportedBeforeHistory) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    AAD::BlackScholes_<double> bs(123.0, 0.0);
    AAD::Dupire_<double> dupire(123.0, 0.0, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.0));
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (auto* model : {static_cast<AAD::Model_<double>*>(&bs), static_cast<AAD::Model_<double>*>(&dupire)}) {
        const Index::DF_ rate(Ccy_("USD"), Cell_("3M"));
        ASSERT_FALSE(model->SupportsIndex(rate));
        const auto mixed = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
        for (const Vector_<ModelIndexBinding_> bindings : {Vector_<ModelIndexBinding_>(),
                                                           {{"spot", "EQ[OTHER]"}},
                                                           {{"unknown", "EQ[DAL196_TEST]"}},
                                                           {{"spot", "EQ[DAL196_TEST]"}, {"spot", "EQ[DAL196_TEST]"}},
                                                           {{"spot", "EQ[DAL196_TEST]"}, {"spot", "EQ[OTHER]"}}}) {
            ScriptValuationSettings_ settings;
            settings.modelBindings_ = bindings;
            ASSERT_THROW(PrepareScript(mixed, model, settings, {}), ScriptError_);
            ASSERT_EQ(reads.histories_, 0);
            ASSERT_EQ(reads.fixings_, 0);
            ASSERT_EQ(workers.submissions_, 0);
        }
        for (const String_ index : {"EQ[OTHER]", "FX[EUR/USD]", "EQ[DAL196_TEST]>3M", "EQ[DAL196_TEST]@2026-12-31", "IR[DF]:USD,3M",
                                    "COMPOSITE[DAL196_TEST]", "UNREGISTERED[DAL217]"}) {
            ASSERT_THROW(PrepareScript(Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)"
                                               " + FIX(" +
                                               index + ", 2026-09-15)"),
                                       model, BoundSettings(), {}),
                         Exception_);
            ASSERT_EQ(reads.histories_, 0);
            ASSERT_EQ(reads.fixings_, 0);
            ASSERT_EQ(workers.submissions_, 0);
        }
    }
}

TEST(ScriptObservationSimulationTest, TestTimelineValidationAcrossSamplesForBothAdapters) {
    AAD::BlackScholes_<double> bs(123.0, 0.0);
    AAD::Dupire_<double> dupire(123.0, 0.0, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.0));
    for (auto* model : {static_cast<AAD::Model_<double>*>(&bs), static_cast<AAD::Model_<double>*>(&dupire)}) {
        Vector_<AAD::SampleDef_> definitions(3);
        definitions[0].indexNames_ = {"EQ[DAL196_TEST]"};
        definitions[2].indexNames_ = {"EQ[OTHER]"};
        ASSERT_THROW(model->Allocate({0.0, 0.5, 1.0}, definitions), Exception_);
        definitions[2].indexNames_ = {"EQ[DAL196_TEST]", "EQ[DAL196_TEST]"};
        ASSERT_THROW(model->Allocate({0.0, 0.5, 1.0}, definitions), Exception_);
        definitions[2].indexNames_ = {"EQ[DAL196_TEST]"};
        for (const Vector_<> times : {Vector_<>{0.0, 0.5},
                                      {0.0, 0.5, 0.5},
                                      {0.0, 1.0, 0.5},
                                      {-0.1, 0.5, 1.0},
                                      {0.0, 0.5, std::numeric_limits<double>::quiet_NaN()},
                                      {0.0, 0.5, std::numeric_limits<double>::infinity()}})
            ASSERT_THROW(model->Allocate(times, definitions), Exception_);
        model->Allocate({0.0, 0.5, 1.0}, definitions);
        model->Init({0.0, 0.5, 1.0}, definitions);
        AAD::Scenario_<double> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        model->GeneratePath(Vector_<>(model->SimDim(), 0.0), &path);
        ASSERT_NEAR(path[0].observations_[0], 123.0, 123.0e-12);
        ASSERT_TRUE(path[1].observations_.empty());
        ASSERT_NEAR(path[2].observations_[0], 123.0, 123.0e-12);
    }
}

TEST(ScriptObservationSimulationTest, TestTodayPolicyAndRngForBothAdapters) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 12));
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 12), 0.0), 80.0}}}}));
    for (const auto& model : {Handle_<ModelData_>(new BSModelData_("", 123.0, 0.2)),
                              Handle_<ModelData_>(new DupireModelData_("", 123.0, 0.0, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.2)))})
        for (const bool historical : {false, true}) {
            auto settings = BoundSettings();
            if (historical)
                settings.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
            for (const String_ rng : {"sobol", "mrg32", "irn"})
                for (const bool bb : {false, true}) {
                    MonteCarloSettings_ simulation;
                    simulation.rsg_ = rng;
                    simulation.useBb_ = bb;
                    ASSERT_NEAR(MCSimulation<double>(product, model, 1, settings, simulation, snapshot).aggregated_, historical ? 80.0 : 123.0,
                                123.0e-12);
                }
        }
}

TEST(ScriptObservationSimulationTest, TestFutureDefaultSpotAndFixShareModelCell) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ControlledModel_ model;
    ScriptProductSettings_ contract;
    contract.defaultIndex_ = "EQ[DAL196_TEST]";
    const auto prepared =
        PrepareScript(Product("pay PAYS SPOT() - FIX(EQ[DAL196_TEST])", Date_(2026, 9, 15)), &model, BoundSettings(), {}, {}, contract);
    ASSERT_EQ(prepared.Plan().Requests().size(), 1);
    ASSERT_EQ(prepared.Plan().Request(0).uses_.size(), 2);
    ASSERT_EQ(prepared.DefLine()[0].indexNames_.size(), 1);
    ASSERT_DOUBLE_EQ(MCDoubleSimulation(prepared, &model, 257, "sobol", false, false, true).aggregated_, 0.0);
}

TEST(ScriptObservationSimulationTest, TestNonFiniteHistoryInDeadBranchFailsBeforeWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const HistoryRestore_ restoreHistory("EQ[DAL217_NONFINITE]");
    const Handle_<ModelData_> model(new BSModelData_("", 123.0, 0.2));
    const auto product = Product("IF 1 = 0 THEN pay PAYS FIX(EQ[DAL217_NONFINITE], 2026-09-11) ELSE pay PAYS 7 END");
    SubmissionCounter_ workers;
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        FixHistory_ history;
        history.vals_ = {{DateTime_(Date_(2026, 9, 11), 0.0), value}};
        XGLOBAL::StoreFixings("EQ[DAL217_NONFINITE]", history, false);
        ASSERT_THROW(MCSimulation<double>(product, model, 8193, ScriptValuationSettings_()), ScriptError_);
        ASSERT_EQ(workers.submissions_, 0);
    }
}

TEST(ScriptObservationSimulationTest, TestDeadBranchPathAndPayoffFailuresDrainEveryBatch) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ restorePool;
    for (const size_t threads : {1, 2, 4}) {
        restorePool.pool_->Start(threads, true);
        for (const bool badPath : {false, true}) {
            ControlledModel_ model;
            model.badPath_ = badPath;
            const auto product = Product(badPath ? "IF 1 = 0 THEN pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) ELSE pay PAYS 7 END"
                                                 : "pay PAYS LOG(-1) + FIX(EQ[DAL196_TEST], 2026-09-15)");
            const auto prepared = PrepareScript(product, &model, BoundSettings(), {});
            SubmissionCounter_ workers;
            const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
            ASSERT_THROW(MCDoubleSimulation(prepared, &model, 16385, "sobol", false, false, true), ScriptError_);
            ASSERT_EQ(workers.submissions_, BatchPlan_(16385, threads).BatchCount());
            ASSERT_EQ(model.paths_->load(), workers.submissions_);
        }
    }
}

TEST(ScriptObservationSimulationTest, TestNamedFuzzyAndCompiledRejectBeforeModelSetup) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = Product("IF 1 = 0 THEN pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) ELSE pay PAYS 7 END");
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (const bool compiled : {false, true}) {
        ControlledModel_ model;
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = !compiled;
        simulation.compiled_ = compiled;
        ASSERT_THROW(PrepareScript(product, &model, BoundSettings(), simulation), ScriptError_);
        ASSERT_EQ(model.allocations_, 0);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 0);
        ASSERT_EQ(workers.submissions_, 0);
    }
}

TEST(ScriptObservationSimulationTest, TestNamedAndLegacyFixedPathParityAcrossAdapters) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ restorePool;
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    const auto named = Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)");
    ScriptProduct_ legacy({Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))}, {"x = SPOT()", "pay PAYS 80 + x"});
    legacy.PreProcess(false, true);
    for (const auto& model : {Handle_<ModelData_>(new BSModelData_("", 120.0, 0.2, 0.05)),
                              Handle_<ModelData_>(new DupireModelData_("", 120.0, 0.05, 0.0, {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.2)))})
        for (const size_t threads : {1, 4}) {
            restorePool.pool_->Start(threads, true);
            for (const String_ rng : {"sobol", "mrg32", "irn"})
                for (const bool bb : {false, true}) {
                    MonteCarloSettings_ simulation;
                    simulation.rsg_ = rng;
                    simulation.useBb_ = bb;
                    const double actual = MCSimulation<double>(named, model, 8193, BoundSettings(), simulation, snapshot).aggregated_ / 8193;
                    for (const bool compiled : {false, true}) {
                        const double expected = MCSimulation<double>(legacy, model, 8193, rng, bb, compiled).aggregated_ / 8193;
                        ASSERT_NEAR(actual, expected, 1.0e-8);
                    }
                }
        }
}

TEST(ScriptObservationSimulationTest, TestInvalidStructureAndTodayRngSubmitNoWorkers) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ empty("", {}, {});
    const ScriptProductData_ definitions("", {Cell_("SCALE")}, {"2"});
    const auto noPayoff = Product("x = 1");
    ControlledModel_ model;
    ReadCounter_ reads;
    SubmissionCounter_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
    const Dal::Script::Detail::ScopedSimulationObserver_ submissions(&workers);
    for (const auto* product : {&empty, &definitions, &noPayoff})
        ASSERT_THROW(PrepareScript(*product, &model, BoundSettings(), {}), ScriptError_);
    MonteCarloSettings_ simulation;
    simulation.rsg_ = "unknown";
    ASSERT_THROW(PrepareScript(Product("pay PAYS FIX(EQ[DAL196_TEST])", Date_(2026, 9, 12)), &model, BoundSettings(), simulation), ScriptError_);
    ASSERT_EQ(model.allocations_, 0);
    ASSERT_EQ(reads.histories_, 0);
    ASSERT_EQ(reads.fixings_, 0);
    ASSERT_EQ(workers.submissions_, 0);
}

TEST(ScriptObservationSimulationTest, TestControlledSinglePathHistoricalOracles) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Handle_<MarketFixingSnapshot_> snapshot(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    for (const bool future : {false, true}) {
        ControlledModel_ model;
        ReadCounter_ reads;
        const Dal::Detail::ScopedFixingReadObserver_ history(&reads);
        const auto product =
            Product("pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], " + String_(future ? "2026-09-15)" : "2026-09-11)"));
        const auto prepared = PrepareScript(product, &model, BoundSettings(), {}, snapshot);
        ASSERT_EQ(reads.histories_, 0);
        ASSERT_EQ(reads.fixings_, 1);
        ASSERT_NEAR(MCDoubleSimulation(prepared, &model, 1, "sobol", false, false, true).aggregated_, future ? 200.0 : 160.0, 160.0e-12);
        ASSERT_EQ(model.paths_->load(), 1);
        ASSERT_EQ(reads.fixings_, 1);
    }
}
