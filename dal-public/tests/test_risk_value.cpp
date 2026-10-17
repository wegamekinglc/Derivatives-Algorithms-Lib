//
// Created by Codex on 2026/10/4.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/storage/globals.hpp>

#include <script_test_observers.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;

namespace {
    struct ScopedThreads_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit ScopedThreads_(size_t threads) { pool_->Start(threads, true); }
        ~ScopedThreads_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    Dal::ScriptValuationSettings_ Valuation() {
        Dal::ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Date_(2026, 9, 12);
        return settings;
    }

    auto Model(double spot = 100.0) { return Dal::NewBSModelData("risk_model", spot, 0.0, 0.0, 0.0); }

    auto HybridAndGsrSamples() {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, 0.2, 0.0)),
                                Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridDeterministicRateData_("RATE", "USD", 0.05))};
        settings.correlation_ =
            Dal::Handle_<Dal::HybridCorrelationData_>(new Dal::HybridConstantCorrelationData_("correlation", {"FA"}, Dal::Matrix_<>(1, 1, 1.0)));
        const auto hybrid = Dal::NewHybridModelData("hybrid", settings);
        const auto equity = Dal::NewScriptProduct("call", {Cell_(Date_(2027, 9, 12))}, {"pay PAYS MAX(FIX(EQ[A]) - 100, 0)"});
        const Date_ today(2026, 9, 12);
        const auto curve = Dal::NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Dal::Matrix_<>(0, 0));
        const auto gsr = Dal::NewGSRModelData("rates", curve, Dal::NewGSRVolData("vol", {today}, {0.02}, {today}, {1.0}));
        const auto bond = Dal::NewScriptProduct("bond", {Cell_(Date_(2027, 9, 12))}, {"pay PAYS FIX(IR[USD,DF,2028-09-12])"});
        return std::array<std::pair<Dal::Handle_<Dal::ScriptProductData_>, Dal::Handle_<Dal::ModelData_>>, 2>{std::make_pair(equity, hybrid),
                                                                                                              std::make_pair(bond, gsr)};
    }

    template <class F_> void AssertError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected error for " << field;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(RiskValueTest, TestStructuredNativeValueMatchesLegacyAndScalesSelectedColumns) {
    const auto product =
        Dal::NewScriptProduct("risk_call", {Cell_("STRIKE"), Cell_(Date_(2026, 9, 22))}, {"100", "call pays MAX(SPOT() - STRIKE, 0)"});
    const auto model = Dal::NewBSModelData("risk_model", 100.0, 0.2, 0.05, 0.02);
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Date_(2026, 9, 12);
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.compiled_ = true;
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Vector_<String_>{"constant:0", "model:1"};
    request.reportFactors_ = Vector_<>{0.5, 0.01};
    const auto legacy = Dal::ValueByMonteCarlo(product, model, 257, valuation, simulation);

    const auto result = Dal::ValueByMonteCarloWithRisk(product, model, 257, request, valuation, simulation);

    ASSERT_EQ(result.Jacobian().Rows(), 1);
    ASSERT_EQ(result.Jacobian().Cols(), 2);
    ASSERT_DOUBLE_EQ(result.Values()[0], legacy.at("PV"));
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), legacy.at("d_STRIKE"));
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 1), legacy.at("d_vol"));
    ASSERT_DOUBLE_EQ(result.ReportedJacobian()(0, 1), 0.01 * legacy.at("d_vol"));
    ASSERT_DOUBLE_EQ(result.LegacyValues().at("d_vol"), legacy.at("d_vol"));
    ASSERT_EQ(result.InputAxis()[0].id_, String_("constant:0"));
    ASSERT_EQ(result.CompleteInputAxis().size(), 5);
    ASSERT_EQ(result.Provenance().method_, String_("NativeAAD"));
    ASSERT_EQ(result.Provenance().evaluationDate_, valuation.evaluationDate_);
}

TEST(RiskValueTest, TestInvalidRequestsFailBeforeHistoryCompilationOrWorkers) {
    Dal::InitGlobalData(1);
    const auto product =
        Dal::NewScriptProduct("history", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"x = FIX(EQ[RISK_REJECTION])", "pay pays x"});
    const auto model = Model();
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.compiled_ = true;
    auto run = [&](const Dal::Script::RiskRequest_& request) {
        static_cast<void>(Dal::ValueByMonteCarloWithRisk(product, model, 32, request, Valuation(), simulation));
    };
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Vector_<String_>{"model:99"};
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "unknown input"));
    request.inputs_ = Vector_<String_>{"model:0", "MODEL:0"};
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "repeated input"));
    request.inputs_ = Vector_<String_>{"model:0"};
    for (const double scale : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        request.reportFactors_ = Vector_<>{scale};
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "report factor"));
    }
    request.reportFactors_.reset();
    request.outputs_ = Vector_<String_>();
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "payoff"));
    request.outputs_.reset();
    request.numericPayloadBudgetBytes_ = 2 * sizeof(double) - 1;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "RiskResultBudgetExceeded"));
    request.numericPayloadBudgetBytes_.reset();
    simulation.enableAad_ = false;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { run(request); }, "price-only"));
    ASSERT_THROW(static_cast<void>(Dal::ValueByMonteCarloWithRisk(product, model, -1)), Dal::ScriptError_);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(RiskValueTest, TestEmptyNativeInputsKeepSmoothedEstimatorAndPassiveOmissionIsPriceOnly) {
    const auto product = Dal::NewScriptProduct("smooth", {Cell_(Date_(2026, 9, 22))}, {"IF SPOT() > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"});
    const auto model = Model();
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Vector_<String_>();
    request.numericPayloadBudgetBytes_ = sizeof(double);
    for (const bool compiled : {false, true}) {
        auto simulation = Dal::DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        const auto native = Dal::ValueByMonteCarloWithRisk(product, model, 32, request, Valuation(), simulation);
        const auto legacy = Dal::ValueByMonteCarlo(product, model, 32, Valuation(), simulation);
        ASSERT_DOUBLE_EQ(native.Values()[0], legacy.at("PV"));
        ASSERT_EQ(native.Jacobian().Rows(), 1);
        ASSERT_EQ(native.Jacobian().Cols(), 0);
        simulation.enableAad_ = false;
        const auto passive = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, Valuation(), simulation);
        ASSERT_EQ(passive.Provenance().method_, String_("PriceOnly"));
        ASSERT_EQ(passive.Jacobian().Cols(), 0);
        ASSERT_DOUBLE_EQ(passive.Values()[0], Dal::ValueByMonteCarlo(product, model, 32, Valuation(), simulation).at("PV"));
        ASSERT_NE(native.Values()[0], passive.Values()[0]);
    }
}

TEST(RiskValueTest, TestSnapshotSurvivesCallbackMutationFailureAndLaterValuation) {
    Dal::InitGlobalData(1);
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[RISK_SNAPSHOT]", 80.0);
    auto product = Dal::NewScriptProduct("snapshot", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                         {"2", "x = SCALE * FIX(EQ[RISK_SNAPSHOT])", "pay PAYS x + FIX(EQ[RISK_SNAPSHOT])"});
    auto model = Model();
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    struct MutateCaller_ : Dal::Detail::FixingReadObserver_ {
        Dal::Handle_<Dal::ScriptProductData_>* product_;
        Dal::Handle_<Dal::ModelData_>* model_;
        Dal::MonteCarloSettings_* simulation_;
        void BeforeHistory(const String_&) override {
            *product_ = Dal::NewScriptProduct("changed", {Cell_(Date_(2026, 9, 22))}, {"pay PAYS 1000"});
            *model_ = Model(500.0);
            simulation_->enableAad_ = false;
        }
    } observer;
    observer.product_ = &product;
    observer.model_ = &model;
    observer.simulation_ = &simulation;
    const auto first = [&] {
        const Dal::Detail::ScopedFixingReadObserver_ observe(&observer);
        return Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, Valuation(), simulation);
    }();
    ASSERT_DOUBLE_EQ(first.Values()[0], 260.0);
    ASSERT_DOUBLE_EQ(first.Jacobian()(0, 4), 80.0);
    ASSERT_EQ(first.Provenance().method_, String_("NativeAAD"));
    const auto& execution = *first.Provenance().execution_;
    ASSERT_EQ(execution.productEvents_.size(), 3);
    ASSERT_EQ(execution.productEvents_[2], String_("pay PAYS x + FIX(EQ[RISK_SNAPSHOT])"));
    ASSERT_TRUE(execution.simulation_.enableAad_);
    ASSERT_EQ(execution.pathsPerReplicate_, 32);
    ASSERT_EQ(execution.observations_.size(), 2);
    ASSERT_EQ(execution.observations_[0].value_, std::optional<double>(80.0));
    Dal::Script::RiskRequest_ invalid;
    invalid.inputs_ = Vector_<String_>{"bad"};
    ASSERT_THROW(static_cast<void>(Dal::ValueByMonteCarloWithRisk(product, model, 32, invalid, Valuation())), Dal::ScriptError_);
    const auto later = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, Valuation(), simulation);
    ASSERT_DOUBLE_EQ(later.Values()[0], 1000.0);
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[RISK_SNAPSHOT]", 90.0);
    ASSERT_DOUBLE_EQ(first.Values()[0], 260.0);
    ASSERT_EQ(first.Provenance().execution_->observations_[0].value_, std::optional<double>(80.0));
}

TEST(RiskValueTest, TestLsmRqmcMatchesLegacyAndLabelsMixedPolicyDerivative) {
    const auto product = Dal::NewScriptProduct("bermudan", {Cell_(Date_(2027, 3, 12)), Cell_(Date_(2027, 9, 12))},
                                               {"EXERCISE MAX(100 - SPOT(), 0)", "EXERCISE MAX(100 - SPOT(), 0)"});
    const auto model = Dal::NewBSModelData("put", 100.0, 0.2, 0.05, 0.0);
    for (const bool compiled : {false, true}) {
        for (const auto& mode : {String_("Frozen"), String_("RetrainedBump")}) {
            auto simulation = Dal::DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            simulation.lsmcTrainingPaths_ = 512;
            simulation.lsmcRqmcReplicates_ = 2;
            simulation.lsmcTrainingSeed_ = 7;
            simulation.lsmcPricingSeed_ = 11;
            simulation.lsmcPolicyRiskMode_ = mode;
            const auto legacy = Dal::ValueByMonteCarlo(product, model, 257, Valuation(), simulation);
            const auto result = Dal::ValueByMonteCarloWithRisk(product, model, 257, {}, Valuation(), simulation);
            ASSERT_EQ(result.LegacyValues(), legacy);
            ASSERT_EQ(result.Provenance().execution_->pricingReplicates_, 2);
            ASSERT_EQ(result.Provenance().execution_->pathsPerReplicate_, 257);
            ASSERT_EQ(result.Provenance().method_, mode == "Frozen" ? String_("NativeAAD") : String_("NativeAADWithRetrainedPolicySecant"));
        }
    }
}

TEST(RiskValueTest, TestHybridAndGsrAxesAndLegacyValuesAgree) {
    Dal::InitGlobalData(1);
    const ScopedThreads_ threads(1);
    const auto samples = HybridAndGsrSamples();
    for (const bool compiled : {false, true}) {
        auto simulation = Dal::DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        for (const auto& sample : samples) {
            const auto legacy = Dal::ValueByMonteCarlo(sample.first, sample.second, 257, Valuation(), simulation);
            const auto result = Dal::ValueByMonteCarloWithRisk(sample.first, sample.second, 257, {}, Valuation(), simulation);
            ASSERT_EQ(result.LegacyValues(), legacy);
            ASSERT_EQ(result.Provenance().modelType_, sample.second->Type());
            ASSERT_FALSE(result.Provenance().execution_->modelSnapshotJson_.empty());
        }
    }
}

TEST(RiskValueTest, TestFourWorkerHybridAndGsrLegacyProjectionPreservesNumericContract) {
    Dal::InitGlobalData(1);
    const ScopedThreads_ threads(4);
    const auto samples = HybridAndGsrSamples();
    for (const bool compiled : {false, true}) {
        auto simulation = Dal::DefaultRiskMonteCarloSettings();
        simulation.compiled_ = compiled;
        for (const int paths : {257, 2057}) {
            for (const auto& sample : samples) {
                const auto legacy = Dal::ValueByMonteCarlo(sample.first, sample.second, paths, Valuation(), simulation);
                const auto result = Dal::ValueByMonteCarloWithRisk(sample.first, sample.second, paths, {}, Valuation(), simulation);
                const auto projected = result.LegacyValues();
                ASSERT_EQ(projected.size(), legacy.size());
                for (const auto& item : legacy) {
                    const double actual = projected.at(item.first);
                    const double tolerance = 1e-10 * std::max(1.0, std::max(std::abs(actual), std::abs(item.second)));
                    ASSERT_NEAR(actual, item.second, tolerance) << item.first;
                }
                ASSERT_EQ(result.Provenance().modelType_, sample.second->Type());
                ASSERT_FALSE(result.Provenance().execution_->modelSnapshotJson_.empty());
            }
        }
    }
}

TEST(RiskValueTest, TestCoincidentLabelsRetainIndependentColumnsAndExpiredResultIsZero) {
    const auto product = Dal::NewScriptProduct("labels", {Cell_("vol"), Cell_(Date_(2026, 9, 22))}, {"2", "pay PAYS vol * SPOT()"});
    const auto model = Model();
    const auto result = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, Valuation());
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 0), 2.0);
    ASSERT_DOUBLE_EQ(result.Jacobian()(0, 4), 100.0);
    ASSERT_THROW(static_cast<void>(result.LegacyValues()), Dal::ScriptError_);
    ASSERT_EQ(result.CompleteInputAxis()[1].physicalUnit_, std::optional<String_>("year^-1/2"));
    ASSERT_FALSE(result.CompleteInputAxis()[0].physicalUnit_);
    const auto expired = Dal::NewScriptProduct("expired", {Cell_(Date_(2026, 9, 11))}, {"pay PAYS 1"});
    const auto zero = Dal::ValueByMonteCarloWithRisk(expired, model, 32, {}, Valuation());
    ASSERT_DOUBLE_EQ(zero.Values()[0], 0.0);
    ASSERT_EQ(zero.Provenance().method_, String_("Expired"));
    ASSERT_TRUE(zero.Provenance().execution_->allExpired_);
}
