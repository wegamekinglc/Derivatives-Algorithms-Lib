//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>
#include <rapidjson/document.h>

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

    Dal::Handle_<Dal::ModelData_>
    HybridCurveModel(const Dal::Vector_<>& times, const Dal::Vector_<>& logDF, double volA = 0.0, double volB = 0.0, double correlationValue = 0.0) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, volA, 0.0)),
                                Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("B", "EQ[B]", "USD", "FB", 120.0, volB, 0.0)),
                                Dal::NewHybridLogDfRateData("RATE", "USD", times, logDF)};
        Dal::Matrix_<> correlation(2, 2, 0.0);
        correlation(0, 0) = correlation(1, 1) = 1.0;
        correlation(0, 1) = correlation(1, 0) = correlationValue;
        settings.correlation_ = Dal::Handle_<Dal::HybridCorrelationData_>(new Dal::HybridConstantCorrelationData_("corr", {"FA", "FB"}, correlation));
        return Dal::NewHybridModelData("hybrid_curve", settings);
    }
} // namespace

TEST(HybridValueTest, TestFlatLogDfMatchesConstantRatePvAndEquityGreeks) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("call", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"x = FIX(EQ[A])", "pay PAYS MAX(FIX(EQ[B]) + x - 220, 0)"});
    constexpr double rate = 0.05;
    const double middle = (Dal::Date_(2027, 3, 27) - Dal::Date_(2026, 9, 27)) / 365.0;
    const auto curve = HybridCurveModel({0.0, middle, 1.0}, {0.0, -rate * middle, -rate}, 0.2, 0.3, 0.35);
    Dal::HybridSettings_ settings;
    settings.domesticCurrency_ = "USD";
    settings.components_ = {Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("A", "EQ[A]", "USD", "FA", 100.0, 0.2, 0.0)),
                            Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridBSEquityData_("B", "EQ[B]", "USD", "FB", 120.0, 0.3, 0.0)),
                            Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridDeterministicRateData_("RATE", "USD", rate))};
    Dal::Matrix_<> correlation(2, 2, 0.35);
    correlation(0, 0) = correlation(1, 1) = 1.0;
    settings.correlation_ = Dal::Handle_<Dal::HybridCorrelationData_>(new Dal::HybridConstantCorrelationData_("corr", {"FA", "FB"}, correlation));
    const auto flat = Dal::NewHybridModelData("hybrid_flat", settings);
    for (const bool compiled : {false, true}) {
        Dal::MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        simulation.enableAad_ = true;
        const auto oldResult = Dal::ValueByMonteCarlo(product, flat, 2048, Dal::ScriptValuationSettings_(), simulation);
        const auto newResult = Dal::ValueByMonteCarlo(product, curve, 2048, Dal::ScriptValuationSettings_(), simulation);
        for (const auto& key : {"PV", "d_spot:EQ[A]", "d_spot:EQ[B]", "d_vol:EQ[A]", "d_vol:EQ[B]", "d_div:EQ[A]", "d_div:EQ[B]"})
            ASSERT_NEAR(newResult.at(key), oldResult.at(key), 1e-10) << key;
        ASSERT_NEAR(oldResult.at("d_rate:USD"), -middle * newResult.at("d_logdf:USD:1") - newResult.at("d_logdf:USD:2"), 1e-10);
    }
}

TEST(HybridValueTest, TestNonflatCurveCrossMomentAndNodeRisk) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("cross", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"x = 0", "pay PAYS FIX(EQ[A]) * FIX(EQ[B])"});
    const double middle = (Dal::Date_(2027, 3, 27) - Dal::Date_(2026, 9, 27)) / 365.0;
    const auto make = [&](double finalLogDF) { return HybridCurveModel({0.0, middle, 1.0}, {0.0, -0.01, finalLogDF}, 0.2, 0.3, 0.6); };
    {
        auto direct = Dal::CreateModel<Dal::AAD::Number_>(make(-0.05));
        const Dal::Vector_<> timeline{1.0};
        Dal::Vector_<Dal::AAD::SampleDef_> definitions(1);
        definitions[0].numeraire_ = true;
        definitions[0].indexNames_ = {"EQ[A]", "EQ[B]"};
        direct->Allocate(timeline, definitions);
        Dal::AAD::Scenario_<Dal::AAD::Number_> path;
        Dal::AAD::AllocatePath(definitions, path);
        Dal::AAD::Rewind(*Dal::AAD::Tape());
        for (auto* parameter : direct->Parameters())
            Dal::AAD::PutOnTape(*parameter);
        Dal::AAD::NewRecording(*Dal::AAD::Tape());
        direct->Init(timeline, definitions);
        direct->GeneratePath({0.0, 0.0}, &path);
        Dal::AAD::Number_ value = path[0].observations_[0] * path[0].observations_[1] / path[0].numeraire_;
        Dal::AAD::Adjoint(value) = 1.0;
        Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
        ASSERT_NEAR(Dal::AAD::Adjoint(*direct->Parameters()[6]), 0.0, 1e-8);
        ASSERT_NEAR(Dal::AAD::Adjoint(*direct->Parameters()[7]), -Dal::AAD::Value(value), 1e-8);
    }
    Dal::MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    constexpr size_t paths = 100000;
    const auto result = Dal::ValueByMonteCarlo(product, make(-0.05), paths, Dal::ScriptValuationSettings_(), simulation);
    const double analytic = 12000.0 * std::exp(0.05 + 0.6 * 0.2 * 0.3);
    ASSERT_NEAR(result.at("PV"), analytic, 55.0);
    ASSERT_NEAR(result.at("d_logdf:USD:2"), -result.at("PV"), 1e-8);
    ASSERT_NEAR(result.at("d_logdf:USD:1"), 0.0, 1e-8);
    simulation.enableAad_ = false;
    const double epsilon = 1e-4;
    const auto up = Dal::ValueByMonteCarlo(product, make(-0.05 + epsilon), paths, Dal::ScriptValuationSettings_(), simulation).at("PV");
    const auto down = Dal::ValueByMonteCarlo(product, make(-0.05 - epsilon), paths, Dal::ScriptValuationSettings_(), simulation).at("PV");
    ASSERT_NEAR(result.at("d_logdf:USD:2"), (up - down) / (2 * epsilon), 1e-4);
    const auto middleUp = HybridCurveModel({0.0, middle, 1.0}, {0.0, -0.01 + epsilon, -0.05}, 0.2, 0.3, 0.6);
    const auto middleDown = HybridCurveModel({0.0, middle, 1.0}, {0.0, -0.01 - epsilon, -0.05}, 0.2, 0.3, 0.6);
    const auto middleUpPv = Dal::ValueByMonteCarlo(product, middleUp, paths, Dal::ScriptValuationSettings_(), simulation).at("PV");
    const auto middleDownPv = Dal::ValueByMonteCarlo(product, middleDown, paths, Dal::ScriptValuationSettings_(), simulation).at("PV");
    ASSERT_NEAR(result.at("d_logdf:USD:1"), (middleUpPv - middleDownPv) / (2 * epsilon), 1e-4);
}

TEST(HybridValueTest, TestNonflatCurveLsmTreeAndCompiledAad) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    Dal::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[A]";
    const auto product = Dal::NewScriptProduct("bermudan", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"EXERCISE MAX(125 - FIX(EQ[A]), 0)", "EXERCISE MAX(125 - FIX(EQ[A]), 0)"}, settings);
    const double middle = (Dal::Date_(2027, 3, 27) - Dal::Date_(2026, 9, 27)) / 365.0;
    const auto make = [&](double firstLogDF) { return HybridCurveModel({0.0, middle, 1.0}, {0.0, firstLogDF, -0.08}); };
    const auto model = make(-0.03);
    for (const bool compiled : {false, true}) {
        Dal::MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        simulation.enableAad_ = true;
        simulation.lsmcTrainingPaths_ = 128;
        const auto result = Dal::ValueByMonteCarlo(product, model, 128, Dal::ScriptValuationSettings_(), simulation);
        ASSERT_NEAR(result.at("PV"), 125.0 * std::exp(-0.03) - 100.0, 1e-8);
        ASSERT_NEAR(result.at("d_logdf:USD:1"), 125.0 * std::exp(-0.03), 1e-8);
        ASSERT_NEAR(result.at("d_logdf:USD:2"), 0.0, 1e-8);
        simulation.enableAad_ = false;
        const double epsilon = 1e-5;
        const auto up = Dal::ValueByMonteCarlo(product, make(-0.03 + epsilon), 128, Dal::ScriptValuationSettings_(), simulation).at("PV");
        const auto down = Dal::ValueByMonteCarlo(product, make(-0.03 - epsilon), 128, Dal::ScriptValuationSettings_(), simulation).at("PV");
        ASSERT_NEAR(result.at("d_logdf:USD:1"), (up - down) / (2 * epsilon), 1e-7);
    }
}

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

TEST(HybridValueTest, TestMultiAssetExplainNamesEachModelBinding) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto product = Dal::NewScriptProduct("joint", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"pay PAYS FIX(EQ[B]) + FIX(EQ[A])"});
    rapidjson::Document json;
    json.Parse(Dal::ExplainScriptValuation(product, CorrelatedModel()).c_str());
    ASSERT_FALSE(json.HasParseError());
    const auto& bindings = json["model_bindings"];
    ASSERT_EQ(bindings.Size(), 2u);
    ASSERT_STREQ(bindings[0]["asset"].GetString(), "EQ[B]");
    ASSERT_STREQ(bindings[1]["asset"].GetString(), "EQ[A]");
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

TEST(HybridValueTest, TestTwoSelectedAssetsPriceWithoutDefaultIndex) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    Dal::ScriptProductSettings_ settings;
    settings.regressionFeatures_ = {"EQ[A]", "EQ[B]"};
    const auto product = Dal::NewScriptProduct("two-state", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"EXERCISE MAX(FIX(EQ[A]) - FIX(EQ[B]), 0)", "EXERCISE MAX(FIX(EQ[B]) - FIX(EQ[A]), 0)"}, settings);
    for (const auto& model : {CorrelatedModel(), HybridModel()})
        for (const bool compiled : {false, true})
            for (const bool aad : {false, true}) {
                Dal::MonteCarloSettings_ simulation;
                simulation.compiled_ = compiled;
                simulation.enableAad_ = aad;
                simulation.lsmcTrainingPaths_ = 128;
                const auto result = Dal::ValueByMonteCarlo(product, model, 128, {}, simulation);
                ASSERT_NEAR(result.at("PV"), 20.0, 1e-10);
                if (aad) {
                    ASSERT_NEAR(result.at("d_spot:EQ[A]"), -1.0, 1e-10);
                    ASSERT_NEAR(result.at("d_spot:EQ[B]"), 1.0, 1e-10);
                }
            }
    auto model = Dal::CreateModel<double>(CorrelatedModel());
    const auto prepared = Dal::Script::PrepareScript(*product, model.get(), {}, {});
    for (const auto& sample : prepared.DefLine())
        ASSERT_EQ(sample.indexNames_.size(), 2u);
    settings.regressionFeatures_ = {"EQ[A]"};
    const auto scalar = Dal::NewScriptProduct("one-selected", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"EXERCISE MAX(FIX(EQ[A]) - 100, 0)"}, settings);
    rapidjson::Document diagnostic;
    diagnostic.Parse(Dal::ExplainScriptSimulation(scalar, CorrelatedModel(), 128).c_str());
    ASSERT_FALSE(diagnostic.HasParseError());
    ASSERT_STREQ(diagnostic["exercise_events"][0]["regressor_index"].GetString(), "EQ[A]");
}

TEST(HybridValueTest, TestTwoStateBermudanMatchesExchangeReference) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    Dal::ScriptProductSettings_ settings;
    settings.regressionFeatures_ = {"VAR[a]", "VAR[b]"};
    const auto product = Dal::NewScriptProduct("exchange-bermudan", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"a = FIX(EQ[A])\nb = FIX(EQ[B])\nEXERCISE MAX(a - b, 0)", "EXERCISE MAX(b - 0.9 * a, 0)"}, settings);
    const double volA = 0.2;
    const double volB = 0.3;
    const double correlation = 0.35;
    //  The second exercise uses the frozen first-date state, so the optimal
    //  first-date payoff is max(A-B, B-0.9A) = A-B + (2B-1.9A)^+.
    const double time = (Dal::Date_(2027, 3, 27) - Dal::Date_(2026, 9, 27)) / 365.0;
    const double exchangeVol = std::sqrt(volA * volA + volB * volB - 2.0 * correlation * volA * volB);
    const double width = exchangeVol * std::sqrt(time);
    const double d1 = (std::log((2.0 * 120.0) / (1.9 * 100.0)) + 0.5 * width * width) / width;
    const double d2 = d1 - width;
    const auto cdf = [](double value) { return 0.5 * std::erfc(-value / std::sqrt(2.0)); };
    const double reference = 100.0 - 120.0 + 2.0 * 120.0 * cdf(d1) - 1.9 * 100.0 * cdf(d2);
    for (const auto& model : {CorrelatedModel(volA, volB, correlation), HybridModel(volA, volB, correlation)})
        for (const bool bridge : {false, true})
            for (const bool aad : {false, true}) {
                Dal::MonteCarloSettings_ simulation;
                simulation.useBb_ = bridge;
                simulation.enableAad_ = aad;
                simulation.lsmcTrainingPaths_ = 8192;
                simulation.lsmcValidationPaths_ = 2048;
                const auto tree = Dal::ValueByMonteCarlo(product, model, 16384, {}, simulation);
                simulation.compiled_ = true;
                const auto compiled = Dal::ValueByMonteCarlo(product, model, 16384, {}, simulation);
                ASSERT_NEAR(tree.at("PV"), reference, 0.4);
                ASSERT_NEAR(compiled.at("PV"), tree.at("PV"), 1e-8);
                if (aad) {
                    for (const auto& risk : {"d_spot:EQ[A]", "d_spot:EQ[B]"}) {
                        ASSERT_TRUE(std::isfinite(tree.at(risk)));
                        ASSERT_NEAR(compiled.at(risk), tree.at(risk), 1e-8);
                    }
                }
            }
    Dal::MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = 4096;
    rapidjson::Document diagnostic;
    diagnostic.Parse(Dal::ExplainScriptSimulation(product, CorrelatedModel(volA, volB, correlation), 4096, {}, simulation).c_str());
    ASSERT_FALSE(diagnostic.HasParseError());
    const auto& event = diagnostic["exercise_events"][0];
    ASSERT_TRUE(event["regressor_index"].IsNull());
    ASSERT_EQ(event["regression_features"].Size(), 2u);
    ASSERT_STREQ(event["regression_features"][0].GetString(), "VAR[a]");
    ASSERT_STREQ(event["regression_features"][1].GetString(), "VAR[b]");
    ASSERT_EQ(event["normalization_means"].Size(), 2u);
    ASSERT_EQ(event["normalization_sigmas"].Size(), 2u);
    ASSERT_EQ(event["basis_powers"].Size(), event["coefficients"].Size());
}

TEST(HybridValueTest, TestMissingRegressionStateLosesContinuationValue) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const Dal::Vector_<Dal::Cell_> dates = {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))};
    const Dal::Vector_<Dal::String_> events = {"a = FIX(EQ[A])\nb = FIX(EQ[B])\nEXERCISE MAX(a - b + 60, 0)", "EXERCISE MAX(b - a + 60, 0)"};
    Dal::MonteCarloSettings_ simulation;
    simulation.useBb_ = true;
    simulation.compiled_ = true;
    simulation.lsmcTrainingPaths_ = 8192;
    simulation.lsmcValidationPaths_ = 2048;
    const auto model = CorrelatedModel(0.2, 0.3, 0.35);
    Dal::ScriptProductSettings_ settings;
    settings.regressionFeatures_ = {"VAR[a]"};
    const double singleState =
        Dal::ValueByMonteCarlo(Dal::NewScriptProduct("one-state", dates, events, settings), model, 32768, {}, simulation).at("PV");
    settings.regressionFeatures_ = {"VAR[a]", "VAR[b]"};
    const double twoState =
        Dal::ValueByMonteCarlo(Dal::NewScriptProduct("two-state", dates, events, settings), model, 32768, {}, simulation).at("PV");

    // The terminal payoff is frozen at the first date; the optimal value is
    // E[60 + |A-B|] = 60 + A0-B0 + 2 E[(B-A)^+].
    const double time = (Dal::Date_(2027, 3, 27) - Dal::Date_(2026, 9, 27)) / 365.0;
    const double width = std::sqrt(0.2 * 0.2 + 0.3 * 0.3 - 2.0 * 0.35 * 0.2 * 0.3) * std::sqrt(time);
    const double d1 = (std::log(120.0 / 100.0) + 0.5 * width * width) / width;
    const double d2 = d1 - width;
    const auto cdf = [](double value) { return 0.5 * std::erfc(-value / std::sqrt(2.0)); };
    const double reference = 60.0 + 100.0 - 120.0 + 2.0 * (120.0 * cdf(d1) - 100.0 * cdf(d2));
    ASSERT_NEAR(twoState, reference, 0.4);
    ASSERT_GT(twoState, singleState + 0.8);
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

TEST(HybridValueTest, TestTwoStateExerciseIsThreadInvariant) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    ThreadPoolRestore_ threads;
    Dal::ScriptProductSettings_ settings;
    settings.regressionFeatures_ = {"EQ[A]", "EQ[B]"};
    const auto product = Dal::NewScriptProduct("two-state-thread", {Dal::Cell_(Dal::Date_(2027, 3, 27)), Dal::Cell_(Dal::Date_(2027, 9, 27))},
                                               {"EXERCISE MAX(FIX(EQ[A]) - FIX(EQ[B]), 0)", "EXERCISE MAX(FIX(EQ[B]) - FIX(EQ[A]), 0)"}, settings);
    const auto model = HybridModel(0.2, 0.3, 0.35);
    for (const bool compiled : {false, true})
        for (const bool aad : {false, true}) {
            Dal::MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            simulation.enableAad_ = aad;
            simulation.lsmcTrainingPaths_ = 4096;
            simulation.lsmcValidationPaths_ = 1024;
            threads.pool_->Start(1, true);
            const auto one = Dal::ValueByMonteCarlo(product, model, 4096, {}, simulation);
            threads.pool_->Start(4, true);
            const auto four = Dal::ValueByMonteCarlo(product, model, 4096, {}, simulation);
            ASSERT_EQ(one, four);
        }
}

TEST(HybridValueTest, TestInvalidRegressionFeaturesFailBeforeWorkerSubmission) {
    Dal::RegisterAll_::Init();
    const auto restore = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2026, 9, 27));
    const auto model = CorrelatedModel();
    SubmissionCounter_ counter;
    const Dal::Script::Detail::ScopedSimulationObserver_ observe(&counter);
    for (const Dal::Vector_<Dal::String_>& features :
         {Dal::Vector_<Dal::String_>{"EQ[A]", "EQ[B]", "EQ[A]"}, Dal::Vector_<Dal::String_>{"EQ[A]", "VAR[missing]"},
          Dal::Vector_<Dal::String_>{"EQ[A]", "FX[EUR/USD]"}, Dal::Vector_<Dal::String_>{"EQ[A]", "EQ[B]", "EQ[C]", "EQ[D]"}}) {
        Dal::ScriptProductSettings_ settings;
        settings.regressionFeatures_ = features;
        const auto product = Dal::NewScriptProduct("invalid-states", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"EXERCISE 1"}, settings);
        ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 128), Dal::ScriptError_);
        ASSERT_EQ(counter.count_, 0u);
    }
    Dal::ScriptProductSettings_ settings;
    settings.regressionFeatures_ = {"EQ[A]", "EQ[B]"};
    const auto product = Dal::NewScriptProduct("too-high-degree", {Dal::Cell_(Dal::Date_(2027, 9, 27))}, {"EXERCISE 1"}, settings);
    Dal::MonteCarloSettings_ simulation;
    simulation.lsmcBasisDegree_ = 4;
    ASSERT_THROW(Dal::ValueByMonteCarlo(product, model, 128, {}, simulation), Dal::ScriptError_);
    ASSERT_EQ(counter.count_, 0u);
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
