#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <future>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using Dal::BSModelData_;
using Dal::Cell_;
using Dal::CreateModel;
using Dal::Date_;
using Dal::Handle_;
using Dal::ModelData_;
using Dal::String_;
using Dal::ThreadPool_;
using Dal::Vector_;
namespace Script = Dal::Script;

namespace {
    struct ScopedChunkWorkers_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        ~ScopedChunkWorkers_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
        void Start(size_t threads) { pool_->Start(threads, true); }
    };

    Script::ScriptProductData_ ChunkProductData() {
        return {"chunk-order",
                {Cell_("SCALE"), Cell_("K"), Cell_(Date_(2026, 7, 1)), Cell_(Date_(2027, 1, 1))},
                {"1.2", "10.5", "a = SPOT() * SPOT()", "b = SCALE * SPOT() c = MAX(SPOT() - K, 0) payoff PAYS a + b + c"}};
    }

    void AssertExactChunkResult(const Script::SimResults_& actual, const Script::SimResults_& expected) {
        ASSERT_EQ(actual.names_, expected.names_);
        ASSERT_EQ(actual.results_, expected.results_);
        ASSERT_EQ(actual.risks_.size(), expected.risks_.size());
        for (size_t input = 0; input < actual.risks_.size(); ++input) {
            SCOPED_TRACE(actual.names_[input].c_str());
            ASSERT_EQ(actual.risks_[input], expected.risks_[input]);
        }
        ASSERT_EQ(actual.aggregated_, expected.aggregated_);
    }

    template <class F_> void AssertAcrossChunkWorkers(const F_& evaluate) {
        ScopedChunkWorkers_ workers;
        workers.Start(1);
        const auto reference = evaluate();
        for (const size_t threads : {1, 3, 8}) {
            workers.Start(threads);
            for (size_t repeat = 0; repeat < 3; ++repeat) {
                SCOPED_TRACE("threads=" + std::to_string(threads) + " repeat=" + std::to_string(repeat));
                const auto result = evaluate();
                AssertExactChunkResult(result, reference);
                if constexpr (std::is_base_of_v<Script::Detail::WeightedSimResults_, std::decay_t<decltype(result)>>)
                    ASSERT_EQ(result.componentSums_, reference.componentSums_);
            }
        }
    }

    void AssertAadChunkDeterminism(bool prepared) {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
        const auto data = ChunkProductData();
        const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
        Script::ScriptProduct_ raw(data.Dates(), data.EventTexts());
        const auto depth = static_cast<int>(raw.PreProcess(true, false));
        for (const bool compiled : {false, true}) {
            for (const String_ method : {String_("sobol"), String_("mrg32")}) {
                for (const size_t paths : {1000, 16385}) {
                    SCOPED_TRACE("method=" + std::string(method.c_str()) + " compiled=" + std::to_string(compiled) +
                                 " paths=" + std::to_string(paths));
                    std::optional<Script::PreparedScript_> product;
                    if (prepared) {
                        auto metadata = CreateModel<double>(model);
                        Script::ScriptValuationSettings_ valuation;
                        valuation.evaluationDate_ = Date_(2026, 1, 1);
                        Script::MonteCarloSettings_ simulation;
                        simulation.enableAad_ = true;
                        simulation.compiled_ = compiled;
                        product.emplace(Script::PrepareScript(data, metadata.get(), valuation, simulation));
                    }
                    AssertAcrossChunkWorkers([&] {
                        if (product)
                            return Script::MCAADSimulation(*product, model, paths, method, false, compiled, -1, 0.01);
                        return Script::MCAADSimulation(raw, model, paths, method, false, compiled, depth, 0.01);
                    });
                }
            }
        }
    }

    struct OwnedDoubleBatch_ {
        std::unique_ptr<double> value_ = std::make_unique<double>(0.0);
    };

    struct LegacyOwnedDoubleObjective_ : Script::Detail::ScalarSimulationObjective_ {
        using DoubleBatch_ = OwnedDoubleBatch_;
        [[nodiscard]] DoubleBatch_ MakeDoubleBatch() const { return {}; }
        [[nodiscard]] Script::Detail::ScalarPayoffCollector_ DoubleCollector(DoubleBatch_*) const { return {}; }
        void SetDoubleValue(DoubleBatch_* batch, double value) const { *batch->value_ = value; }
        void AggregateDouble(Result_* result, const Vector_<DoubleBatch_>& batches) const {
            for (const auto& batch : batches)
                result->aggregated_ += *batch.value_;
        }
        [[nodiscard]] Dal::AAD::ProfilingArrayStatistics_ DoubleArrays(const Vector_<DoubleBatch_>& batches) const {
            return Script::Detail::ProfileArrays(batches);
        }
    };

    struct CustomNumericAadBatch_ {
        double aggregated_ = 0.0;
        Vector_<> risks_;
        explicit CustomNumericAadBatch_(size_t inputs) : risks_(inputs, 0.0) {}
    };

    struct LegacyNumericAadObjective_ : Script::Detail::ScalarSimulationObjective_ {
        using AADBatch_ = CustomNumericAadBatch_;
        [[nodiscard]] AADBatch_ MakeAADBatch(size_t inputs) const { return AADBatch_(inputs); }
        [[nodiscard]] Script::Detail::ScalarPayoffCollector_ AADCollector(AADBatch_*) const { return {}; }
    };

    struct LegacyReducedDoubleObjective_ : Script::Detail::ScalarSimulationObjective_ {
        void AggregateDouble(Result_* result, const Vector_<DoubleBatch_>& batches) const {
            Script::Detail::ScalarSimulationObjective_::AggregateDouble(result, batches);
            result->aggregated_ *= 2.0;
        }
    };
} // namespace

TEST(ScriptTest, TestChunkRawAadIsExactlyIndependentOfWorkers) { AssertAadChunkDeterminism(false); }

TEST(ScriptTest, TestChunkPreparedAadIsExactlyIndependentOfWorkers) { AssertAadChunkDeterminism(true); }

TEST(ScriptTest, TestChunkDoubleIsExactlyIndependentOfWorkers) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    product.PreProcess(false, false);
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    for (const bool compiled : {false, true}) {
        for (const String_ method : {String_("sobol"), String_("mrg32")}) {
            SCOPED_TRACE("method=" + std::string(method.c_str()) + " compiled=" + std::to_string(compiled));
            AssertAcrossChunkWorkers([&] {
                auto instance = CreateModel<double>(model);
                return Script::MCDoubleSimulation(product, instance.get(), 1000, method, false, compiled);
            });
        }
    }
}

TEST(ScriptTest, TestChunkWeightedAadComponentsAreExactlyIndependentOfWorkers) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    product.PreProcess(true, false);
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    auto metadata = CreateModel<double>(model);
    Vector_<Script::RiskCoordinate_> inputs;
    for (size_t input = 0; input < metadata->Parameters().size(); ++input)
        inputs.push_back({"model:" + String_(std::to_string(input)), metadata->ParameterLabels()[input], "model", input,
                          *metadata->Parameters()[input], "model-coordinate", std::nullopt, 1.0});
    for (size_t input = 0; input < product.ConstVarNames().size(); ++input)
        inputs.push_back({"constant:" + String_(std::to_string(input)), product.ConstVarNames()[input], "constant", input,
                          product.ConstVarValues()[input], "script-number", std::nullopt, 1.0});
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "output:2"};
    request.weights_ = Vector_<>{1.25, -0.4, 0.0};
    const auto plan = Script::PlanWeightedRiskRequest(product, inputs, Date_(2026, 1, 1), request, true);
    const Script::Detail::WeightedSimulationObjective_ objective(plan);
    for (const bool compiled : {false, true}) {
        for (const String_ method : {String_("sobol"), String_("mrg32")}) {
            SCOPED_TRACE("method=" + std::string(method.c_str()) + " compiled=" + std::to_string(compiled));
            AssertAcrossChunkWorkers(
                [&] { return Script::MCAADSimulationWithObjective(product, model, 1000, method, false, compiled, 0, 0.01, objective); });
        }
    }
}

TEST(ScriptTest, TestChunkPlanCoversZeroTailAndSizeMaxWithoutOverflow) {
    const Script::Detail::ChunkPlan_ empty(0);
    ASSERT_EQ(empty.BatchSize(), 0u);
    ASSERT_EQ(empty.BatchCount(), 0u);
    ASSERT_THROW(static_cast<void>(empty.BatchAt(0)), Dal::Exception_);
    for (const size_t paths : {size_t{1}, size_t{64}, size_t{65}, size_t{16384}, size_t{16385}, static_cast<size_t>(std::numeric_limits<int>::max()),
                               std::numeric_limits<size_t>::max()}) {
        const Script::Detail::ChunkPlan_ plan(paths);
        ASSERT_GE(plan.BatchSize(), 64u);
        ASSERT_LE(plan.BatchSize(), 2048u);
        ASSERT_EQ(plan.BatchAt(0).firstPath_, 0u);
        const auto last = plan.BatchAt(plan.BatchCount() - 1);
        ASSERT_GT(last.pathCount_, 0u);
        ASSERT_LE(last.pathCount_, plan.BatchSize());
        ASSERT_EQ(last.firstPath_, paths - last.pathCount_);
        ASSERT_THROW(static_cast<void>(plan.BatchAt(plan.BatchCount())), Dal::Exception_);
    }
    ASSERT_EQ(Script::BatchPlan_(10, 4).BatchSize(), 3u);
    ASSERT_EQ(Script::BatchPlan_(16385, 1).BatchSize(), 8192u);
}

TEST(ScriptTest, TestChunkPartialCapacityBoundsDenseAndOversizedPayloads) {
    constexpr uint64_t BUDGET = 16 * 1024 * 1024;
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(0, 8), 0u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(3, 8), 3u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(10000, 8), 256u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(10000, 4102 * 8), 256u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(10000, BUDGET / 4), 4u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(10000, BUDGET + 1), 1u);
    ASSERT_EQ(Script::Detail::ChunkPartialCapacity(10000, std::numeric_limits<uint64_t>::max()), 1u);
}

TEST(ScriptTest, TestChunkRingReducesInPathOrderAndClearsReusedSlots) {
    Script::Detail::OrderedChunkResults_<double> partials(Vector_<>{0.0, 0.0});
    double sum = 0.0;
    const auto reduce = [&](double value) { sum += value; };
    *partials.Acquire(0) = 1.0e16;
    *partials.Acquire(1) = 1.0;
    partials.Publish(1, reduce);
    ASSERT_EQ(sum, 0.0);
    partials.Publish(0, reduce);
    ASSERT_EQ(sum, 1.0e16);
    ASSERT_EQ(*partials.Acquire(2), 0.0);
    ASSERT_EQ(*partials.Acquire(3), 0.0);
    *partials.Acquire(2) = -1.0e16;
    *partials.Acquire(3) = 3.0;
    partials.Publish(3, reduce);
    partials.Publish(2, reduce);
    ASSERT_EQ(sum, 3.0);
}

TEST(ScriptTest, TestChunkPersistentPrefixSecondChunkMatchesFreshRecording) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    const int depth = static_cast<int>(product.PreProcess(true, false));
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    auto metadata = CreateModel<double>(model);
    const auto names = Dal::Vector::Join(metadata->ParameterLabels(), product.ConstVarNames());
    for (const bool compiled : {false, true}) {
        for (const String_ method : {String_("sobol"), String_("mrg32")}) {
            std::optional<Script::ScriptCompiled_> program;
            if (compiled)
                program.emplace(product.Compile(true));
            Script::SimResults_ first(names), second(names), fresh(names);
            const Script::Detail::AADBatchSettings_ settings{
                method, false, depth, 0.01, 1000, metadata->Parameters().size(), product.ConstVarNames().size(), product.PayOffIdx()};
            Dal::AAD::ProfilingData_ statistics;
            std::optional<Dal::AAD::ProfilingScope_> profile;
            if (Dal::AAD::ProfilingAvailable())
                profile.emplace(&statistics);
            Script::Detail::WithAADWorker(product, model, settings, program, [&](const auto& evaluate) {
                evaluate(Script::PathBatch_{0, 64}, &first, Script::Detail::ScalarPayoffCollector_{}, nullptr);
                evaluate(Script::PathBatch_{64, 64}, &second, Script::Detail::ScalarPayoffCollector_{}, nullptr);
            });
            profile.reset();
            if (Dal::AAD::ProfilingAvailable()) {
                ASSERT_TRUE(statistics.complete_);
                ASSERT_EQ(statistics.Phase(Dal::AAD::AADProfilingPhase_::Value_::WORKER_INIT).calls_, 1u);
                ASSERT_EQ(statistics.Phase(Dal::AAD::AADProfilingPhase_::Value_::REVERSE_PREFIX).calls_, 2u);
            }
            Script::Detail::EvaluateAADBatch(product, model, settings, program, Script::PathBatch_{64, 64}, &fresh);
            AssertExactChunkResult(second, fresh);
        }
    }
}

TEST(ScriptTest, TestChunkWorkersReuseBoundedSlotsAndSubmitOnlyWorkers) {
    ScopedChunkWorkers_ workers;
    for (const size_t threads : {1, 3, 8}) {
        workers.Start(threads);
        const Script::Detail::ChunkPlan_ plan(4097);
        Script::Detail::OrderedChunkResults_<double> partials(Vector_<>{0.0, 0.0});
        std::atomic<size_t> initialized{0};
        double sum = 0.0;
        Script::Detail::RunChunkWorkers(
            workers.pool_, plan, &partials, nullptr,
            [&](const auto& run) {
                ++initialized;
                run([&](const Script::PathBatch_& batch, double* partial, std::unique_ptr<Dal::Random_>) {
                    *partial = static_cast<double>(batch.pathCount_);
                });
            },
            [&](double partial) { sum += partial; });
        ASSERT_EQ(sum, 4097.0);
        ASSERT_EQ(initialized.load(), std::min(workers.pool_->NumThreads(), plan.BatchCount()));
        ASSERT_EQ(partials.Partials().size(), 2u);
    }
}

TEST(ScriptTest, TestChunkIrnPreparedPrecisionAndBridgeAreExactlyIndependentOfWorkers) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    for (const String_ precision : {String_("Fast"), String_("Precise")}) {
        for (const bool bridge : {false, true}) {
            for (const bool compiled : {false, true}) {
                SCOPED_TRACE(std::string(precision.c_str()) + " bridge=" + std::to_string(bridge) + " compiled=" + std::to_string(compiled));
                auto metadata = CreateModel<double>(model);
                Script::ScriptValuationSettings_ valuation;
                valuation.evaluationDate_ = Date_(2026, 1, 1);
                Script::MonteCarloSettings_ simulation;
                simulation.enableAad_ = true;
                simulation.compiled_ = compiled;
                simulation.rsg_ = "irn";
                simulation.useBb_ = bridge;
                simulation.normalPrecision_ = precision;
                const auto product = Script::PrepareScript(data, metadata.get(), valuation, simulation);
                AssertAcrossChunkWorkers([&] { return Script::MCAADSimulation(product, model, 1000, "irn", bridge, compiled, -1, 0.01); });
                simulation.enableAad_ = false;
                const auto passive = Script::PrepareScript(data, metadata.get(), valuation, simulation);
                AssertAcrossChunkWorkers([&] {
                    auto instance = CreateModel<double>(model);
                    return Script::MCDoubleSimulation(passive, instance.get(), 1000, "irn", bridge, compiled);
                });
            }
        }
    }
}

TEST(ScriptTest, TestChunkWorkersCancelCapacityWaitersOnReductionFailure) {
    ScopedChunkWorkers_ workers;
    workers.Start(8);
    const Script::Detail::ChunkPlan_ plan(4097);
    Script::Detail::OrderedChunkResults_<double> partials(Vector_<>{0.0});
    std::atomic<size_t> live{0};
    std::atomic<size_t> started{0};
    const size_t expectedWorkers = std::min(workers.pool_->NumThreads(), plan.BatchCount());
    std::promise<void> allStarted;
    const auto ready = allStarted.get_future().share();
    struct LiveWorker_ {
        std::atomic<size_t>* live_;
        explicit LiveWorker_(std::atomic<size_t>* live) : live_(live) { ++*live_; }
        ~LiveWorker_() { --*live_; }
    };
    bool caught = false;
    try {
        Script::Detail::RunChunkWorkers(
            workers.pool_, plan, &partials, nullptr,
            [&](const auto& run) {
                const LiveWorker_ owner(&live);
                if (++started == expectedWorkers)
                    allStarted.set_value();
                ready.wait();
                run([](const Script::PathBatch_&, double* partial, std::unique_ptr<Dal::Random_>) { *partial = 1.0; });
            },
            [](double) { THROW("injected reduction failure"); });
    } catch (const Dal::Exception_& error) {
        caught = std::string(error.what()).find("injected reduction failure") != std::string::npos;
    }
    ASSERT_TRUE(caught);
    ASSERT_TRUE(partials.Cancelled());
    ASSERT_EQ(started.load(), expectedWorkers);
    ASSERT_EQ(live.load(), 0u);
    std::atomic<size_t> recovered{0};
    Script::Detail::OrderedChunkResults_<double> recovery(Vector_<>{0.0});
    Script::Detail::RunChunkWorkers(
        workers.pool_, plan, &recovery, nullptr,
        [&](const auto& run) {
            run([](const Script::PathBatch_& batch, double* partial, std::unique_ptr<Dal::Random_>) { *partial = batch.pathCount_; });
        },
        [&](double value) { recovered += static_cast<size_t>(value); });
    ASSERT_EQ(recovered.load(), 4097u);
}

TEST(ScriptTest, TestChunkZeroPathsSubmitNoWorkers) {
    ScopedChunkWorkers_ workers;
    workers.Start(3);
    Script::Detail::OrderedChunkResults_<double> partials(Vector_<>{});
    ASSERT_NO_THROW(Script::Detail::RunChunkWorkers(
        workers.pool_, Script::Detail::ChunkPlan_(0), &partials, nullptr, [](const auto&) { THROW("unexpected worker"); },
        [](double) { THROW("unexpected reduction"); }));
    ASSERT_TRUE(partials.Partials().empty());
}

TEST(ScriptTest, TestChunkLegacyDoubleObjectivesRetainOwnedBatchesAndCustomReduction) {
    ScopedChunkWorkers_ workers;
    workers.Start(3);
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    product.PreProcess(false, false);
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    for (const bool compiled : {false, true}) {
        for (const size_t paths : {0, 1000}) {
            const auto evaluate = [&](const auto& objective) {
                auto instance = CreateModel<double>(model);
                return Script::MCDoubleSimulationWithObjective(product, instance.get(), paths, "sobol", false, compiled, false, objective);
            };
            const auto owned = evaluate(LegacyOwnedDoubleObjective_{});
            const auto custom = evaluate(LegacyReducedDoubleObjective_{});
            ASSERT_EQ(custom.aggregated_, 2.0 * owned.aggregated_);
            const auto standard = evaluate(Script::Detail::ScalarSimulationObjective_{});
            ASSERT_NEAR(owned.aggregated_, standard.aggregated_, 1.0e-8);
            ASSERT_EQ(owned.names_, standard.names_);
        }
    }
}

TEST(ScriptTest, TestChunkWeightedDoubleComponentsAreExactlyIndependentOfWorkers) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    product.PreProcess(false, false);
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    Script::WeightedRiskRequest_ request;
    request.selection_.outputs_ = Vector_<String_>{"output:0", "output:1", "output:2"};
    request.weights_ = Vector_<>{1.25, -0.4, 0.0};
    const auto plan = Script::PlanWeightedRiskRequest(product, {}, Date_(2026, 1, 1), request, false);
    const Script::Detail::WeightedSimulationObjective_ objective(plan);
    for (const bool compiled : {false, true}) {
        for (const String_ method : {String_("sobol"), String_("mrg32")}) {
            AssertAcrossChunkWorkers([&] {
                auto instance = CreateModel<double>(model);
                return Script::MCDoubleSimulationWithObjective(product, instance.get(), 1000, method, false, compiled, false, objective);
            });
        }
    }
}

TEST(ScriptTest, TestChunkLegacyNumericAadObjectiveRetainsTypedBatches) {
    ScopedChunkWorkers_ workers;
    workers.Start(3);
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 1, 1));
    const auto data = ChunkProductData();
    Script::ScriptProduct_ product(data.Dates(), data.EventTexts());
    const auto depth = static_cast<int>(product.PreProcess(true, false));
    const Handle_<ModelData_> model(new BSModelData_("model", 10.0, 0.27, 0.04, 0.013));
    for (const bool compiled : {false, true}) {
        for (const size_t paths : {0, 1000}) {
            const auto actual =
                Script::MCAADSimulationWithObjective(product, model, paths, "sobol", false, compiled, depth, 0.01, LegacyNumericAadObjective_{});
            const auto expected = Script::MCAADSimulation(product, model, paths, "sobol", false, compiled, depth, 0.01);
            ASSERT_EQ(actual.names_, expected.names_);
            ASSERT_EQ(actual.results_, expected.results_);
            ASSERT_NEAR(actual.aggregated_, expected.aggregated_, 1.0e-8);
            ASSERT_EQ(actual.risks_.size(), expected.risks_.size());
            for (size_t input = 0; input < actual.risks_.size(); ++input)
                ASSERT_NEAR(actual.risks_[input], expected.risks_[input], 1.0e-10);
        }
    }
}

TEST(ScriptTest, TestChunkWholeWindowReducesAfterWorkersExitInPathOrder) {
    ScopedChunkWorkers_ workers;
    workers.Start(4);
    const Script::Detail::ChunkPlan_ plan(4097);
    Script::Detail::OrderedChunkResults_<double> partials(Vector_<>(plan.BatchCount(), 0.0));
    std::atomic<size_t> live{0};
    size_t liveAtReduction = 0;
    double sum = 0.0;
    struct LiveWorker_ {
        std::atomic<size_t>* live_;
        explicit LiveWorker_(std::atomic<size_t>* live) : live_(live) { ++*live_; }
        ~LiveWorker_() { --*live_; }
    };
    Script::Detail::RunChunkWorkers(
        workers.pool_, plan, &partials, nullptr,
        [&](const auto& run) {
            const LiveWorker_ owner(&live);
            run([&](const Script::PathBatch_& batch, double* partial, std::unique_ptr<Dal::Random_>) {
                const size_t index = batch.firstPath_ / plan.BatchSize();
                const Vector_<> values{1.0e16, 1.0, -1.0e16, 3.0};
                *partial = index < values.size() ? values[index] : 0.0;
            });
        },
        [&](double value) {
            liveAtReduction = std::max(liveAtReduction, live.load());
            sum += value;
        });
    ASSERT_EQ(live.load(), 0u);
    ASSERT_EQ(liveAtReduction, 0u);
    ASSERT_EQ(sum, 3.0);
}
