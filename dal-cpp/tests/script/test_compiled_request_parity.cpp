#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <type_traits>

#include <dal/platform/platform.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::Handle_;
using Dal::ModelData_;
using Dal::String_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    struct PoolScope_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t previous_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        PoolScope_() { pool_->Start(3, true); }
        ~PoolScope_() {
            pool_->Start(previous_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    struct Oracle_ {
        double price_ = 0.0;
        std::array<double, 5> risks_{};
    };

    Oracle_ HistoricalOracle(size_t paths, const String_& method) {
        auto rng = Script::CreateRNG(method, 1, false);
        rng->SkipNormalTo(0);
        Vector_<> normal(1);
        Oracle_ result;
        const double weight = std::exp(-0.06) / static_cast<double>(paths);
        for (size_t path = 0; path < paths; ++path) {
            rng->FillNormal(&normal);
            const double spot = std::exp(std::log(100.0) + 0.03 - 0.01 - 0.5 * 0.2 * 0.2 + 0.2 * normal[0]);
            const double amount = 0.5 * (160.0 + spot);
            result.price_ += amount * weight;
            result.risks_[0] += 0.5 * spot / 100.0 * weight;
            result.risks_[1] += 0.5 * spot * (normal[0] - 0.2) * weight;
            result.risks_[2] += (0.5 * spot - 2.0 * amount) * weight;
            result.risks_[3] -= 0.5 * spot * weight;
            result.risks_[4] += 40.0 * weight;
        }
        return result;
    }

    template <bool ACTIVE_> void CheckHistoricalRequest() {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
        const PoolScope_ pool;
        const Script::ScriptProductData_ product(
            "", {Cell_("K"), Cell_(Date_(2026, 9, 30)), Cell_(Date_(2027, 10, 1))},
            {"2", "APPEND(v, K * FIX(EQ[DAL486_DEFAULT]))", "APPEND(v, 0.5 * (SPOT() + FIX(EQ[DAL486_DEFAULT]))) pay PAYS AVERAGE(v) ON 2028-09-30"});
        const Handle_<Dal::MarketFixingSnapshot_> snapshot(
            new Dal::MarketFixingSnapshot_({{"EQ[DAL486_DEFAULT]", {{Dal::DateTime_(Date_(2026, 9, 30), 0.0), 80.0}}}}));
        const Handle_<ModelData_> model(new Dal::BSModelData_("", 100.0, 0.2, 0.03, 0.01));
        Script::ScriptProductSettings_ contract;
        contract.defaultIndex_ = "EQ[DAL486_DEFAULT]";
        for (const String_ method : {"sobol", "mrg32"}) {
            for (const size_t paths : {1, 257}) {
                const auto expected = HistoricalOracle(paths, method);
                for (const bool compiled : {false, true}) {
                    SCOPED_TRACE(::testing::Message() << method << "; paths=" << paths << "; compiled=" << compiled);
                    Script::MonteCarloSettings_ settings;
                    settings.rsg_ = method;
                    settings.enableAad_ = ACTIVE_;
                    settings.compiled_ = compiled;
                    using Number_ = std::conditional_t<ACTIVE_, AAD::Number_, double>;
                    const auto result = Script::MCSimulation<Number_>(product, model, paths, {}, settings, snapshot, contract);
                    ASSERT_NEAR(result.aggregated_ / paths, expected.price_, 1e-10);
                    if constexpr (ACTIVE_) {
                        ASSERT_EQ(result.risks_.size(), 5);
                        ASSERT_NEAR(result["spot"], expected.risks_[0], 1e-10);
                        ASSERT_NEAR(result["vol"], expected.risks_[1], 1e-10);
                        ASSERT_NEAR(result["rate"], expected.risks_[2], 1e-10);
                        ASSERT_NEAR(result["div"], expected.risks_[3], 1e-10);
                        ASSERT_NEAR(result["K"], expected.risks_[4], 1e-10);
                    }
                }
            }
        }
    }

    double MeanNormal(size_t paths) {
        auto rng = Script::CreateRNG("sobol", 1, false);
        rng->SkipNormalTo(0);
        Vector_<> normal(1);
        double result = 0.0;
        for (size_t path = 0; path < paths; ++path) {
            rng->FillNormal(&normal);
            result += normal[0] / static_cast<double>(paths);
        }
        return result;
    }

    void CheckFuzzyBoundaries(bool explicitWidth) {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
        const PoolScope_ pool;
        const Handle_<ModelData_> model(new Dal::BSModelData_("", 1.0, 0.0, 0.0, 0.0));
        constexpr size_t PATHS = 17;
        const double meanNormal = MeanNormal(PATHS);
        struct Case_ {
            double barrier_, price_, delta_, barrierRisk_;
        };
        for (const auto row : {Case_{0.5, 1.0, 1.0, 0.0}, Case_{0.75, 1.0, 1.0, 0.0}, Case_{1.0, 1.5, -0.5, 2.0}, Case_{1.25, 2.0, 2.0, 0.0},
                               Case_{1.5, 2.0, 2.0, 0.0}}) {
            const String_ comparison = explicitWidth ? "SPOT() > B:0.5" : "SPOT() > B";
            const Script::ScriptProductData_ product(
                "", {Cell_("B"), Cell_(Date_(2027, 10, 1))},
                {Dal::ToString(row.barrier_), "IF " + comparison + " THEN APPEND(v, SPOT()) ELSE APPEND(v, 2 * SPOT()) END pay PAYS SUM(v)"});
            for (const bool compiled : {false, true}) {
                SCOPED_TRACE(::testing::Message() << "barrier=" << row.barrier_ << "; compiled=" << compiled);
                Script::MonteCarloSettings_ settings;
                settings.compiled_ = compiled;
                settings.smooth_ = 0.5;
                const auto result = Script::MCSimulation<AAD::Number_>(product, model, PATHS, {}, settings);
                ASSERT_NEAR(result.aggregated_ / PATHS, row.price_, 1e-10);
                ASSERT_EQ(result.risks_.size(), 5);
                ASSERT_NEAR(result["spot"], row.delta_, 1e-10);
                ASSERT_NEAR(result["vol"], row.delta_ * meanNormal, 1e-10);
                ASSERT_NEAR(result["rate"], row.delta_ - row.price_, 1e-10);
                ASSERT_NEAR(result["div"], -row.delta_, 1e-10);
                ASSERT_NEAR(result["B"], row.barrierRisk_, 1e-10);
            }
        }
    }

    Script::ScriptProductData_ HistoricalExercise() {
        return {"",
                {Cell_("K"), Cell_(Date_(2026, 9, 30)), Cell_(Date_(2027, 4, 1)), Cell_(Date_(2027, 10, 1))},
                {"100", "APPEND(v, K)", "APPEND(v, SPOT()) EXERCISE MAX(K - AVERAGE(v), 0)", "APPEND(v, SPOT()) pay PAYS MAX(K - AVERAGE(v), 0)"}};
    }
} // namespace

TEST(CompiledRequestParityTest, TestHistoricalVectorDelayedPaymentDouble) { ASSERT_NO_FATAL_FAILURE(CheckHistoricalRequest<false>()); }

TEST(CompiledRequestParityTest, TestHistoricalVectorDelayedPaymentAllRisks) { ASSERT_NO_FATAL_FAILURE(CheckHistoricalRequest<true>()); }

TEST(CompiledRequestParityTest, TestExplicitFuzzyVectorBoundaryAllRisks) { ASSERT_NO_FATAL_FAILURE(CheckFuzzyBoundaries(true)); }

TEST(CompiledRequestParityTest, TestDefaultFuzzyVectorBoundaryAllRisks) { ASSERT_NO_FATAL_FAILURE(CheckFuzzyBoundaries(false)); }

TEST(CompiledRequestParityTest, TestLsmcHistoricalVectorDefaultBudgetDiagnostics) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
    const PoolScope_ pool;
    constexpr size_t PATHS = 257;
    const Handle_<ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.03, 0.01));
    Vector_<Script::SimResults_> results;
    Vector_<Script::LsmcDiagnostics_> diagnostics(2);
    for (const bool compiled : {false, true}) {
        Script::MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        auto model = Dal::CreateModel<double>(data);
        const auto prepared = Script::PrepareScript(HistoricalExercise(), model.get(), {}, settings);
        auto& diagnostic = diagnostics[compiled];
        diagnostic.nPaths_ = PATHS;
        results.push_back(Script::MCLsmcSimulation(prepared, model.get(), PATHS, &diagnostic));
        ASSERT_EQ(diagnostic.trainingPaths_, PATHS);
        ASSERT_EQ(diagnostic.validationPaths_, 0);
        ASSERT_EQ(diagnostic.pricingPathsPerReplicate_, PATHS);
        ASSERT_EQ(diagnostic.events_.size(), 1);
    }
    ASSERT_NEAR(results[0].aggregated_, results[1].aggregated_, 1e-10);
    const auto& tree = diagnostics[0].events_[0];
    const auto& compiled = diagnostics[1].events_[0];
    ASSERT_EQ(tree.numCondTruePaths_, compiled.numCondTruePaths_);
    ASSERT_EQ(tree.effectiveRank_, compiled.effectiveRank_);
    ASSERT_EQ(tree.solver_, compiled.solver_);
    ASSERT_EQ(tree.coefficients_.size(), compiled.coefficients_.size());
    ASSERT_NEAR(tree.exerciseRate_, compiled.exerciseRate_, 1e-12);
    for (size_t i = 0; i < tree.coefficients_.size(); ++i)
        ASSERT_NEAR(tree.coefficients_[i], compiled.coefficients_[i], 1e-12);
}

TEST(CompiledRequestParityTest, TestLsmcHistoricalVectorRiskModesAndReplicates) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
    const PoolScope_ pool;
    const Handle_<ModelData_> data(new Dal::BSModelData_("", 100.0, 0.2, 0.03, 0.01));
    for (const String_ mode : {"Frozen", "RetrainedBump"}) {
        Vector_<Script::SimResults_> results;
        for (const bool compiled : {false, true}) {
            SCOPED_TRACE(::testing::Message() << mode << "; compiled=" << compiled);
            Script::MonteCarloSettings_ settings;
            settings.enableAad_ = true;
            settings.compiled_ = compiled;
            settings.smooth_ = 2.0;
            settings.lsmcBasisDegree_ = 2;
            settings.lsmcTrainingPaths_ = 128;
            settings.lsmcValidationPaths_ = 64;
            settings.lsmcRqmcReplicates_ = 2;
            settings.lsmcTrainingSeed_ = 3;
            settings.lsmcPricingSeed_ = 17;
            settings.lsmcPolicyRiskMode_ = mode;
            auto model = Dal::CreateModel<double>(data);
            const auto prepared = Script::PrepareScript(HistoricalExercise(), model.get(), {}, settings);
            results.push_back(Script::MCLsmcAadSimulation(prepared, data, 257));
            ASSERT_TRUE(std::isfinite(results.back().aggregated_));
            ASSERT_GE(results.back().aggregated_, 0.0);
        }
        ASSERT_EQ(results[0].names_, results[1].names_);
        ASSERT_EQ(results[0].risks_.size(), 5);
        ASSERT_NEAR(results[0].aggregated_, results[1].aggregated_, 1e-10);
        for (size_t i = 0; i < results[0].risks_.size(); ++i) {
            ASSERT_TRUE(std::isfinite(results[0].risks_[i]));
            ASSERT_TRUE(std::isfinite(results[1].risks_[i]));
            ASSERT_NEAR(results[0].risks_[i], results[1].risks_[i], 1e-10);
        }
    }
}
