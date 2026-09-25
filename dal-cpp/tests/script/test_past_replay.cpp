//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <array>
#include <atomic>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    Handle_<MarketFixingSnapshot_> History(double fixing = 80.0) {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), fixing}}}}));
    }

    ScriptProductData_ HistoricalProduct(const String_& past = "x = SCALE * FIX(EQ[DAL196_TEST], 2026-09-11)", const String_& future = "pay PAYS x") {
        return {"", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))}, {"2", past, future}};
    }

    Handle_<ModelData_> Model() { return Handle_<ModelData_>(new BSModelData_("", 100.0, 0.2, 0.03, 0.01)); }

    template <class F_> void CheckPreparedModeFailure(const F_& simulate) {
        try {
            simulate();
            FAIL() << "preparation accepted incompatible execution settings";
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find(
                          "UnsupportedExecutionMode: AAD mode, smoothing, or compiled/tree mode differs from preparation"),
                      std::string::npos);
        }
    }

    struct CompiledRootCase_ {
        ScriptProductData_ product_;
        double payoff_;
        double scaleRisk_;
        double rateRisk_;
    };

    void CheckCompiledRoot(const CompiledRootCase_& scenario, double fixing) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        const auto result = MCSimulation<AAD::Number_>(scenario.product_, Model(), 8193, {}, simulation, History(fixing));
        const auto price = MCSimulation<double>(scenario.product_, Model(), 8193, {}, simulation, History(fixing));
        ASSERT_NEAR(result.aggregated_ / 8193, scenario.payoff_, scenario.payoff_ * 1.0e-12);
        ASSERT_NEAR(price.aggregated_ / 8193, scenario.payoff_, scenario.payoff_ * 1.0e-12);
        ASSERT_NEAR(result["SCALE"], scenario.scaleRisk_, 1.0e-10);
        ASSERT_NEAR(result["rate"], scenario.rateRisk_, 1.0e-10);
        ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
        ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
    }

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

    struct SeedAudit_ {
        std::atomic<size_t> seeds_{0};
        std::atomic<size_t> paths_{0};
        bool fail_ = false;
    };

    struct SeedAuditedPrepared_ : PreparedScript_ {
        SeedAudit_* audit_;
        SeedAuditedPrepared_(PreparedScript_&& prepared, SeedAudit_* audit) : PreparedScript_(std::move(prepared)), audit_(audit) {}
        template <class E_> void InitializeHistoricalState(E_* evaluator) const {
            PreparedScript_::InitializeHistoricalState(evaluator);
            ++audit_->seeds_;
            REQUIRE(!audit_->fail_, "injected failure after recording historical seed");
        }
        template <class T_, class E_> void Evaluate(const AAD::Scenario_<T_>& path, E_& evaluator) const {
            PreparedScript_::Evaluate(path, evaluator);
            ++audit_->paths_;
        }
    };

    SimResults_ RebuildEveryPath(const PreparedScript_& prepared, const Handle_<ModelData_>& data, size_t paths) {
        auto metadata = CreateModel<double>(data);
        metadata->Allocate(prepared.TimeLine(), prepared.DefLine());
        auto random = CreateRNG("sobol", metadata->SimDim(), false);
        random->SkipTo(0);
        Vector_<> gauss(metadata->SimDim());
        SimResults_ result(Vector::Join(metadata->ParameterLabels(), prepared.ConstVarNames()));
        for (size_t i = 0; i < paths; ++i) {
            AAD::Activate(*AAD::Tape());
            TapeGuard_ guard(AAD::Tape());
            auto model = CreateModel<AAD::Number_>(data);
            model->Allocate(prepared.TimeLine(), prepared.DefLine());
            auto evaluator = prepared.BuildFuzzyEvaluator<AAD::Number_>(0, 0.01);
            for (auto* parameter : model->Parameters())
                AAD::PutOnTape(*parameter);
            for (auto& parameter : evaluator.ConstVarVals())
                AAD::PutOnTape(parameter);
            AAD::NewRecording(*AAD::Tape());
            model->Init(prepared.TimeLine(), prepared.DefLine());
            PastEvaluator_<AAD::Number_> past(Vector_<>(prepared.Product().VarNames().size(), 0.0), evaluator.ConstVarVals());
            past.SetObservations(&prepared.Plan());
            prepared.Product().Visit(past, true, false);
            evaluator.SetHistoricalSeed(past.VarVals());
            AAD::Scenario_<AAD::Number_> path;
            AAD::AllocatePath(prepared.DefLine(), path);
            AAD::InitializePath(path);
            random->FillNormal(&gauss);
            model->GeneratePath(gauss, &path);
            prepared.Evaluate(path, evaluator);
            AAD::Number_ payoff = evaluator.VarVals()[prepared.PayOffIdx()] + 0.0;
            AAD::Adjoint(payoff) = 1.0;
            AAD::PropagateToStart(*AAD::Tape());
            result.aggregated_ += AAD::Value(payoff);
            size_t j = 0;
            for (const auto* parameter : model->Parameters())
                result.risks_[j++] += AAD::Adjoint(*parameter) / paths;
            for (const auto& parameter : evaluator.ConstVarVals())
                result.risks_[j++] += AAD::Adjoint(parameter) / paths;
        }
        return result;
    }

    void CheckNonlinearHistoryRepricing(size_t paths, const String_& scaleText, const String_& strikeText, bool compiled) {
        const double t = 10.0 / DAYS_PER_YEAR;
        const double discount = exp(-0.03 * t);
        const ScriptProductData_ product(
            "", {Cell_("SCALE"), Cell_("SHIFT"), Cell_("K"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))},
            {scaleText, "0.5", strikeText, "x = SCALE * FIX(EQ[DAL196_TEST]) IF x > K:0.2 THEN x = x * x / 100 + SHIFT * x ELSE x = SHIFT * x END",
             "x = x + SCALE", "pay PAYS x"});
        const double scale = scaleText == "2" ? 2.0 : 3.0;
        const double seed = 80.0 * scale;
        const bool selected = scale == 3.0 || strikeText == "159.95";
        const double payoff = 0.5 * seed + scale + (selected ? seed * seed / 100.0 : 0.0);
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        const auto result = MCSimulation<AAD::Number_>(product, Model(), paths, ScriptValuationSettings_(), simulation, History());
        const auto price = MCSimulation<double>(product, Model(), paths, ScriptValuationSettings_(), simulation, History());
        ASSERT_NEAR(result.aggregated_ / paths, payoff * discount, payoff * discount * 1.0e-12);
        ASSERT_NEAR(price.aggregated_ / paths, payoff * discount, payoff * discount * 1.0e-12);
        ASSERT_NEAR(result["SCALE"], (41.0 + (selected ? 1.6 * seed : 0.0)) * discount, 1.0e-10);
        ASSERT_NEAR(result["SHIFT"], seed * discount, 1.0e-10);
        ASSERT_NEAR(result["K"], 0.0, 1.0e-10);
        ASSERT_NEAR(result["rate"], -t * payoff * discount, 1.0e-10);
        ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
        ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
        ASSERT_NEAR(result["div"], 0.0, 1.0e-10);
        ASSERT_EQ(result.names_.size(), 7u);
    }
} // namespace

TEST(ScriptPastReplayTest, TestParameterRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto result = MCSimulation<AAD::Number_>(HistoricalProduct(), Model(), 257, ScriptValuationSettings_(), {}, History());
    const double t = 10.0 / DAYS_PER_YEAR;
    const double discount = exp(-0.03 * t);
    ASSERT_NEAR(result.aggregated_ / 257, 160.0 * discount, 160.0e-12);
    ASSERT_NEAR(result.risks_.back(), 80.0 * discount, 1.0e-10);
    ASSERT_NEAR(result.risks_[0], 0.0, 1.0e-10);
    ASSERT_NEAR(result.risks_[1], 0.0, 1.0e-10);
    ASSERT_NEAR(result.risks_[2], -t * 160.0 * discount, 1.0e-10);
    ASSERT_EQ(result.risks_.size(), 5u);
}

TEST(ScriptPastReplayTest, TestVectorHistoryAndAadRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                                     {"2", "APPEND(v, SCALE * FIX(EQ[DAL196_TEST]))", "APPEND(v, SCALE * 10) pay PAYS AVERAGE(v)"});
    const double discount = exp(-0.03 * 10.0 / DAYS_PER_YEAR);
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        const auto result = MCSimulation<AAD::Number_>(product, Model(), 17, ScriptValuationSettings_(), simulation, History());
        ASSERT_NEAR(result.aggregated_ / 17, 90.0 * discount, 1.0e-10);
        ASSERT_NEAR(result["SCALE"], 45.0 * discount, 1.0e-10);
    }
}

TEST(ScriptPastReplayTest, TestCompiledParameterRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    MonteCarloSettings_ settings;
    settings.compiled_ = true;
    const auto result = MCSimulation<AAD::Number_>(HistoricalProduct(), Model(), 257, ScriptValuationSettings_(), settings, History());
    const double t = 10.0 / DAYS_PER_YEAR;
    const double discount = exp(-0.03 * t);
    ASSERT_NEAR(result.aggregated_ / 257, 160.0 * discount, 160.0e-12);
    ASSERT_NEAR(result["SCALE"], 80.0 * discount, 1.0e-10);
    ASSERT_NEAR(result["rate"], -t * 160.0 * discount, 1.0e-10);
    ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
    ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
    ASSERT_EQ(result.risks_.size(), 5u);
}

TEST(ScriptPastReplayTest, TestCompiledBatchLifetimeAndDirectRoots) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const double t = 10.0 / DAYS_PER_YEAR;
    const double discount = exp(-0.03 * t);
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        for (double fixing : {80.0, 90.0, 80.0}) {
            const std::array<CompiledRootCase_, 3> scenarios{
                {{HistoricalProduct(), 2.0 * fixing * discount, fixing * discount, -t * 2.0 * fixing * discount},
                 {HistoricalProduct("unused PAYS 0 x = SCALE * FIX(EQ[DAL196_TEST])", "unused = 7"), 2.0 * fixing, fixing, 0.0},
                 {HistoricalProduct("unused PAYS SCALE x = 17", "unused = 7"), 17.0, 0.0, 0.0}}};
            for (size_t kind = 0; kind < scenarios.size(); ++kind) {
                SCOPED_TRACE(::testing::Message() << "threads=" << threads << " fixing=" << fixing << " kind=" << kind);
                ASSERT_NO_FATAL_FAILURE(CheckCompiledRoot(scenarios[kind], fixing));
            }
        }
    }
}

TEST(ScriptPastReplayTest, TestCompiledHardPastDecisionAndPaysDiscard) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    String_ settled = "x = 0 ";
    for (size_t i = 0; i < 100; ++i)
        settled += "pay PAYS SCALE * FIX(EQ[DAL196_TEST]) ";
    for (const String_ comparison : {">", ">=", "="})
        for (double fixing : {79.95, 80.0, 80.05}) {
            const auto product = HistoricalProduct(settled + "IF FIX(EQ[DAL196_TEST]) " + comparison +
                                                   " 80:0.2 THEN x = SCALE * FIX(EQ[DAL196_TEST]) ELSE x = 3 * SCALE * FIX(EQ[DAL196_TEST]) END");
            const bool selected = comparison == ">" ? fixing > 80.0 : (comparison == ">=" ? fixing >= 80.0 : fixing == 80.0);
            const double derivative = (selected ? 1.0 : 3.0) * fixing * exp(-0.03 * 10.0 / DAYS_PER_YEAR);
            MonteCarloSettings_ simulation;
            simulation.compiled_ = true;
            const auto result = MCSimulation<AAD::Number_>(product, Model(), 257, {}, simulation, History(fixing));
            const auto price = MCSimulation<double>(product, Model(), 257, {}, simulation, History(fixing));
            ASSERT_NEAR(result.aggregated_ / 257, 2.0 * derivative, 2.0 * derivative * 1.0e-12);
            ASSERT_NEAR(price.aggregated_ / 257, 2.0 * derivative, 2.0 * derivative * 1.0e-12);
            ASSERT_NEAR(result["SCALE"], derivative, 1.0e-10);
        }
}

TEST(ScriptPastReplayTest, TestCompiledEveryPathRebuildAcrossBatches) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const auto product = HistoricalProduct("x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x + SCALE * FIX(EQ[DAL196_TEST], 2026-09-15)");
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    ScriptValuationSettings_ settings;
    Vector_<Handle_<ModelData_>> models{Model()};
    models.emplace_back(new DupireModelData_("", 100.0, 0.03, 0.01, Vector_<>{50.0, 100.0, 150.0}, Vector_<>{0.0, 0.5, 1.0}, Matrix_<>(3, 3, 0.2)));
    for (const auto& data : models) {
        auto model = CreateModel<double>(data);
        const auto prepared = PrepareScript(product, model.get(), settings, simulation, History());
        const auto oracle = RebuildEveryPath(prepared, data, 8193);
        for (size_t threads : {1, 2, 4}) {
            pool.pool_->Start(threads, true);
            for (size_t repeat = 0; repeat < 2; ++repeat) {
                const auto result = MCSimulation<AAD::Number_>(prepared, data, 8193, "sobol", false, true);
                ASSERT_NEAR(result.aggregated_ / 8193, oracle.aggregated_ / 8193, 1.0e-8);
                ASSERT_EQ(result.names_, oracle.names_);
                for (size_t j = 0; j < result.risks_.size(); ++j)
                    ASSERT_NEAR(result.risks_[j], oracle.risks_[j], 1.0e-8);
            }
        }
    }
}

TEST(ScriptPastReplayTest, TestPreparedCompiledModeCannotChange) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    auto model = CreateModel<double>(Model());
    for (bool compiled : {false, true}) {
        SCOPED_TRACE(compiled);
        simulation.compiled_ = compiled;
        const auto prepared = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
        ASSERT_NO_FATAL_FAILURE(CheckPreparedModeFailure(
            [&] { MCSimulation<AAD::Number_>(prepared, Model(), 257, "sobol", false, !compiled); }));
        const auto result = MCSimulation<AAD::Number_>(prepared, Model(), 257, "sobol", false, compiled);
        ASSERT_NEAR(result.aggregated_ / 257, 160.0 * exp(-0.03 * 10.0 / DAYS_PER_YEAR), 1.0e-10);
        ASSERT_THROW(prepared.BuildFuzzyEvaluator<double>(0, 0.2), ScriptError_);
        ASSERT_THROW(prepared.BuildEvalState<double>(0, 0.2), ScriptError_);
    }
}

TEST(ScriptPastReplayTest, TestPreparedDefaultCompiledModeResolvesToTree) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.compiled_ = true;
    auto model = CreateModel<double>(Model());
    const auto compiled = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
    ASSERT_NO_FATAL_FAILURE(CheckPreparedModeFailure([&] { MCSimulation<AAD::Number_>(compiled, Model(), 257); }));
    simulation.compiled_ = std::nullopt;
    const auto tree = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
    ASSERT_NO_FATAL_FAILURE(CheckPreparedModeFailure([&] { MCSimulation<AAD::Number_>(tree, Model(), 257, "sobol", false, true); }));
    const auto result = MCSimulation<AAD::Number_>(tree, Model(), 257);
    ASSERT_NEAR(result.aggregated_ / 257, 160.0 * exp(-0.03 * 10.0 / DAYS_PER_YEAR), 1.0e-10);
}

TEST(ScriptPastReplayTest, TestPreparedAadAndSmoothingMismatchDiagnostic) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    auto model = CreateModel<double>(Model());
    MonteCarloSettings_ simulation;
    const auto passive = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
    ASSERT_NO_FATAL_FAILURE(CheckPreparedModeFailure([&] { MCAADSimulation(passive, Model(), 257, "sobol", false, false, -1, 0.01); }));
    simulation.enableAad_ = true;
    const auto active = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
    ASSERT_NO_FATAL_FAILURE(CheckPreparedModeFailure(
        [&] { MCSimulation<AAD::Number_>(active, Model(), 257, "sobol", false, false, -1, 0.2); }));
}

TEST(ScriptPastReplayTest, TestExpiredPreparationSkipsAadModeMismatch) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 23));
    auto model = CreateModel<double>(Model());
    const auto expired = PrepareScript(HistoricalProduct(), model.get(), {}, {});
    const auto result = MCAADSimulation(expired, Model(), 257, "sobol", false, true, -1, 0.2);
    ASSERT_DOUBLE_EQ(result.aggregated_, 0.0);
    ASSERT_FALSE(result.risks_.empty());
    for (double risk : result.risks_)
        ASSERT_DOUBLE_EQ(risk, 0.0);
}

TEST(ScriptPastReplayTest, TestDirectSeedPayoff) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const auto product = HistoricalProduct("unused PAYS 0 x = SCALE * FIX(EQ[DAL196_TEST], 2026-09-11)", "unused = 7");
    const auto result = MCSimulation<AAD::Number_>(product, Model(), 8193, ScriptValuationSettings_(), {}, History());
    ASSERT_NEAR(result.aggregated_ / 8193, 160.0, 160.0e-12);
    ASSERT_NEAR(result["SCALE"], 80.0, 1.0e-10);
}

TEST(ScriptPastReplayTest, TestConstantPayoffAndEmptySuffix) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        const ScriptProductData_ data("", {Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 12))}, {"unused PAYS 0 x = 17", "unused = 7"});
        const auto result = MCSimulation<AAD::Number_>(data, Model(), 8193, ScriptValuationSettings_());
        ASSERT_DOUBLE_EQ(result.aggregated_ / 8193, 17.0);
        for (double risk : result.risks_)
            ASSERT_DOUBLE_EQ(risk, 0.0);
    }
    AAD::Activate(*AAD::Tape());
    TapeGuard_ guard(AAD::Tape());
    AAD::Number_ scale = 2.0;
    AAD::Number_ zero = 0.0;
    AAD::PutOnTape(scale);
    AAD::PutOnTape(zero);
    AAD::NewRecording(*AAD::Tape());
    const AAD::Number_ seed = scale * 80.0;
    const AAD::Number_ constant = 17.0;
    AAD::Mark(*AAD::Tape());
    for (size_t i = 0; i < 257; ++i) {
        AAD::RewindToMark(*AAD::Tape());
        AAD::Number_ root = AAD::PayoffRoot(seed, zero);
        AAD::Adjoint(root) = 1.0;
        AAD::PropagateToMark(*AAD::Tape());
        AAD::RewindToMark(*AAD::Tape());
        AAD::Number_ constantRoot = AAD::PayoffRoot(constant, zero);
        AAD::Adjoint(constantRoot) = 1.0;
        AAD::PropagateToMark(*AAD::Tape());
    }
    AAD::PropagateMarkToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::AdjointValue(scale) / 257, 80.0, 1.0e-10);
}

TEST(ScriptPastReplayTest, TestHardPastDecision) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    for (const String_ comparison : {">", ">=", "="})
        for (double fixing : {79.95, 80.0, 80.05}) {
            SCOPED_TRACE(::testing::Message() << comparison << " fixing=" << fixing);
            const auto product = HistoricalProduct("IF FIX(EQ[DAL196_TEST]) " + comparison +
                                                   " 80:0.2 THEN x = SCALE * FIX(EQ[DAL196_TEST]) ELSE x = 3 * SCALE * FIX(EQ[DAL196_TEST]) END");
            const bool selected = comparison == ">" ? fixing > 80.0 : (comparison == ">=" ? fixing >= 80.0 : fixing == 80.0);
            const double sensitivity = (selected ? 1.0 : 3.0) * fixing * exp(-0.03 * 10.0 / DAYS_PER_YEAR);
            const auto result = MCSimulation<AAD::Number_>(product, Model(), 257, ScriptValuationSettings_(), {}, History(fixing));
            const auto price = MCSimulation<double>(product, Model(), 257, ScriptValuationSettings_(), {}, History(fixing));
            ASSERT_NEAR(result.aggregated_ / 257, 2.0 * sensitivity, 2.0 * sensitivity * 1.0e-12);
            ASSERT_NEAR(price.aggregated_ / 257, 2.0 * sensitivity, 2.0 * sensitivity * 1.0e-12);
            ASSERT_NEAR(result["SCALE"], sensitivity, 1.0e-10);
        }
}

TEST(ScriptPastReplayTest, TestAadBatchLifetime) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const auto direct = HistoricalProduct("unused PAYS 0 x = SCALE * FIX(EQ[DAL196_TEST])", "unused = 7");
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        for (size_t paths : {1, 257, 8193})
            for (double fixing : {80.0, 90.0, 80.0}) {
                SCOPED_TRACE(::testing::Message() << "threads=" << threads << " paths=" << paths << " fixing=" << fixing);
                const auto result = MCSimulation<AAD::Number_>(HistoricalProduct(), Model(), paths, ScriptValuationSettings_(), {}, History(fixing));
                const double discount = exp(-0.03 * 10.0 / DAYS_PER_YEAR);
                ASSERT_NEAR(result.aggregated_ / paths, 2.0 * fixing * discount, 2.0 * fixing * 1.0e-12);
                ASSERT_NEAR(result["SCALE"], fixing * discount, 1.0e-10);
                ASSERT_NEAR(result["rate"], -10.0 / DAYS_PER_YEAR * 2.0 * fixing * discount, 1.0e-10);
                ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
                ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
                const auto seed = MCSimulation<AAD::Number_>(direct, Model(), paths, ScriptValuationSettings_(), {}, History(fixing));
                ASSERT_NEAR(seed.aggregated_ / paths, 2.0 * fixing, 2.0 * fixing * 1.0e-12);
                ASSERT_NEAR(seed["SCALE"], fixing, 1.0e-10);
            }
    }
}

TEST(ScriptPastReplayTest, TestEveryPathRebuildOracle) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const auto product = HistoricalProduct("x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x + SCALE * FIX(EQ[DAL196_TEST], 2026-09-15)");
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    ScriptValuationSettings_ settings;
    Vector_<Handle_<ModelData_>> models{Model()};
    models.emplace_back(new DupireModelData_("", 100.0, 0.03, 0.01, Vector_<>{50.0, 100.0, 150.0}, Vector_<>{0.0, 0.5, 1.0}, Matrix_<>(3, 3, 0.2)));
    for (const auto& data : models) {
        auto model = CreateModel<double>(data);
        const auto prepared = PrepareScript(product, model.get(), settings, simulation, History());
        const auto oracle = RebuildEveryPath(prepared, data, 257);
        for (size_t threads : {1, 2, 4}) {
            pool.pool_->Start(threads, true);
            const auto result = MCSimulation<AAD::Number_>(prepared, data, 257);
            ASSERT_NEAR(result.aggregated_ / 257, oracle.aggregated_ / 257, 1.0e-8);
            ASSERT_EQ(result.names_, oracle.names_);
            for (size_t j = 0; j < result.risks_.size(); ++j)
                ASSERT_NEAR(result.risks_[j], oracle.risks_[j], 1.0e-8);
        }
    }
}

TEST(ScriptPastReplayTest, TestFutureKnownFixingKeepsFuzzyRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product(
        "", {Cell_("SCALE"), Cell_("K"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
        {"2", "79.95", "x = SCALE * FIX(EQ[DAL196_TEST])", "IF FIX(EQ[DAL196_TEST], 2026-09-11) > K:0.2 THEN pay PAYS x ELSE pay PAYS 0 END"});
    const auto result = MCSimulation<AAD::Number_>(product, Model(), 257, ScriptValuationSettings_(), {}, History());
    const double discount = exp(-0.03 * 10.0 / DAYS_PER_YEAR);
    ASSERT_NEAR(result.aggregated_ / 257, 120.0 * discount, 120.0e-12);
    ASSERT_NEAR(result["SCALE"], 60.0 * discount, 1.0e-10);
    ASSERT_NEAR(result["K"], -800.0 * discount, 1.0e-10);
    auto model = CreateModel<double>(Model());
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    const auto prepared = PrepareScript(product, model.get(), {}, simulation, History());
    auto evaluator = prepared.BuildFuzzyEvaluator<double>(0, 0.01);
    AAD::Scenario_<> path;
    AAD::AllocatePath(prepared.DefLine(), path);
    AAD::InitializePath(path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.0), &path);
    prepared.Evaluate(path, evaluator);
    ASSERT_NEAR(result.aggregated_ / 257, evaluator.VarVals()[prepared.PayOffIdx()], 120.0e-12);
}

TEST(ScriptPastReplayTest, TestExpiredAadSkipsExecutionModeAndHistory) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const ScriptProductData_ product("", {Cell_(Date_(2026, 9, 11))}, {"pay PAYS FIX(EQ[DAL196_TEST])"});
    for (bool compiled : {false, true}) {
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        const auto result = MCSimulation<AAD::Number_>(product, Model(), 257, ScriptValuationSettings_(), simulation);
        ASSERT_DOUBLE_EQ(result.aggregated_, 0.0);
        for (double risk : result.risks_)
            ASSERT_DOUBLE_EQ(risk, 0.0);
    }
}

TEST(ScriptPastReplayTest, TestPreparedAadRejectsChangedSmoothing) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    auto model = CreateModel<double>(Model());
    const auto prepared = PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History());
    ASSERT_THROW(MCSimulation<AAD::Number_>(prepared, Model(), 257, "sobol", false, false, 0, 0.2), ScriptError_);
}

TEST(ScriptPastReplayTest, TestNonlinearHistoryAndParameterRepricing) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        for (size_t paths : {1, 257, 8193})
            for (const String_ scaleText : {"2", "3", "2"})
                for (const String_ strikeText : {"159.95", "160", "160.05"}) {
                    SCOPED_TRACE(::testing::Message()
                                 << "threads=" << threads << " paths=" << paths << " scale=" << scaleText << " strike=" << strikeText);
                    for (bool compiled : {false, true}) {
                        SCOPED_TRACE(compiled);
                        ASSERT_NO_FATAL_FAILURE(CheckNonlinearHistoryRepricing(paths, scaleText, strikeText, compiled));
                    }
                }
    }
}

TEST(ScriptPastReplayTest, TestTodayPolicyPreservesHistoricalAndModelRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 12)), Cell_(Date_(2026, 9, 22))},
                                     {"2", "x = SCALE * FIX(EQ[DAL196_TEST])", "x = x + SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"});
    const Handle_<MarketFixingSnapshot_> snapshot(
        new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}, {DateTime_(Date_(2026, 9, 12), 0.0), 90.0}}}}));
    const double t = 10.0 / DAYS_PER_YEAR;
    const double discount = exp(-0.03 * t);
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        for (bool historicalToday : {false, true})
            for (const String_ rng : {"sobol", "mrg32", "irn"})
                for (bool bridge : {false, true}) {
                    SCOPED_TRACE(::testing::Message()
                                 << "threads=" << threads << " historicalToday=" << historicalToday << " rng=" << rng << " bridge=" << bridge);
                    ScriptValuationSettings_ settings;
                    if (historicalToday)
                        settings.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
                    MonteCarloSettings_ simulation;
                    simulation.rsg_ = rng;
                    simulation.useBb_ = bridge;
                    const auto result = MCSimulation<AAD::Number_>(product, Model(), 257, settings, simulation, snapshot);
                    const double fixingSum = 80.0 + (historicalToday ? 90.0 : 100.0);
                    ASSERT_NEAR(result.aggregated_ / 257, 2.0 * fixingSum * discount, 2.0 * fixingSum * discount * 1.0e-12);
                    ASSERT_NEAR(result["SCALE"], fixingSum * discount, 1.0e-10);
                    ASSERT_NEAR(result["spot"], historicalToday ? 0.0 : 2.0 * discount, 1.0e-10);
                    ASSERT_NEAR(result["rate"], -t * 2.0 * fixingSum * discount, 1.0e-10);
                    ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
                    ASSERT_NEAR(result["div"], 0.0, 1.0e-10);
                    ASSERT_EQ(result.names_.size(), 5u);
                }
    }
}

TEST(ScriptPastReplayTest, TestEveryPathRebuildAcrossBatchBoundary) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    const ScriptProductData_ product(
        "", {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 15)), Cell_(Date_(2026, 9, 22))},
        {"2", "x = SCALE * SCALE * FIX(EQ[DAL196_TEST])", "x = x + SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x * x / 1000"});
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    ScriptValuationSettings_ settings;
    Vector_<Handle_<ModelData_>> models{Model()};
    models.emplace_back(new DupireModelData_("", 100.0, 0.03, 0.01, Vector_<>{50.0, 100.0, 150.0}, Vector_<>{0.0, 0.5, 1.0}, Matrix_<>(3, 3, 0.2)));
    for (const auto& data : models) {
        auto model = CreateModel<double>(data);
        const auto prepared = PrepareScript(product, model.get(), settings, simulation, History());
        const auto oracle = RebuildEveryPath(prepared, data, 8193);
        for (size_t threads : {1, 2, 4}) {
            SCOPED_TRACE(::testing::Message() << "model=" << data->Type() << " threads=" << threads);
            pool.pool_->Start(threads, true);
            const auto result = MCSimulation<AAD::Number_>(prepared, data, 8193);
            ASSERT_NEAR(result.aggregated_ / 8193, oracle.aggregated_ / 8193, 1.0e-8);
            ASSERT_EQ(result.names_, oracle.names_);
            for (size_t j = 0; j < result.risks_.size(); ++j) {
                SCOPED_TRACE(result.names_[j]);
                ASSERT_NEAR(result.risks_[j], oracle.risks_[j], 1.0e-8);
            }
        }
    }
}

TEST(ScriptPastReplayTest, TestHistoricalSeedFailureDrainsAndRebuildsEveryBatch) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    PoolRestore_ pool;
    for (size_t threads : {1, 2, 4}) {
        pool.pool_->Start(threads, true);
        auto model = CreateModel<double>(Model());
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        SeedAudit_ audit;
        const SeedAuditedPrepared_ prepared(PrepareScript(HistoricalProduct(), model.get(), {}, simulation, History()), &audit);
        const size_t batches = BatchPlan_(16385, pool.pool_->NumThreads()).BatchCount();
        audit.fail_ = true;
        ASSERT_THROW(MCAADSimulation(prepared, Model(), 16385, "sobol", false, false, 0, 0.01), Exception_);
        ASSERT_EQ(audit.seeds_.load(), batches);
        ASSERT_EQ(audit.paths_.load(), 0u);
        audit.fail_ = false;
        for (size_t repeat = 1; repeat <= 2; ++repeat) {
            SCOPED_TRACE(::testing::Message() << "threads=" << threads << " repeat=" << repeat);
            const auto result = MCAADSimulation(prepared, Model(), 16385, "sobol", false, false, 0, 0.01);
            const double discount = exp(-0.03 * 10.0 / DAYS_PER_YEAR);
            ASSERT_NEAR(result.aggregated_ / 16385, 160.0 * discount, 160.0 * discount * 1.0e-12);
            ASSERT_NEAR(result["SCALE"], 80.0 * discount, 1.0e-10);
            ASSERT_NEAR(result["rate"], -10.0 / DAYS_PER_YEAR * 160.0 * discount, 1.0e-10);
            ASSERT_EQ(audit.seeds_.load(), (repeat + 1) * batches);
            ASSERT_EQ(audit.paths_.load(), repeat * 16385);
        }
    }
}
