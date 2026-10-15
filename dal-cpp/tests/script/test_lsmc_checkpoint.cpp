//
// Created by Codex on 2026/10/16.
//

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;
using Dal::Script::LsmcDiagnostics_;
using Dal::Script::MonteCarloSettings_;
using Dal::Script::ScriptProductData_;

namespace {
    uint64_t CheckpointBits(double value) {
        uint64_t result;
        std::memcpy(&result, &value, sizeof(value));
        return result;
    }

    struct CheckpointThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t previous_ = pool_->NumThreads();
        CheckpointThreads_() { pool_->Start(1, true); }
        ~CheckpointThreads_() { pool_->Start(previous_, true); }
    };

    ScriptProductData_ CheckpointProduct(bool growing = false, size_t events = 65, bool fixing = false) {
        Dal::Script::ScriptProductSettings_ settings;
        settings.regressionFeatures_ = {"VAR[s]", "VAR[avg]", "VAR[peak]"};
        if (fixing)
            settings.defaultIndex_ = "EQ[CHECKPOINT]";
        Vector_<Cell_> dates{Cell_("K"), Cell_(Date_(2023, 12, 31))};
        Vector_<String_> scripts{"100", "total = K n = 1 peak = K APPEND(history, K)"};
        if (fixing)
            scripts[1] = "seed = K * FIX(EQ[CHECKPOINT], 2023-12-31) / 100 total = seed n = 1 peak = seed APPEND(history, seed)";
        for (size_t e = 0; e < events; ++e) {
            dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(static_cast<int>(7 * (e + 1)))));
            String_ script = "s = SPOT() total = total + s n = n + 1 peak = MAX(peak, s) ";
            script += growing ? "APPEND(history, s) avg = AVERAGE(history) " : "avg = (total + SUM(history)) / (n + 1) ";
            script += "pay PAYS 0.001 * avg ";
            script += e + 1 < events ? "EXERCISE MAX(K - avg, 0) IF s < 125" : "pay PAYS MAX(K - avg, 0)";
            scripts.push_back(script);
        }
        return {"checkpoint production", dates, scripts, settings};
    }

    MonteCarloSettings_ CheckpointSettings() {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        simulation.smooth_ = 2.0;
        simulation.lsmcBasisDegree_ = 2;
        simulation.lsmcTrainingPaths_ = 257;
        simulation.lsmcValidationPaths_ = 73;
        simulation.lsmcRqmcReplicates_ = 2;
        simulation.lsmcTrainingSeed_ = 17;
        simulation.lsmcPricingSeed_ = 29;
        return simulation;
    }

    Dal::Script::PreparedScript_ CheckpointPrepare(const ScriptProductData_& product,
                                                   Dal::AAD::Model_<double>* model,
                                                   const MonteCarloSettings_& simulation,
                                                   const Dal::Handle_<Dal::MarketFixingSnapshot_>& fixings = {}) {
        Dal::Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2024, 1, 1);
        valuation.fixings_ = fixings;
        auto prepared = Dal::Script::PrepareScript(product, model, valuation, simulation);
        model->Allocate(prepared.TimeLine(), prepared.DefLine());
        model->Init(prepared.TimeLine(), prepared.DefLine());
        return prepared;
    }

    void AssertCheckpointPolicies(const LsmcDiagnostics_& actual, const LsmcDiagnostics_& expected) {
        ASSERT_EQ(actual.events_.size(), expected.events_.size());
        ASSERT_EQ(actual.payoffSum_, expected.payoffSum_);
        ASSERT_EQ(actual.payoffSumSq_, expected.payoffSumSq_);
        for (size_t day = 0; day < actual.events_.size(); ++day) {
            const auto& a = actual.events_[day];
            const auto& b = expected.events_[day];
            ASSERT_EQ(a.eventId_, b.eventId_);
            ASSERT_EQ(a.coefficients_, b.coefficients_);
            ASSERT_EQ(a.normalizationMeans_, b.normalizationMeans_);
            ASSERT_EQ(a.normalizationSigmas_, b.normalizationSigmas_);
            ASSERT_EQ(a.basisPowers_, b.basisPowers_);
            ASSERT_EQ(a.numCondTruePaths_, b.numCondTruePaths_);
            ASSERT_EQ(a.validationMse_, b.validationMse_);
            ASSERT_EQ(a.exerciseRate_, b.exerciseRate_);
            ASSERT_EQ(a.basisDegree_, b.basisDegree_);
            ASSERT_EQ(a.solver_, b.solver_);
        }
    }
} // namespace

TEST(LsmcCheckpointTest, TestBlockTailPendingHistoryAndPolicyParity) {
    CheckpointThreads_ threads;
    const auto product = CheckpointProduct();
    for (bool bridge : {false, true}) {
        auto simulation = CheckpointSettings();
        simulation.useBb_ = bridge;
        Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
        const auto full = CheckpointPrepare(product, &model, simulation);
        LsmcDiagnostics_ baseline;
        const auto reference = Dal::Script::MCLsmcSimulation(full, &model, 129, &baseline);
        for (int block : {1, 4, 8, 65, 79}) {
            simulation.lsmcCheckpointBlockEvents_ = block;
            simulation.lsmcTrainingMemoryMb_ = 64;
            const auto prepared = CheckpointPrepare(product, &model, simulation);
            LsmcDiagnostics_ actual;
            const auto result = Dal::Script::MCLsmcSimulation(prepared, &model, 129, &actual);
            ASSERT_EQ(result.aggregated_, reference.aggregated_);
            AssertCheckpointPolicies(actual, baseline);
            ASSERT_LE(actual.trainingAllocationPeak_, actual.trainingAllocationBound_);
            ASSERT_LE(actual.trainingAllocationBound_, size_t{64} * 1024 * 1024);
            const size_t visits = actual.checkpointTraining_ ? 1 + (65 + static_cast<size_t>(block) - 1) / static_cast<size_t>(block) : 1;
            ASSERT_EQ(actual.trainingNormalVectors_, 330 * visits);
            ASSERT_EQ(actual.trainingModelSteps_, 330 * 65 * (actual.checkpointTraining_ ? 2 : 1));
            ASSERT_EQ(actual.trainingScriptEvents_, actual.trainingModelSteps_);
            if (block == 8)
                ASSERT_TRUE(actual.checkpointTraining_);
            if (block >= 65)
                ASSERT_FALSE(actual.checkpointTraining_);
        }
    }
}

TEST(LsmcCheckpointTest, TestFrozenAndRetrainedEveryNamedRiskParity) {
    CheckpointThreads_ threads;
    const auto product = CheckpointProduct();
    Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100, 0.2, 0.05, 0.02));
    for (const String_ mode : {"Frozen", "RetrainedBump"}) {
        auto simulation = CheckpointSettings();
        simulation.enableAad_ = true;
        simulation.useBb_ = true;
        simulation.lsmcPolicyRiskMode_ = mode;
        Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
        const auto full = CheckpointPrepare(product, &model, simulation);
        const auto expected = Dal::Script::MCLsmcAadSimulation(full, data, 129);
        simulation.lsmcCheckpointBlockEvents_ = 8;
        simulation.lsmcTrainingMemoryMb_ = 64;
        const auto prepared = CheckpointPrepare(product, &model, simulation);
        const auto actual = Dal::Script::MCLsmcAadSimulation(prepared, data, 129);
        ASSERT_EQ(CheckpointBits(actual.aggregated_), CheckpointBits(expected.aggregated_));
        ASSERT_EQ(actual.names_, expected.names_);
        ASSERT_EQ(actual.names_, (Vector_<String_>{"spot", "vol", "rate", "div", "K"}));
        ASSERT_EQ(actual.risks_.size(), 5);
        for (size_t j = 0; j < actual.risks_.size(); ++j)
            ASSERT_EQ(CheckpointBits(actual.risks_[j]), CheckpointBits(expected.risks_[j])) << actual.names_[j];
    }
}

TEST(LsmcCheckpointTest, TestGrowingStateFallsBackAndBudgetFailureRecovers) {
    CheckpointThreads_ threads;
    auto simulation = CheckpointSettings();
    Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
    const auto product = CheckpointProduct(true);
    const auto full = CheckpointPrepare(product, &model, simulation);
    LsmcDiagnostics_ baseline;
    const auto expected = Dal::Script::MCLsmcSimulation(full, &model, 17, &baseline);
    simulation.lsmcCheckpointBlockEvents_ = 8;
    simulation.lsmcTrainingMemoryMb_ = 64;
    const auto fallback = CheckpointPrepare(product, &model, simulation);
    LsmcDiagnostics_ diagnostics;
    ASSERT_EQ(Dal::Script::MCLsmcSimulation(fallback, &model, 17, &diagnostics).aggregated_, expected.aggregated_);
    ASSERT_FALSE(diagnostics.checkpointTraining_);
    AssertCheckpointPolicies(diagnostics, baseline);
    simulation.lsmcTrainingPaths_ = 8193;
    simulation.lsmcTrainingMemoryMb_ = 1;
    const auto rejected = CheckpointPrepare(product, &model, simulation);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(rejected, &model, 17), Dal::ScriptError_);
    ASSERT_EQ(Dal::Script::MCLsmcSimulation(fallback, &model, 17).aggregated_, expected.aggregated_);
    Dal::AAD::BlackScholes_<double> invalidPath(1e308, 0.2, 10.0, 0.02);
    simulation.lsmcTrainingPaths_ = 257;
    simulation.lsmcTrainingMemoryMb_ = 64;
    const auto failing = CheckpointPrepare(CheckpointProduct(), &invalidPath, simulation);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(failing, &invalidPath, 17), Dal::Exception_);
    ASSERT_EQ(Dal::Script::MCLsmcSimulation(fallback, &model, 17).aggregated_, expected.aggregated_);
}

namespace {
    void AssertHistoricalProgramBudget() {
        auto simulation = CheckpointSettings();
        simulation.lsmcValidationPaths_.reset();
        simulation.lsmcCheckpointBlockEvents_ = 8;
        simulation.lsmcTrainingMemoryMb_ = 1;
        Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
        const auto product = [](size_t increments) {
            String_ history = "seed = K ";
            for (size_t i = 0; i < increments; ++i)
                history += "seed = seed + K ";
            Vector_<Cell_> dates{Cell_("K"), Cell_(Date_(2023, 12, 31))};
            Vector_<String_> scripts{"100", history};
            for (int i = 0; i < 65; ++i) {
                dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(7 * (i + 1))));
                scripts.push_back(i < 64 ? "EXERCISE MAX(K + seed * 0.000001 - SPOT(), 0)" : "pay PAYS MAX(K + seed * 0.000001 - SPOT(), 0)");
            }
            return ScriptProductData_("historical bytecode budget", dates, scripts);
        };
        const auto shortHistory = CheckpointPrepare(product(0), &model, simulation);
        const auto longProduct = product(100000);
        const auto longHistory = CheckpointPrepare(longProduct, &model, simulation);
        ASSERT_EQ(shortHistory.Product().VarNames(), longHistory.Product().VarNames());
        ASSERT_EQ(shortHistory.ConstVarNames(), longHistory.ConstVarNames());
        ASSERT_EQ(shortHistory.TimeLine(), longHistory.TimeLine());
        const auto programBytes = [](const Dal::Script::ScriptCompiled_& program) {
            size_t bytes = program.NodeStreams().capacity() * sizeof(Vector_<int>) + program.ConstStreams().capacity() * sizeof(Vector_<>);
            for (const auto& row : program.NodeStreams())
                bytes += row.capacity() * sizeof(int);
            for (const auto& row : program.ConstStreams())
                bytes += row.capacity() * sizeof(double);
            return bytes;
        };
        const size_t historyGrowth = programBytes(longHistory.HistoricalCompiledProgram()) - programBytes(shortHistory.HistoricalCompiledProgram());
        ASSERT_GT(historyGrowth, size_t{1024} * 1024);
        LsmcDiagnostics_ shortDiagnostics;
        const auto shortPrice = Dal::Script::MCLsmcSimulation(shortHistory, &model, 17, &shortDiagnostics).aggregated_;
        ASSERT_LE(shortDiagnostics.trainingAllocationBound_, size_t{1024} * 1024);
        ASSERT_THROW(Dal::Script::MCLsmcSimulation(longHistory, &model, 17), Dal::ScriptError_);
        ASSERT_EQ(Dal::Script::MCLsmcSimulation(shortHistory, &model, 17).aggregated_, shortPrice);

        simulation.lsmcTrainingMemoryMb_ = 64;
        const auto retry = CheckpointPrepare(longProduct, &model, simulation);
        LsmcDiagnostics_ diagnostics;
        const auto recovered = Dal::Script::MCLsmcSimulation(retry, &model, 17, &diagnostics);
        ASSERT_GT(diagnostics.trainingAllocationBound_, shortDiagnostics.trainingAllocationBound_ + size_t{1024} * 1024);
        ASSERT_EQ(diagnostics.trainingAllocationBound_ - shortDiagnostics.trainingAllocationBound_, historyGrowth);
        ASSERT_LE(diagnostics.trainingAllocationPeak_, diagnostics.trainingAllocationBound_);
        ::testing::Test::RecordProperty("historical_capacity_growth_bytes", std::to_string(historyGrowth));
        ::testing::Test::RecordProperty("short_history_bound_bytes", std::to_string(shortDiagnostics.trainingAllocationBound_));
        ::testing::Test::RecordProperty("long_history_bound_bytes", std::to_string(diagnostics.trainingAllocationBound_));
        ::testing::Test::RecordProperty("long_history_peak_bytes", std::to_string(diagnostics.trainingAllocationPeak_));
        simulation.lsmcCheckpointBlockEvents_.reset();
        simulation.lsmcTrainingMemoryMb_.reset();
        const auto full = CheckpointPrepare(longProduct, &model, simulation);
        LsmcDiagnostics_ baseline;
        ASSERT_EQ(recovered.aggregated_, Dal::Script::MCLsmcSimulation(full, &model, 17, &baseline).aggregated_);
        AssertCheckpointPolicies(diagnostics, baseline);

        const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.05, 0.02));
        for (const String_ mode : {"Frozen", "RetrainedBump"}) {
            simulation.enableAad_ = true;
            simulation.lsmcPolicyRiskMode_ = mode;
            const auto reference = CheckpointPrepare(longProduct, &model, simulation);
            const auto expected = Dal::Script::MCLsmcAadSimulation(reference, data, 17);
            simulation.lsmcCheckpointBlockEvents_ = 8;
            simulation.lsmcTrainingMemoryMb_ = 64;
            const auto budgeted = CheckpointPrepare(longProduct, &model, simulation);
            const auto actual = Dal::Script::MCLsmcAadSimulation(budgeted, data, 17);
            ASSERT_EQ(CheckpointBits(actual.aggregated_), CheckpointBits(expected.aggregated_));
            ASSERT_EQ(actual.names_, (Vector_<String_>{"spot", "vol", "rate", "div", "K"}));
            ASSERT_EQ(actual.names_, expected.names_);
            ASSERT_EQ(actual.risks_.size(), expected.risks_.size());
            for (size_t j = 0; j < actual.risks_.size(); ++j)
                ASSERT_EQ(CheckpointBits(actual.risks_[j]), CheckpointBits(expected.risks_[j])) << actual.names_[j];
            simulation.lsmcCheckpointBlockEvents_.reset();
            simulation.lsmcTrainingMemoryMb_.reset();
        }
    }
} // namespace

TEST(LsmcCheckpointTest, TestHistoricalProgramCapacityIsBudgetedAndRejectionRecovers) {
    CheckpointThreads_ threads;
    std::exception_ptr failure;
    // Admission includes retained tape, so the 1 MiB control needs a fresh worker.
    std::thread worker([&] {
        try {
            AssertHistoricalProgramBudget();
        } catch (...) {
            failure = std::current_exception();
        }
    });
    worker.join();
    if (failure)
        std::rethrow_exception(failure);
}

TEST(LsmcCheckpointTest, TestTwoSidedFitDoesNotRequireUnusedBumpsAtBumpedPoints) {
    CheckpointThreads_ threads;
    auto simulation = CheckpointSettings();
    simulation.enableAad_ = true;
    simulation.lsmcPolicyRiskMode_ = "RetrainedBump";
    Dal::AAD::BlackScholes_<double> model(100.0, 0.0015, 0.05, 0.02);
    const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100.0, 0.0015, 0.05, 0.02));
    const auto full = CheckpointPrepare(CheckpointProduct(), &model, simulation);
    const auto expected = Dal::Script::MCLsmcAadSimulation(full, data, 17);
    simulation.lsmcCheckpointBlockEvents_ = 8;
    simulation.lsmcTrainingMemoryMb_ = 64;
    const auto prepared = CheckpointPrepare(CheckpointProduct(), &model, simulation);
    const auto actual = Dal::Script::MCLsmcAadSimulation(prepared, data, 17);
    ASSERT_EQ(CheckpointBits(actual.aggregated_), CheckpointBits(expected.aggregated_));
    for (size_t j = 0; j < expected.risks_.size(); ++j)
        ASSERT_EQ(CheckpointBits(actual.risks_[j]), CheckpointBits(expected.risks_[j])) << actual.names_[j];
}

TEST(LsmcCheckpointTest, TestRetrainedHistoricalFixingAndConstantSeedParity) {
    CheckpointThreads_ threads;
    auto simulation = CheckpointSettings();
    simulation.enableAad_ = true;
    simulation.lsmcPolicyRiskMode_ = "RetrainedBump";
    Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
    const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.05, 0.02));
    const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(
        new Dal::MarketFixingSnapshot_({{"EQ[CHECKPOINT]", {{Dal::DateTime_(Date_(2023, 12, 31), 0.0), 100.0}}}}));
    const auto product = CheckpointProduct(false, 65, true);
    const auto full = CheckpointPrepare(product, &model, simulation, fixings);
    const auto expected = Dal::Script::MCLsmcAadSimulation(full, data, 17);
    simulation.lsmcCheckpointBlockEvents_ = 8;
    simulation.lsmcTrainingMemoryMb_ = 64;
    const auto prepared = CheckpointPrepare(product, &model, simulation, fixings);
    const auto actual = Dal::Script::MCLsmcAadSimulation(prepared, data, 17);
    ASSERT_EQ(CheckpointBits(actual.aggregated_), CheckpointBits(expected.aggregated_));
    for (size_t j = 0; j < expected.risks_.size(); ++j)
        ASSERT_EQ(CheckpointBits(actual.risks_[j]), CheckpointBits(expected.risks_[j])) << actual.names_[j];
}

TEST(LsmcCheckpointTest, TestRetainedPricingTapeCanRejectBumpedFitAndRecover) {
    CheckpointThreads_ threads;
    bool rejected = false, recovered = false, pricedBeforeRejection = false;
    std::exception_ptr failure;
    std::thread worker([&] {
        try {
            auto simulation = CheckpointSettings();
            simulation.enableAad_ = true;
            simulation.lsmcPolicyRiskMode_ = "RetrainedBump";
            simulation.lsmcCheckpointBlockEvents_ = 8;
            simulation.lsmcTrainingMemoryMb_ = 2;
            Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
            const Dal::Handle_<Dal::ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.05, 0.02));
            const auto limited = CheckpointPrepare(CheckpointProduct(), &model, simulation);
            try {
                static_cast<void>(Dal::Script::MCLsmcAadSimulation(limited, data, 17));
            } catch (const Dal::ScriptError_&) {
                rejected = true;
                pricedBeforeRejection = Dal::AAD::NativeThread::retainedTape != nullptr;
            }
            simulation.lsmcTrainingMemoryMb_ = 64;
            const auto retry = CheckpointPrepare(CheckpointProduct(), &model, simulation);
            recovered = std::isfinite(Dal::Script::MCLsmcAadSimulation(retry, data, 17).aggregated_);
        } catch (...) {
            failure = std::current_exception();
        }
    });
    worker.join();
    if (failure)
        std::rethrow_exception(failure);
    ASSERT_TRUE(rejected);
    ASSERT_TRUE(pricedBeforeRejection);
    ASSERT_TRUE(recovered);
}

TEST(LsmcCheckpointTest, TestSettingsAndUnsupportedGridRejectExplicitly) {
    auto simulation = CheckpointSettings();
    simulation.lsmcCheckpointBlockEvents_ = 8;
    ASSERT_THROW(Dal::Script::ValidateSimulationSettings(simulation), Dal::ScriptError_);
    simulation.lsmcTrainingMemoryMb_ = 64;
    ASSERT_NO_THROW(Dal::Script::ValidateSimulationSettings(simulation));
    simulation.lsmcCheckpointBlockEvents_ = 0;
    ASSERT_THROW(Dal::Script::ValidateSimulationSettings(simulation), Dal::ScriptError_);
    simulation.lsmcCheckpointBlockEvents_ = 8;
    CheckpointThreads_ threads;
    Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
    simulation.compiled_ = false;
    const auto prepared = CheckpointPrepare(CheckpointProduct(), &model, simulation);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(prepared, &model, 17), Dal::ScriptError_);
    simulation.compiled_ = true;
    const ScriptProductData_ today("today", {Cell_(Date_(2024, 1, 1)), Cell_(Date_(2024, 1, 8))}, {"pay PAYS SPOT()", "EXERCISE 1"});
    const auto zeroTime = CheckpointPrepare(today, &model, simulation);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(zeroTime, &model, 17), Dal::ScriptError_);
    Dal::Script::ScriptProductSettings_ contract;
    contract.defaultIndex_ = "EQ[CHECKPOINT]";
    const ScriptProductData_ retained("retained", {Cell_(Date_(2024, 1, 8)), Cell_(Date_(2024, 1, 15))},
                                      {"EXERCISE MAX(100 - SPOT(), 0)", "pay PAYS FIX(EQ[CHECKPOINT], 2024-01-08)"}, contract);
    const auto futureFixing = CheckpointPrepare(retained, &model, simulation);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(futureFixing, &model, 17), Dal::ScriptError_);
    const auto oneWorker = CheckpointPrepare(CheckpointProduct(), &model, simulation);
    threads.pool_->Start(2, true);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(oneWorker, &model, 17), Dal::ScriptError_);
    threads.pool_->Start(1, true);
    simulation.enableAad_ = true;
    simulation.lsmcPolicyRiskMode_ = "RetrainedBump";
    const auto constrained = CheckpointPrepare(CheckpointProduct(), &model, simulation);
    const Dal::Handle_<Dal::ModelData_> zeroVol(new Dal::BSModelData_("", 100.0, 0.0, 0.05, 0.02));
    ASSERT_THROW(Dal::Script::MCLsmcAadSimulation(constrained, zeroVol, 17), Dal::ScriptError_);
}

TEST(LsmcCheckpointTest, TestScalarFitAcrossNativePathChunkWithoutValidation) {
    CheckpointThreads_ threads;
    Vector_<Cell_> dates;
    Vector_<String_> scripts;
    for (int e = 0; e < 65; ++e) {
        dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(7 * (e + 1))));
        scripts.push_back(e < 64 ? "pay PAYS 0.01 EXERCISE MAX(100 - SPOT(), 0)" : "pay PAYS MAX(100 - SPOT(), 0)");
    }
    const ScriptProductData_ product("chunk boundary", dates, scripts);
    auto simulation = CheckpointSettings();
    simulation.lsmcTrainingPaths_ = 8193;
    simulation.lsmcValidationPaths_.reset();
    simulation.lsmcBasisDegree_ = 3;
    Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
    const auto full = CheckpointPrepare(product, &model, simulation);
    LsmcDiagnostics_ baseline;
    const auto expected = Dal::Script::MCLsmcSimulation(full, &model, 17, &baseline);
    simulation.lsmcCheckpointBlockEvents_ = 8;
    simulation.lsmcTrainingMemoryMb_ = 64;
    const auto prepared = CheckpointPrepare(product, &model, simulation);
    LsmcDiagnostics_ actual;
    ASSERT_EQ(Dal::Script::MCLsmcSimulation(prepared, &model, 17, &actual).aggregated_, expected.aggregated_);
    ASSERT_TRUE(actual.checkpointTraining_);
    AssertCheckpointPolicies(actual, baseline);
}
