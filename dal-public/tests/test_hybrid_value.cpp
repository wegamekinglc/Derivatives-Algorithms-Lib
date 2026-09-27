//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include <dal/concurrency/threadpool.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/globals.hpp>

#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

namespace {
    class UnprovenNumeraireModel_ final : public Dal::AAD::Model_<double> {
        Dal::AAD::BlackScholes_<double> delegate_{100.0, 0.2, 0.05, 0.0};

    public:
        [[nodiscard]] bool SupportsIndex(const Dal::Index_& index) const override { return delegate_.SupportsIndex(index); }
        void Allocate(const Dal::Vector_<>& times, const Dal::Vector_<Dal::AAD::SampleDef_>& definitions) override {
            delegate_.Allocate(times, definitions);
        }
        void Init(const Dal::Vector_<>& times, const Dal::Vector_<Dal::AAD::SampleDef_>& definitions) override { delegate_.Init(times, definitions); }
        [[nodiscard]] size_t SimDim() const override { return delegate_.SimDim(); }
        void GeneratePath(const Dal::Vector_<>& gaussian, Dal::AAD::Scenario_<>* path) const override { delegate_.GeneratePath(gaussian, path); }
        [[nodiscard]] std::unique_ptr<Dal::AAD::Model_<double>> Clone() const override { return std::make_unique<UnprovenNumeraireModel_>(); }
        [[nodiscard]] const Dal::Vector_<double*>& Parameters() const override { return delegate_.Parameters(); }
        [[nodiscard]] const Dal::Vector_<Dal::String_>& ParameterLabels() const override { return delegate_.ParameterLabels(); }
    };

    struct SubmissionCounter_ final : Dal::Script::Detail::SimulationObserver_ {
        size_t count_ = 0;
        void AfterSubmission() override { ++count_; }
    };

    struct ThreadPoolRestore_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        ~ThreadPoolRestore_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    Dal::Handle_<Dal::ModelData_> CorrelatedModel(double volA = 0.0, double volB = 0.0, double correlation = 0.0) {
        Dal::CorrelatedBSSettings_ settings;
        settings.assets_ = {{"EQ[A]", 100.0, volA, 0.0}, {"EQ[B]", 120.0, volB, 0.0}};
        settings.correlations_ = Dal::Matrix_<>(2, 2, 0.0);
        settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
        settings.correlations_(0, 1) = settings.correlations_(1, 0) = correlation;
        return Dal::NewCorrelatedBSModelData("joint", settings);
    }

    Dal::Handle_<Dal::ModelData_> HybridModel(double volA = 0.0, double volB = 0.0, double correlationValue = 0.0) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, volA, 0.0)),
                                Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("B", "EQ[B]", "USD", "FB", 120.0, volB, 0.0)),
                                Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridDeterministicRateData_("RATE", "USD", 0.0))};
        Dal::Matrix_<> correlation(2, 2, 0.0);
        correlation(0, 0) = correlation(1, 1) = 1.0;
        correlation(0, 1) = correlation(1, 0) = correlationValue;
        settings.correlation_ = Dal::Handle_<Dal::HybridCorrelationData_>(new Dal::HybridConstantCorrelationData_("corr", {"FA", "FB"}, correlation));
        return Dal::NewHybridModelData("hybrid", settings);
    }
} // namespace

TEST(HybridValueTest, TestNamedObservationsShareAJointPath) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("joint", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[B]) + 2 * FIX(EQ[A])"});
    for (const auto& model : {CorrelatedModel(), HybridModel()})
        for (const bool compiled : {false, true})
            for (const bool aad : {false, true}) {
                Dal::MonteCarloSettings_ simulation;
                simulation.compiled_ = compiled;
                simulation.enableAad_ = aad;
                const auto result = Dal::ValueByMonteCarlo(product, model, 16, Dal::ScriptValuationSettings_(), simulation);
                ASSERT_NEAR(result.at("PV"), 320.0, 1.0e-10);
                if (aad) {
                    ASSERT_NEAR(result.at("d_spot:EQ[A]"), 2.0, 1.0e-10);
                    ASSERT_NEAR(result.at("d_spot:EQ[B]"), 1.0, 1.0e-10);
                }
            }
}

TEST(HybridValueTest, TestCrossMomentWithBrownianBridgeAndAad) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("cross", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A]) * FIX(EQ[B])"});
    const double expected = 12000.0 * std::exp(0.6 * 0.2 * 0.3);
    for (const auto& model : {CorrelatedModel(0.2, 0.3, 0.6), HybridModel(0.2, 0.3, 0.6)})
        for (const bool bridge : {false, true})
            for (const bool compiled : {false, true})
                for (const bool aad : {false, true}) {
                    Dal::MonteCarloSettings_ simulation;
                    simulation.useBb_ = bridge;
                    simulation.compiled_ = compiled;
                    simulation.enableAad_ = aad;
                    const auto result = Dal::ValueByMonteCarlo(product, model, 16384, Dal::ScriptValuationSettings_(), simulation);
                    ASSERT_NEAR(result.at("PV"), expected, 45.0);
                    if (aad) {
                        ASSERT_NEAR(result.at("d_spot:EQ[A]"), expected / 100.0, 0.45);
                        ASSERT_NEAR(result.at("d_spot:EQ[B]"), expected / 120.0, 0.40);
                    }
                }
}

TEST(HybridValueTest, TestAdditiveCallsMatchSingleAssetPrices) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product =
        Dal::NewScriptProduct("calls", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS MAX(FIX(EQ[A]) - 100, 0) + MAX(FIX(EQ[B]) - 120, 0)"});
    const auto cdf = [](double value) { return 0.5 * std::erfc(-value / std::sqrt(2.0)); };
    const double expected = 100.0 * (2.0 * cdf(0.1) - 1.0) + 120.0 * (2.0 * cdf(0.15) - 1.0);
    for (const auto& model : {CorrelatedModel(0.2, 0.3, 0.6), HybridModel(0.2, 0.3, 0.6)})
        for (const bool bridge : {false, true})
            for (const bool compiled : {false, true})
                for (const bool aad : {false, true}) {
                    Dal::MonteCarloSettings_ simulation;
                    simulation.useBb_ = bridge;
                    simulation.compiled_ = compiled;
                    simulation.enableAad_ = aad;
                    const auto result = Dal::ValueByMonteCarlo(product, model, 16384, Dal::ScriptValuationSettings_(), simulation);
                    ASSERT_NEAR(result.at("PV"), expected, 0.15);
                }
}

TEST(HybridValueTest, TestMultiAssetSpotNeedsAnExplicitDefault) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto unbound = Dal::NewScriptProduct("unbound", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS SPOT()"});
    ASSERT_THROW(Dal::ValueByMonteCarlo(unbound, CorrelatedModel(), 16), Dal::ScriptError_);
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[B]";
    const auto bound = Dal::NewScriptProduct("bound", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS SPOT() + FIX(EQ[A])"}, settings);
    ASSERT_NEAR(Dal::ValueByMonteCarlo(bound, CorrelatedModel(), 16).at("PV"), 220.0, 1.0e-10);
}

TEST(HybridValueTest, TestUnknownAndUnsupportedFutureIndicesReportTheirSource) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    for (const char* index : {"EQ[UNKNOWN]", "FX[EUR/USD]"}) {
        const auto product =
            Dal::NewScriptProduct("unsupported", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {Dal::String_("pay PAYS FIX(") + index + ")"});
        try {
            Dal::ValueByMonteCarlo(product, HybridModel(), 16);
            FAIL() << "expected an unsupported model observation";
        } catch (const Dal::ScriptError_& error) {
            const std::string message = error.what();
            ASSERT_NE(message.find("UnsupportedModelObservation"), std::string::npos);
            ASSERT_NE(message.find(index), std::string::npos);
            ASSERT_NE(message.find("event=2027-09-27"), std::string::npos);
        }
    }
}

TEST(HybridValueTest, TestHistoricalFixingKeepsItsIndexIdentity) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("history", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[A], 2026-09-26) + FIX(EQ[B])"});
    Dal::ScriptValuationSettings_ valuation;
    valuation.fixings_ =
        Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_({{"EQ[A]", {{Dal::DateTime_(Dal::Date_(2026, 9, 26), 0.0), 80.0}}}}));
    for (const auto& model : {CorrelatedModel(), HybridModel()}) {
        const auto result = Dal::ValueByMonteCarlo(product, model, 16, valuation);
        ASSERT_NEAR(result.at("PV"), 200.0, 1.0e-10);
    }
}

TEST(HybridValueTest, TestNamedObservationsOnDifferentDates) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("dates", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"x = FIX(EQ[A])", "pay PAYS x + FIX(EQ[B])"});
    for (const auto& model : {CorrelatedModel(), HybridModel()})
        for (const bool compiled : {false, true}) {
            Dal::MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            ASSERT_NEAR(Dal::ValueByMonteCarlo(product, model, 16, Dal::ScriptValuationSettings_(), simulation).at("PV"), 220.0, 1.0e-10);
        }
}

TEST(HybridValueTest, TestMultiAssetExerciseUsesSelectedRegressor) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const Dal::Vector_<Dal::Cell_> dates = {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))};
    const Dal::Vector_<Dal::String_> events = {"EXERCISE MAX(150 - FIX(EQ[B]), 0)", "EXERCISE MAX(150 - FIX(EQ[B]), 0)"};
    ASSERT_THROW(Dal::ValueByMonteCarlo(Dal::NewScriptProduct("ambiguous", dates, events), CorrelatedModel(), 64), Dal::ScriptError_);
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[B]";
    const auto product = Dal::NewScriptProduct("selected", dates, events, settings);
    for (const auto& model : {CorrelatedModel(), HybridModel()})
        for (const bool compiled : {false, true})
            for (const bool aad : {false, true}) {
                Dal::MonteCarloSettings_ simulation;
                simulation.compiled_ = compiled;
                simulation.enableAad_ = aad;
                simulation.lsmcTrainingPaths_ = 64;
                const auto result = Dal::ValueByMonteCarlo(product, model, 64, Dal::ScriptValuationSettings_(), simulation);
                ASSERT_NEAR(result.at("PV"), 30.0, 1.0e-8);
            }
}

TEST(HybridValueTest, TestRetrainedPolicyRespectsEachParameterConstraint) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[B]";
    const auto product = Dal::NewScriptProduct("policy", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"EXERCISE MAX(150 - FIX(EQ[B]), 0)", "EXERCISE MAX(150 - FIX(EQ[B]), 0)"}, settings);
    Dal::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.lsmcTrainingPaths_ = 64;
    simulation.lsmcPolicyRiskMode_ = "RetrainedBump";
    for (const auto& data : {CorrelatedModel(), HybridModel()}) {
        auto model = Dal::CreateModel<double>(data);
        ASSERT_FALSE(model->ValidParameterValue(0, 0.0));
        ASSERT_TRUE(model->ValidParameterValue(1, 0.0));
        ASSERT_FALSE(model->ValidParameterValue(1, -0.001));
        ASSERT_TRUE(model->ValidParameterValue(2, -0.01));
        const auto result = Dal::ValueByMonteCarlo(product, data, 64, Dal::ScriptValuationSettings_(), simulation);
        ASSERT_NEAR(result.at("PV"), 30.0, 1.0e-8);
        ASSERT_TRUE(std::isfinite(result.at("d_vol:EQ[B]")));
    }
}

TEST(HybridValueTest, TestMultiAssetExerciseIsThreadInvariant) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    ThreadPoolRestore_ threads;
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[B]";
    const auto product = Dal::NewScriptProduct("thread-invariant", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"EXERCISE MAX(120 - FIX(EQ[B]), 0)", "EXERCISE MAX(120 - FIX(EQ[B]), 0)"}, settings);
    const auto model = CorrelatedModel(0.10, 0.20, 0.40);
    for (const bool compiled : {false, true})
        for (const bool aad : {false, true}) {
            Dal::MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            simulation.enableAad_ = aad;
            simulation.lsmcTrainingPaths_ = 4096;
            simulation.lsmcValidationPaths_ = 1024;
            threads.pool_->Start(1, true);
            const auto one = Dal::ValueByMonteCarlo(product, model, 1024, Dal::ScriptValuationSettings_(), simulation);
            threads.pool_->Start(4, true);
            const auto four = Dal::ValueByMonteCarlo(product, model, 1024, Dal::ScriptValuationSettings_(), simulation);
            ASSERT_EQ(one, four);
        }
}

TEST(HybridValueTest, TestUnprovenNumeraireFailsBeforeWorkerSubmission) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const Dal::Script::ScriptProductData_ product("unproven", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"EXERCISE MAX(100 - FIX(EQ[A]), 0)"});
    UnprovenNumeraireModel_ model;
    const auto prepared = Dal::Script::PrepareScript(product, &model, {}, {});
    SubmissionCounter_ counter;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&counter);
    ASSERT_THROW(Dal::Script::MCLsmcSimulation(prepared, &model, 64), Dal::ScriptError_);
    ASSERT_EQ(counter.count_, 0u);
}
